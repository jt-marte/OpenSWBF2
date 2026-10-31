#include "third_party/doctest/doctest.h"

#include <string.h>

#include "RedEngineFL/input/pc/pcSonyPad.h"

using namespace SonyPad;

namespace
{

// Hand-built input reports, laid out as the pads send them (report ID first).
// Sticks are 0..255 with 128 centred and 255 down on Y; the three button
// bytes are [0] hat + Square/Cross/Circle/Triangle, [1] L1 R1 L2 R2
// Share/Create Options L3 R3, [2] PS and touchpad.
struct State
{
    unsigned char ucLX, ucLY, ucRX, ucRY;
    unsigned char ucL2, ucR2;
    unsigned char aucButtons[3];
};

State Centred()
{
    State s;
    s.ucLX = s.ucLY = s.ucRX = s.ucRY = 128;
    s.ucL2 = s.ucR2 = 0;
    s.aucButtons[0] = 0x08; // hat released
    s.aucButtons[1] = 0;
    s.aucButtons[2] = 0;
    return s;
}

void PutCrc(unsigned char* puc)
{
    const unsigned int uiCrc = Crc32(kCrcSeedInput, puc, kBtReportSize - 4);
    puc[74] = (unsigned char)uiCrc;
    puc[75] = (unsigned char)(uiCrc >> 8);
    puc[76] = (unsigned char)(uiCrc >> 16);
    puc[77] = (unsigned char)(uiCrc >> 24);
}

// Sticks, three button bytes, L2, R2: DualShock 4 and simple mode.
void PutBasic(unsigned char* puc, const State& s)
{
    puc[0] = s.ucLX;
    puc[1] = s.ucLY;
    puc[2] = s.ucRX;
    puc[3] = s.ucRY;
    memcpy(puc + 4, s.aucButtons, 3);
    puc[7] = s.ucL2;
    puc[8] = s.ucR2;
}

// Sticks, L2, R2, counter, buttons: DualSense full reports.
void PutDualSense(unsigned char* puc, const State& s)
{
    puc[0] = s.ucLX;
    puc[1] = s.ucLY;
    puc[2] = s.ucRX;
    puc[3] = s.ucRY;
    puc[4] = s.ucL2;
    puc[5] = s.ucR2;
    puc[6] = 0x5A; // counter
    memcpy(puc + 7, s.aucButtons, 3);
}

enum Kind
{
    kDualSenseUsb,
    kDualSenseBt,
    kDualSenseSimple,
    kDs4Usb,
    kDs4Bt,
    kDs4Simple,
    kKindCount
};

const char* KindName(int iKind)
{
    static const char* const kNames[] = { "DualSense USB", "DualSense BT", "DualSense BT simple",
                                          "DS4 USB", "DS4 BT", "DS4 BT simple" };
    return kNames[iKind];
}

Model KindModel(int iKind)
{
    return iKind <= kDualSenseSimple ? kModelDualSense : kModelDualShock4;
}

// The report in pucOut (sized for the largest, 547 like a padded Bluetooth
// read); returns its length as ReadFile would.
unsigned int Build(int iKind, const State& s, unsigned char* pucOut)
{
    memset(pucOut, 0, 547);
    switch (iKind)
    {
    case kDualSenseUsb:
        pucOut[0] = 0x01;
        PutDualSense(pucOut + 1, s);
        return 64;
    case kDualSenseBt:
        pucOut[0] = 0x31;
        pucOut[1] = 0x30;
        PutDualSense(pucOut + 2, s);
        PutCrc(pucOut);
        return 78;
    case kDualSenseSimple:
        pucOut[0] = 0x01;
        PutBasic(pucOut + 1, s);
        return 78; // padded to the Bluetooth input size
    case kDs4Usb:
        pucOut[0] = 0x01;
        PutBasic(pucOut + 1, s);
        return 64;
    case kDs4Bt:
        pucOut[0] = 0x11;
        pucOut[1] = 0xC0;
        PutBasic(pucOut + 3, s);
        PutCrc(pucOut);
        return 547; // Windows pads DS4 Bluetooth reads to its largest report
    case kDs4Simple:
        pucOut[0] = 0x01;
        PutBasic(pucOut + 1, s);
        return 10;
    }
    return 0;
}

bool Parse(int iKind, const State& s, XInputPad::Pad& rPad)
{
    unsigned char aucReport[547];
    const unsigned int uiLength = Build(iKind, s, aucReport);
    memset(&rPad, 0xCD, sizeof(rPad));
    return ParseReport(KindModel(iKind), aucReport, uiLength, rPad);
}

}

TEST_CASE("sony pad models by vid and pid" * doctest::test_suite("unit"))
{
    CHECK(ModelFor(0x054C, 0x0CE6) == kModelDualSense);
    CHECK(ModelFor(0x054C, 0x0DF2) == kModelDualSense);
    CHECK(ModelFor(0x054C, 0x05C4) == kModelDualShock4);
    CHECK(ModelFor(0x054C, 0x09CC) == kModelDualShock4);
    CHECK(ModelFor(0x054C, 0x0BA0) == kModelNone); // DS4 wireless adapter
    CHECK(ModelFor(0x054C, 0x0268) == kModelNone); // DualShock 3
    CHECK(ModelFor(0x045E, 0x0CE6) == kModelNone);
    CHECK(ModelFor(0x045E, 0x028E) == kModelNone);
}

TEST_CASE("sony pad ids from usb and bluetooth hid paths" * doctest::test_suite("unit"))
{
    unsigned short usVid = 0;
    unsigned short usPid = 0;
    CHECK(ParseHidPathIds("\\\\?\\hid#vid_054c&pid_0ce6&mi_03#8&2f6b1e3c&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}",
                          usVid, usPid));
    CHECK(usVid == 0x054C);
    CHECK(usPid == 0x0CE6);

    usVid = usPid = 0;
    CHECK(ParseHidPathIds("\\\\?\\HID#{00001124-0000-1000-8000-00805f9b34fb}_VID&0002054c_PID&09cc#9&1a2b3c4d&0&0000#"
                          "{4d1e55b2-f16f-11cf-88cb-001111000030}",
                          usVid, usPid));
    CHECK(usVid == 0x054C);
    CHECK(usPid == 0x09CC);

    CHECK_FALSE(ParseHidPathIds(0, usVid, usPid));
    CHECK_FALSE(ParseHidPathIds("", usVid, usPid));
    CHECK_FALSE(ParseHidPathIds("\\\\?\\hid#vid_054c#nopid", usVid, usPid));
    CHECK_FALSE(ParseHidPathIds("\\\\?\\hid#vid_05zz&pid_0ce6", usVid, usPid));
    CHECK_FALSE(ParseHidPathIds("\\\\?\\hid#vid&0002&pid&0ce6", usVid, usPid));
}

TEST_CASE("sony pad centred report is a neutral pad on every model and bus" * doctest::test_suite("unit"))
{
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        const char* pcKind = KindName(iKind);
        INFO(pcKind);
        XInputPad::Pad pad;
        REQUIRE(Parse(iKind, Centred(), pad));
        CHECK(pad.wButtons == 0);
        CHECK(pad.bLeftTrigger == 0);
        CHECK(pad.bRightTrigger == 0);
        CHECK(pad.sThumbLX == 0);
        CHECK(pad.sThumbLY == 0);
        CHECK(pad.sThumbRX == 0);
        CHECK(pad.sThumbRY == 0);
    }
}

TEST_CASE("sony pad buttons map to the xbox buttons on every model and bus" * doctest::test_suite("unit"))
{
    static const struct
    {
        int iByte;
        unsigned char ucBit;
        unsigned short usPad;
    } kCases[] = {
        { 0, 0x20, XInputPad::kPadA },             // Cross
        { 0, 0x40, XInputPad::kPadB },             // Circle
        { 0, 0x10, XInputPad::kPadX },             // Square
        { 0, 0x80, XInputPad::kPadY },             // Triangle
        { 1, 0x01, XInputPad::kPadLeftShoulder },  // L1
        { 1, 0x02, XInputPad::kPadRightShoulder }, // R1
        { 1, 0x10, XInputPad::kPadBack },          // Share / Create
        { 1, 0x20, XInputPad::kPadStart },         // Options
        { 1, 0x40, XInputPad::kPadLeftThumb },     // L3
        { 1, 0x80, XInputPad::kPadRightThumb },    // R3
        { 1, 0x04, 0 },                            // L2 click: the analog value is used
        { 1, 0x08, 0 },                            // R2 click
        { 2, 0x01, 0 },                            // PS
        { 2, 0x02, 0 },                            // touchpad click
    };
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        for (int i = 0; i < (int)(sizeof(kCases) / sizeof(kCases[0])); ++i)
        {
            const char* pcKind = KindName(iKind);
            INFO(pcKind << " case " << i);
            State s = Centred();
            s.aucButtons[kCases[i].iByte] |= kCases[i].ucBit;
            XInputPad::Pad pad;
            REQUIRE(Parse(iKind, s, pad));
            CHECK(pad.wButtons == kCases[i].usPad);
        }
    }
}

TEST_CASE("sony pad all buttons at once" * doctest::test_suite("unit"))
{
    State s = Centred();
    s.aucButtons[0] = 0xF0 | 0x02; // face buttons + hat right
    s.aucButtons[1] = 0xFF;
    s.aucButtons[2] = 0xFF;
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        const char* pcKind = KindName(iKind);
        INFO(pcKind);
        XInputPad::Pad pad;
        REQUIRE(Parse(iKind, s, pad));
        CHECK(pad.wButtons == (XInputPad::kPadA | XInputPad::kPadB | XInputPad::kPadX | XInputPad::kPadY |
                               XInputPad::kPadLeftShoulder | XInputPad::kPadRightShoulder | XInputPad::kPadBack |
                               XInputPad::kPadStart | XInputPad::kPadLeftThumb | XInputPad::kPadRightThumb |
                               XInputPad::kPadDPadRight));
    }
}

TEST_CASE("sony pad dpad hat to xinput directions" * doctest::test_suite("unit"))
{
    static const unsigned short kExpected[16] = {
        XInputPad::kPadDPadUp,
        XInputPad::kPadDPadUp | XInputPad::kPadDPadRight,
        XInputPad::kPadDPadRight,
        XInputPad::kPadDPadDown | XInputPad::kPadDPadRight,
        XInputPad::kPadDPadDown,
        XInputPad::kPadDPadDown | XInputPad::kPadDPadLeft,
        XInputPad::kPadDPadLeft,
        XInputPad::kPadDPadUp | XInputPad::kPadDPadLeft,
        0, 0, 0, 0, 0, 0, 0, 0 // 8 is released, 9-15 never sent
    };
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        for (int iHat = 0; iHat < 16; ++iHat)
        {
            const char* pcKind = KindName(iKind);
            INFO(pcKind << " hat " << iHat);
            State s = Centred();
            s.aucButtons[0] = (unsigned char)iHat;
            XInputPad::Pad pad;
            REQUIRE(Parse(iKind, s, pad));
            CHECK(pad.wButtons == kExpected[iHat]);
        }
    }
}

TEST_CASE("sony pad sticks reach full range with y up positive" * doctest::test_suite("unit"))
{
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        const char* pcKind = KindName(iKind);
        INFO(pcKind);
        State s = Centred();
        s.ucLX = 255; // right
        s.ucLY = 0;   // up
        s.ucRX = 0;   // left
        s.ucRY = 255; // down
        XInputPad::Pad pad;
        REQUIRE(Parse(iKind, s, pad));
        CHECK(pad.sThumbLX == 32767);
        CHECK(pad.sThumbLY == 32767);
        CHECK(pad.sThumbRX == -32768);
        CHECK(pad.sThumbRY == -32768);

        s = Centred();
        s.ucLX = 64;
        s.ucRY = 192;
        REQUIRE(Parse(iKind, s, pad));
        CHECK(pad.sThumbLX == -16384);
        CHECK(pad.sThumbLY == 0);
        CHECK(pad.sThumbRY < -16000);
        CHECK(pad.sThumbRY > -16700);
    }
}

TEST_CASE("sony pad stick value endpoints and centre" * doctest::test_suite("unit"))
{
    CHECK(StickValue(128, false) == 0);
    CHECK(StickValue(128, true) == 0);
    CHECK(StickValue(0, false) == -32768);
    CHECK(StickValue(255, false) == 32767);
    CHECK(StickValue(0, true) == 32767);
    CHECK(StickValue(255, true) == -32768);
    // monotonic, no wrap-around
    for (int i = 1; i < 256; ++i)
    {
        CHECK(StickValue((unsigned char)i, false) > StickValue((unsigned char)(i - 1), false));
        CHECK(StickValue((unsigned char)i, true) < StickValue((unsigned char)(i - 1), true));
    }
}

TEST_CASE("sony pad triggers are the analog values" * doctest::test_suite("unit"))
{
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        const char* pcKind = KindName(iKind);
        INFO(pcKind);
        State s = Centred();
        s.ucL2 = 200;
        s.ucR2 = 17;
        XInputPad::Pad pad;
        REQUIRE(Parse(iKind, s, pad));
        CHECK(pad.bLeftTrigger == 200);
        CHECK(pad.bRightTrigger == 17);
        CHECK(pad.wButtons == 0);
    }
}

TEST_CASE("sony pad reports feed the xinput mapping" * doctest::test_suite("unit"))
{
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        const char* pcKind = KindName(iKind);
        INFO(pcKind);
        State s = Centred();
        s.aucButtons[0] = 0x20 | 0x00; // Cross + D-pad up
        s.aucButtons[1] = 0x02;        // R1
        s.ucR2 = 255;
        s.ucLY = 0; // full forward
        XInputPad::Pad pad;
        REQUIRE(Parse(iKind, s, pad));
        XInputPad::Mapped m;
        XInputPad::Map(pad, XInputPad::kMoveDeadZone, XInputPad::kLookDeadZone, m);
        CHECK(m.uiButtons == (((unsigned __int64)1 << XInputPad::kBitA) | ((unsigned __int64)1 << XInputPad::kBitRB) |
                              ((unsigned __int64)1 << XInputPad::kBitDPadUp)));
        CHECK(m.afAxis[6] == doctest::Approx(1.0f));
        CHECK(m.afAxis[5] == 0.0f);
        CHECK(m.afAxis[1] == doctest::Approx(-1.0f)); // LY: down positive
        CHECK(m.afAxis[0] == 0.0f);
    }
}

TEST_CASE("sony pad short reports are rejected" * doctest::test_suite("unit"))
{
    for (int iKind = 0; iKind < kKindCount; ++iKind)
    {
        const char* pcKind = KindName(iKind);
        INFO(pcKind);
        unsigned char aucReport[547];
        Build(iKind, Centred(), aucReport);
        XInputPad::Pad pad;
        for (unsigned int uiLength = 0; uiLength < kSimpleInputSize; ++uiLength)
        {
            CHECK_FALSE(ParseReport(KindModel(iKind), aucReport, uiLength, pad));
        }
    }
    // Bluetooth full reports need all 78 bytes (the CRC is at the end).
    unsigned char aucReport[547];
    XInputPad::Pad pad;
    Build(kDualSenseBt, Centred(), aucReport);
    CHECK_FALSE(ParseReport(kModelDualSense, aucReport, 77, pad));
    Build(kDs4Bt, Centred(), aucReport);
    CHECK_FALSE(ParseReport(kModelDualShock4, aucReport, 77, pad));
    CHECK_FALSE(ParseReport(kModelDualSense, 0, 64, pad));
}

TEST_CASE("sony pad bluetooth reports with a bad crc are rejected" * doctest::test_suite("unit"))
{
    unsigned char aucReport[547];
    XInputPad::Pad pad;
    const int aiKinds[] = { kDualSenseBt, kDs4Bt };
    for (int k = 0; k < 2; ++k)
    {
        const char* pcKind = KindName(aiKinds[k]);
        INFO(pcKind);
        const unsigned int uiLength = Build(aiKinds[k], Centred(), aucReport);
        REQUIRE(ParseReport(KindModel(aiKinds[k]), aucReport, uiLength, pad));
        // any flipped bit in the covered bytes or the CRC itself
        for (int i = 1; i < 78; ++i)
        {
            Build(aiKinds[k], Centred(), aucReport);
            aucReport[i] ^= 0x04;
            CHECK_FALSE(ParseReport(KindModel(aiKinds[k]), aucReport, uiLength, pad));
        }
    }
    // DS4 0x11 without the "holds controller state" flag, even with a good CRC
    Build(kDs4Bt, Centred(), aucReport);
    aucReport[1] = 0x40;
    PutCrc(aucReport);
    CHECK_FALSE(ParseReport(kModelDualShock4, aucReport, 78, pad));
}

TEST_CASE("sony pad other report ids and models are rejected" * doctest::test_suite("unit"))
{
    unsigned char aucReport[547];
    XInputPad::Pad pad;
    // the other model's Bluetooth report, CRC and all
    unsigned int uiLength = Build(kDualSenseBt, Centred(), aucReport);
    CHECK_FALSE(ParseReport(kModelDualShock4, aucReport, uiLength, pad));
    uiLength = Build(kDs4Bt, Centred(), aucReport);
    CHECK_FALSE(ParseReport(kModelDualSense, aucReport, uiLength, pad));
    // not a Sony pad
    uiLength = Build(kDs4Usb, Centred(), aucReport);
    CHECK_FALSE(ParseReport(kModelNone, aucReport, uiLength, pad));
    // feature / audio / unknown report IDs
    static const unsigned char kIds[] = { 0x00, 0x02, 0x05, 0x09, 0x12, 0x19, 0x20, 0x32, 0xFF };
    for (int i = 0; i < (int)sizeof(kIds); ++i)
    {
        memset(aucReport, 0, sizeof(aucReport));
        aucReport[0] = kIds[i];
        CHECK_FALSE(ParseReport(kModelDualSense, aucReport, 78, pad));
        CHECK_FALSE(ParseReport(kModelDualShock4, aucReport, 547, pad));
    }
}

TEST_CASE("sony pad a rejected report leaves the last state" * doctest::test_suite("unit"))
{
    State s = Centred();
    s.aucButtons[0] = 0x20;
    unsigned char aucReport[547];
    unsigned int uiLength = Build(kDualSenseUsb, s, aucReport);
    XInputPad::Pad pad;
    REQUIRE(ParseReport(kModelDualSense, aucReport, uiLength, pad));
    const XInputPad::Pad kBefore = pad;
    uiLength = Build(kDualSenseBt, Centred(), aucReport);
    aucReport[10] ^= 0xFF;
    CHECK_FALSE(ParseReport(kModelDualSense, aucReport, uiLength, pad));
    CHECK(memcmp(&pad, &kBefore, sizeof(pad)) == 0);
}

TEST_CASE("sony pad garbage reports never parse as bluetooth full reports" * doctest::test_suite("unit"))
{
    unsigned int uiSeed = 0x5EED;
    unsigned char aucReport[547];
    XInputPad::Pad pad;
    int iParsed = 0;
    for (int iRound = 0; iRound < 20000; ++iRound)
    {
        for (int i = 0; i < (int)sizeof(aucReport); ++i)
        {
            uiSeed = uiSeed * 1664525u + 1013904223u;
            aucReport[i] = (unsigned char)(uiSeed >> 24);
        }
        uiSeed = uiSeed * 1664525u + 1013904223u;
        const unsigned int uiLength = (uiSeed >> 8) % (sizeof(aucReport) + 1);
        aucReport[0] = (iRound & 1) ? 0x31 : 0x11;
        aucReport[1] |= 0x80;
        const Model eModel = (iRound & 1) ? kModelDualSense : kModelDualShock4;
        if (ParseReport(eModel, aucReport, uiLength, pad))
        {
            ++iParsed;
        }
    }
    CHECK(iParsed == 0);
}

TEST_CASE("sony pad crc32 matches zlib" * doctest::test_suite("unit"))
{
    // CRC-32 of "123456789" is 0xCBF43926; the seed is just the first byte.
    const unsigned char* puc = (const unsigned char*)"23456789";
    CHECK(Crc32('1', puc, 8) == 0xCBF43926);
    CHECK(Crc32('a', puc, 0) == 0xE8B7BE43); // CRC-32 of "a"
}

TEST_CASE("sony pad firmware version and rumble mode" * doctest::test_suite("unit"))
{
    unsigned char aucFeature[64];
    memset(aucFeature, 0, sizeof(aucFeature));
    aucFeature[0] = 0x20;
    aucFeature[44] = 0x24;
    aucFeature[45] = 0x02;
    unsigned short usVersion = 0;
    CHECK(ParseFirmwareVersion(aucFeature, 64, usVersion));
    CHECK(usVersion == 0x0224);
    CHECK_FALSE(ParseFirmwareVersion(aucFeature, 45, usVersion));
    aucFeature[0] = 0x09;
    CHECK_FALSE(ParseFirmwareVersion(aucFeature, 64, usVersion));
    CHECK_FALSE(ParseFirmwareVersion(0, 64, usVersion));

    CHECK_FALSE(UseEnhancedRumble(kPidDualSense, true, 0x0223));
    CHECK(UseEnhancedRumble(kPidDualSense, true, 0x0224));
    CHECK(UseEnhancedRumble(kPidDualSense, true, 0x0410));
    CHECK(UseEnhancedRumble(kPidDualSense, false, 0));
    CHECK(UseEnhancedRumble(kPidDualSense, true, 0));
    CHECK(UseEnhancedRumble(kPidDualSenseEdge, true, 0x0100));
}

TEST_CASE("sony pad rumble reports for every model and bus" * doctest::test_suite("unit"))
{
    unsigned char aucOut[547];
    const unsigned short usHeavy = 0xC000;
    const unsigned short usLight = 0x4000;

    // DualShock 4 USB: 0x05, motors flag, right (light) then left (heavy) motor.
    CHECK(BuildRumbleReport(kModelDualShock4, false, true, 0, usHeavy, usLight, aucOut, 32) == 32);
    CHECK(aucOut[0] == 0x05);
    CHECK(aucOut[1] == 0x01);
    CHECK(aucOut[2] == 0x00);
    CHECK(aucOut[4] == 0x40);
    CHECK(aucOut[5] == 0xC0);
    CHECK(aucOut[6] == 0); // lightbar left alone
    CHECK(aucOut[7] == 0);
    CHECK(aucOut[8] == 0);

    // DualShock 4 Bluetooth: 0x11, HID + CRC flags, padded write, CRC.
    memset(aucOut, 0xEE, sizeof(aucOut));
    CHECK(BuildRumbleReport(kModelDualShock4, true, true, 0, usHeavy, usLight, aucOut, 547) == 78);
    CHECK(aucOut[0] == 0x11);
    CHECK(aucOut[1] == 0xC4);
    CHECK(aucOut[3] == 0x01);
    CHECK(aucOut[6] == 0x40);
    CHECK(aucOut[7] == 0xC0);
    CHECK(aucOut[8] == 0);
    const unsigned int uiDs4Crc = Crc32(kCrcSeedOutput, aucOut, 74);
    CHECK(aucOut[74] == (unsigned char)uiDs4Crc);
    CHECK(aucOut[77] == (unsigned char)(uiDs4Crc >> 24));
    bool bPadded = true;
    for (int i = 78; i < 547; ++i)
    {
        bPadded = bPadded && aucOut[i] == 0;
    }
    CHECK(bPadded);

    // DualSense USB, improved rumble: 0x02, rumble over audio haptics, flag 2 bit 2.
    CHECK(BuildRumbleReport(kModelDualSense, false, true, 0, usHeavy, usLight, aucOut, 48) == 48);
    CHECK(aucOut[0] == 0x02);
    CHECK(aucOut[1] == 0x02);
    CHECK(aucOut[2] == 0x00);
    CHECK(aucOut[3] == 0x40);
    CHECK(aucOut[4] == 0xC0);
    CHECK(aucOut[39] == 0x04);
    CHECK(aucOut[40] == 0); // lightbar setup untouched
    CHECK(aucOut[45] == 0);

    // DualSense USB, compatible rumble on old firmware: half strength.
    CHECK(BuildRumbleReport(kModelDualSense, false, false, 0, usHeavy, usLight, aucOut, 48) == 48);
    CHECK(aucOut[1] == 0x03);
    CHECK(aucOut[3] == 0x20);
    CHECK(aucOut[4] == 0x60);
    CHECK(aucOut[39] == 0x00);

    // DualSense Bluetooth: 0x31, sequence in the high nibble, tag 0x10, CRC.
    CHECK(BuildRumbleReport(kModelDualSense, true, true, 5, usHeavy, usLight, aucOut, 78) == 78);
    CHECK(aucOut[0] == 0x31);
    CHECK(aucOut[1] == 0x50);
    CHECK(aucOut[2] == 0x10);
    CHECK(aucOut[3] == 0x02);
    CHECK(aucOut[5] == 0x40);
    CHECK(aucOut[6] == 0xC0);
    CHECK(aucOut[41] == 0x04);
    const unsigned int uiDsCrc = Crc32(kCrcSeedOutput, aucOut, 74);
    CHECK(aucOut[74] == (unsigned char)uiDsCrc);
    CHECK(aucOut[75] == (unsigned char)(uiDsCrc >> 8));
    CHECK(aucOut[76] == (unsigned char)(uiDsCrc >> 16));
    CHECK(aucOut[77] == (unsigned char)(uiDsCrc >> 24));
    CHECK(BuildRumbleReport(kModelDualSense, true, true, 0x1F, 0, 0, aucOut, 78) == 78);
    CHECK(aucOut[1] == 0xF0);
}

TEST_CASE("sony pad rumble off still selects rumble" * doctest::test_suite("unit"))
{
    unsigned char aucOut[78];
    CHECK(BuildRumbleReport(kModelDualSense, false, true, 0, 0, 0, aucOut, 48) == 48);
    CHECK(aucOut[1] == 0x02);
    CHECK(aucOut[3] == 0);
    CHECK(aucOut[4] == 0);
    CHECK(BuildRumbleReport(kModelDualShock4, false, true, 0, 0, 0, aucOut, 32) == 32);
    CHECK(aucOut[1] == 0x01);
    CHECK(aucOut[4] == 0);
    CHECK(aucOut[5] == 0);
}

TEST_CASE("sony pad rumble report needs room" * doctest::test_suite("unit"))
{
    unsigned char aucOut[547];
    CHECK(BuildRumbleReport(kModelDualShock4, false, true, 0, 1, 1, aucOut, 31) == 0);
    CHECK(BuildRumbleReport(kModelDualShock4, true, true, 0, 1, 1, aucOut, 77) == 0);
    CHECK(BuildRumbleReport(kModelDualSense, false, true, 0, 1, 1, aucOut, 47) == 0);
    CHECK(BuildRumbleReport(kModelDualSense, true, true, 0, 1, 1, aucOut, 64) == 0);
    CHECK(BuildRumbleReport(kModelNone, false, true, 0, 1, 1, aucOut, 547) == 0);
    CHECK(BuildRumbleReport(kModelDualSense, false, true, 0, 1, 1, 0, 547) == 0);
}

TEST_CASE("sony pad steam input ignore list" * doctest::test_suite("unit"))
{
    const char* pcList = "0x045E/0x028E,0x054C/0x0CE6, 0x054c/0x09cc";
    CHECK(IdListHas(pcList, 0x054C, 0x0CE6));
    CHECK(IdListHas(pcList, 0x054C, 0x09CC));
    CHECK(IdListHas(pcList, 0x045E, 0x028E));
    CHECK_FALSE(IdListHas(pcList, 0x054C, 0x05C4));
    CHECK_FALSE(IdListHas(pcList, 0x0CE6, 0x054C));
    CHECK_FALSE(IdListHas("", 0x054C, 0x0CE6));
    CHECK_FALSE(IdListHas(0, 0x054C, 0x0CE6));
    CHECK_FALSE(IdListHas("0x054C", 0x054C, 0x0CE6));
    CHECK_FALSE(IdListHas("garbage 0x 0xzz/0x", 0x054C, 0x0CE6));
}
