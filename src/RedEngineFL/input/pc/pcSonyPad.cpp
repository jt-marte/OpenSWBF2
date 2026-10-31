#include "pcSonyPad.h"

#include <stdlib.h>
#include <string.h>

namespace SonyPad
{

bool gDisabled;

Model ModelFor(unsigned short usVid, unsigned short usPid)
{
    if (usVid != kVidSony)
    {
        return kModelNone;
    }
    switch (usPid)
    {
    case kPidDualSense:
    case kPidDualSenseEdge:
        return kModelDualSense;
    case kPidDualShock4:
    case kPidDualShock4v2:
        return kModelDualShock4;
    }
    return kModelNone;
}

const char* ModelName(Model eModel)
{
    switch (eModel)
    {
    case kModelDualShock4:
        return "DualShock 4";
    case kModelDualSense:
        return "DualSense";
    default:
        return "none";
    }
}

static const char* FindNoCase(const char* pcText, const char* pcWord)
{
    const size_t uLen = strlen(pcWord);
    for (const char* pc = pcText; *pc; ++pc)
    {
        if (_strnicmp(pc, pcWord, uLen) == 0)
        {
            return pc + uLen;
        }
    }
    return 0;
}

static bool ParseHex(const char* pc, int iDigits, unsigned int& uiOut)
{
    unsigned int uiValue = 0;
    for (int i = 0; i < iDigits; ++i)
    {
        const char c = pc[i];
        unsigned int uiDigit;
        if (c >= '0' && c <= '9')
        {
            uiDigit = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            uiDigit = c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'F')
        {
            uiDigit = c - 'A' + 10;
        }
        else
        {
            return false;
        }
        uiValue = uiValue * 16 + uiDigit;
    }
    uiOut = uiValue;
    return true;
}

bool ParseHidPathIds(const char* pcPath, unsigned short& usVid, unsigned short& usPid)
{
    if (!pcPath)
    {
        return false;
    }
    unsigned int uiVid = 0;
    unsigned int uiPid = 0;
    const char* pc;
    if ((pc = FindNoCase(pcPath, "VID_")) != 0)
    {
        if (!ParseHex(pc, 4, uiVid))
        {
            return false;
        }
    }
    else if ((pc = FindNoCase(pcPath, "VID&")) != 0)
    {
        if (!ParseHex(pc, 8, uiVid))
        {
            return false;
        }
        uiVid &= 0xFFFF;
    }
    else
    {
        return false;
    }
    if (!(pc = FindNoCase(pcPath, "PID_")) && !(pc = FindNoCase(pcPath, "PID&")))
    {
        return false;
    }
    if (!ParseHex(pc, 4, uiPid))
    {
        return false;
    }
    usVid = (unsigned short)uiVid;
    usPid = (unsigned short)uiPid;
    return true;
}

short StickValue(unsigned char ucRaw, bool bInvert)
{
    int iValue = (int)ucRaw - 128;
    int iPos = 127;
    int iNeg = 128;
    if (bInvert)
    {
        iValue = -iValue;
        iPos = 128;
        iNeg = 127;
    }
    return (short)(iValue >= 0 ? iValue * 32767 / iPos : iValue * 32768 / iNeg);
}

unsigned int Crc32(unsigned char ucSeed, const unsigned char* pucData, unsigned int uiLength)
{
    unsigned int uiCrc = 0xFFFFFFFF;
    for (unsigned int i = 0; i <= uiLength; ++i)
    {
        uiCrc ^= i == 0 ? ucSeed : pucData[i - 1];
        for (int iBit = 0; iBit < 8; ++iBit)
        {
            uiCrc = (uiCrc >> 1) ^ (0xEDB88320 & (0u - (uiCrc & 1)));
        }
    }
    return ~uiCrc;
}

static unsigned int ReadLe32(const unsigned char* puc)
{
    return (unsigned int)puc[0] | ((unsigned int)puc[1] << 8) | ((unsigned int)puc[2] << 16) |
           ((unsigned int)puc[3] << 24);
}

static void WriteLe32(unsigned char* puc, unsigned int uiValue)
{
    puc[0] = (unsigned char)uiValue;
    puc[1] = (unsigned char)(uiValue >> 8);
    puc[2] = (unsigned char)(uiValue >> 16);
    puc[3] = (unsigned char)(uiValue >> 24);
}

// A 78-byte Bluetooth report's trailing CRC.
static bool BtCrcValid(const unsigned char* pucData)
{
    return Crc32(kCrcSeedInput, pucData, kBtReportSize - 4) == ReadLe32(pucData + kBtReportSize - 4);
}

// Both models share the button bytes: [0] D-pad hat (low nibble) and the face
// buttons, [1] shoulders, digital triggers, Share/Create, Options and stick
// clicks, [2] PS and touchpad (unused). Face buttons go by position, as on an
// Xbox pad: Cross A, Circle B, Square X, Triangle Y.
static unsigned short MapButtons(const unsigned char* pucButtons)
{
    static const unsigned short kHat[8] = {
        XInputPad::kPadDPadUp,
        XInputPad::kPadDPadUp | XInputPad::kPadDPadRight,
        XInputPad::kPadDPadRight,
        XInputPad::kPadDPadDown | XInputPad::kPadDPadRight,
        XInputPad::kPadDPadDown,
        XInputPad::kPadDPadDown | XInputPad::kPadDPadLeft,
        XInputPad::kPadDPadLeft,
        XInputPad::kPadDPadUp | XInputPad::kPadDPadLeft,
    };
    unsigned short usButtons = 0;
    const unsigned char ucHat = pucButtons[0] & 0x0F;
    if (ucHat < 8)
    {
        usButtons |= kHat[ucHat];
    }
    if (pucButtons[0] & 0x10)
    {
        usButtons |= XInputPad::kPadX;
    }
    if (pucButtons[0] & 0x20)
    {
        usButtons |= XInputPad::kPadA;
    }
    if (pucButtons[0] & 0x40)
    {
        usButtons |= XInputPad::kPadB;
    }
    if (pucButtons[0] & 0x80)
    {
        usButtons |= XInputPad::kPadY;
    }
    if (pucButtons[1] & 0x01)
    {
        usButtons |= XInputPad::kPadLeftShoulder;
    }
    if (pucButtons[1] & 0x02)
    {
        usButtons |= XInputPad::kPadRightShoulder;
    }
    if (pucButtons[1] & 0x10)
    {
        usButtons |= XInputPad::kPadBack;
    }
    if (pucButtons[1] & 0x20)
    {
        usButtons |= XInputPad::kPadStart;
    }
    if (pucButtons[1] & 0x40)
    {
        usButtons |= XInputPad::kPadLeftThumb;
    }
    if (pucButtons[1] & 0x80)
    {
        usButtons |= XInputPad::kPadRightThumb;
    }
    return usButtons;
}

static void Sticks(const unsigned char* pucSticks, XInputPad::Pad& rPad)
{
    rPad.sThumbLX = StickValue(pucSticks[0], false);
    rPad.sThumbLY = StickValue(pucSticks[1], true);
    rPad.sThumbRX = StickValue(pucSticks[2], false);
    rPad.sThumbRY = StickValue(pucSticks[3], true);
}

// DualShock 4 reports and both models' simple mode: sticks, three button
// bytes, then L2 and R2.
static void ParseBasic(const unsigned char* pucState, XInputPad::Pad& rPad)
{
    Sticks(pucState, rPad);
    rPad.wButtons = MapButtons(pucState + 4);
    rPad.bLeftTrigger = pucState[7];
    rPad.bRightTrigger = pucState[8];
}

// DualSense full reports: sticks, L2 and R2, a counter, then the buttons.
static void ParseDualSense(const unsigned char* pucState, XInputPad::Pad& rPad)
{
    Sticks(pucState, rPad);
    rPad.bLeftTrigger = pucState[4];
    rPad.bRightTrigger = pucState[5];
    rPad.wButtons = MapButtons(pucState + 7);
}

bool ParseReport(Model eModel, const unsigned char* pucData, unsigned int uiLength, XInputPad::Pad& rPad)
{
    if (!pucData || uiLength < kSimpleInputSize)
    {
        return false;
    }
    const unsigned char ucId = pucData[0];
    if (eModel == kModelDualSense)
    {
        if (ucId == 0x01)
        {
            // USB reads are exactly 64 bytes; Bluetooth simple mode is 10,
            // padded to the Bluetooth report size on Windows.
            if (uiLength == kUsbInputSize)
            {
                ParseDualSense(pucData + 1, rPad);
            }
            else
            {
                ParseBasic(pucData + 1, rPad);
            }
            return true;
        }
        if (ucId == 0x31 && uiLength >= kBtReportSize && BtCrcValid(pucData))
        {
            ParseDualSense(pucData + 2, rPad);
            return true;
        }
        return false;
    }
    if (eModel == kModelDualShock4)
    {
        if (ucId == 0x01)
        {
            ParseBasic(pucData + 1, rPad);
            return true;
        }
        // 0x11 byte 1 bit 7: the report holds controller state.
        if (ucId == 0x11 && uiLength >= kBtReportSize && (pucData[1] & 0x80) && BtCrcValid(pucData))
        {
            ParseBasic(pucData + 3, rPad);
            return true;
        }
        return false;
    }
    return false;
}

bool ParseFirmwareVersion(const unsigned char* pucFeature, unsigned int uiLength, unsigned short& rusVersion)
{
    if (!pucFeature || uiLength < 46 || pucFeature[0] != 0x20)
    {
        return false;
    }
    rusVersion = (unsigned short)(pucFeature[44] | (pucFeature[45] << 8));
    return true;
}

bool UseEnhancedRumble(unsigned short usPid, bool bFirmwareKnown, unsigned short usFirmware)
{
    return usPid == kPidDualSenseEdge || !bFirmwareKnown || usFirmware == 0 || usFirmware >= kFirmwareEnhancedRumble;
}

unsigned int BuildRumbleReport(Model eModel, bool bBluetooth, bool bEnhancedRumble, unsigned char ucSequence,
                               unsigned short usHeavy, unsigned short usLight, unsigned char* pucOut,
                               unsigned int uiSize)
{
    unsigned int uiLength;
    if (eModel == kModelDualShock4)
    {
        uiLength = bBluetooth ? kBtReportSize : kDs4UsbOutputSize;
    }
    else if (eModel == kModelDualSense)
    {
        uiLength = bBluetooth ? kBtReportSize : kDualSenseUsbOutputSize;
    }
    else
    {
        return 0;
    }
    if (!pucOut || uiSize < uiLength)
    {
        return 0;
    }
    memset(pucOut, 0, uiSize);

    if (eModel == kModelDualShock4)
    {
        // Common part: valid flags (0x01 motors), a reserved byte, then the
        // right (light) and left (heavy) motors.
        unsigned char* pucCommon;
        if (bBluetooth)
        {
            pucOut[0] = 0x11;
            pucOut[1] = 0xC4; // HID + CRC, 4 ms report interval
            pucCommon = pucOut + 3;
        }
        else
        {
            pucOut[0] = 0x05;
            pucCommon = pucOut + 1;
        }
        pucCommon[0] = 0x01;
        pucCommon[3] = (unsigned char)(usLight >> 8);
        pucCommon[4] = (unsigned char)(usHeavy >> 8);
    }
    else
    {
        // Common part: valid flags 0 and 1, the right (light) and left (heavy)
        // motors, and valid flags 2 at offset 38.
        unsigned char* pucCommon;
        if (bBluetooth)
        {
            pucOut[0] = 0x31;
            pucOut[1] = (unsigned char)((ucSequence & 0x0F) << 4);
            pucOut[2] = 0x10;
            pucCommon = pucOut + 3;
        }
        else
        {
            pucOut[0] = 0x02;
            pucCommon = pucOut + 1;
        }
        if (bEnhancedRumble)
        {
            pucCommon[0] = 0x02;  // rumble instead of audio haptics
            pucCommon[38] = 0x04; // improved rumble emulation
            pucCommon[2] = (unsigned char)(usLight >> 8);
            pucCommon[3] = (unsigned char)(usHeavy >> 8);
        }
        else
        {
            pucCommon[0] = 0x03; // rumble instead of audio haptics, compatible rumble emulation
            pucCommon[2] = (unsigned char)(usLight >> 9);
            pucCommon[3] = (unsigned char)(usHeavy >> 9);
        }
    }

    if (bBluetooth)
    {
        WriteLe32(pucOut + kBtReportSize - 4, Crc32(kCrcSeedOutput, pucOut, kBtReportSize - 4));
    }
    return uiLength;
}

bool IdListHas(const char* pcList, unsigned short usVid, unsigned short usPid)
{
    if (!pcList)
    {
        return false;
    }
    const char* pc = pcList;
    while ((pc = FindNoCase(pc, "0x")) != 0)
    {
        char* pcEnd;
        const unsigned long ulVid = strtoul(pc, &pcEnd, 16);
        if (pcEnd == pc)
        {
            continue;
        }
        pc = FindNoCase(pcEnd, "0x");
        if (!pc)
        {
            break;
        }
        const unsigned long ulPid = strtoul(pc, &pcEnd, 16);
        if (pcEnd == pc)
        {
            continue;
        }
        pc = pcEnd;
        if (ulVid == usVid && ulPid == usPid)
        {
            return true;
        }
    }
    return false;
}

}
