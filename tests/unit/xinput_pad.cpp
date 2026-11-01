#include "third_party/doctest/doctest.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include "testutil.h"
#include "RedEngineFL/input/pc/pcXInputPad.h"

using namespace XInputPad;

namespace
{

Pad Neutral()
{
    Pad pad;
    memset(&pad, 0, sizeof(pad));
    return pad;
}

Mapped MapPad(const Pad& pad, float fDeadZone = 0.2f)
{
    Mapped out;
    Map(pad, fDeadZone, fDeadZone, out);
    return out;
}

bool HasBit(const Mapped& m, int iBit)
{
    return (m.uiButtons & ((unsigned __int64)1 << iBit)) != 0;
}

}

TEST_CASE("xinput neutral pad maps to nothing" * doctest::test_suite("unit"))
{
    Mapped m = MapPad(Neutral());
    CHECK(m.uiButtons == 0);
    for (int i = 0; i < 8; ++i)
    {
        CHECK(m.afAxis[i] == 0.0f);
    }
}

TEST_CASE("xinput buttons land on the directinput xbox layout" * doctest::test_suite("unit"))
{
    static const struct { unsigned short wPad; int iBit; } kCases[] =
    {
        { kPadA, 0 }, { kPadB, 1 }, { kPadX, 2 }, { kPadY, 3 },
        { kPadLeftShoulder, 4 }, { kPadRightShoulder, 5 }, { kPadBack, 6 }, { kPadStart, 7 },
        { kPadLeftThumb, 8 }, { kPadRightThumb, 9 },
        { kPadDPadUp, 32 }, { kPadDPadRight, 33 }, { kPadDPadDown, 34 }, { kPadDPadLeft, 35 }
    };
    for (int i = 0; i < (int)(sizeof(kCases) / sizeof(kCases[0])); ++i)
    {
        Pad pad = Neutral();
        pad.wButtons = kCases[i].wPad;
        Mapped m = MapPad(pad);
        CHECK(m.uiButtons == ((unsigned __int64)1 << kCases[i].iBit));
    }
}

TEST_CASE("xinput dpad diagonal sets both directions" * doctest::test_suite("unit"))
{
    Pad pad = Neutral();
    pad.wButtons = kPadDPadUp | kPadDPadLeft;
    Mapped m = MapPad(pad);
    CHECK(HasBit(m, kBitDPadUp));
    CHECK(HasBit(m, kBitDPadLeft));
    CHECK(!HasBit(m, kBitDPadDown));
    CHECK(!HasBit(m, kBitDPadRight));
}

TEST_CASE("xinput both triggers held are independent" * doctest::test_suite("unit"))
{
    Pad pad = Neutral();
    pad.bLeftTrigger = 255;
    pad.bRightTrigger = 255;
    Mapped m = MapPad(pad);
    CHECK(m.afAxis[5] == doctest::Approx(1.0f));
    CHECK(m.afAxis[6] == doctest::Approx(1.0f));
    // The legacy combined axis cancels, which is why DirectInput pads could
    // never bind fire and secondary fire to the two triggers.
    CHECK(m.afAxis[2] == doctest::Approx(0.0f));
}

TEST_CASE("xinput trigger threshold and legacy combined axis sign" * doctest::test_suite("unit"))
{
    CHECK(TriggerValue(0) == 0.0f);
    CHECK(TriggerValue(kTriggerThreshold) == 0.0f);
    CHECK(TriggerValue(kTriggerThreshold + 1) > 0.0f);
    CHECK(TriggerValue(255) == doctest::Approx(1.0f));

    Pad pad = Neutral();
    pad.bLeftTrigger = 255;
    CHECK(MapPad(pad).afAxis[2] == doctest::Approx(1.0f));
    pad = Neutral();
    pad.bRightTrigger = 255;
    CHECK(MapPad(pad).afAxis[2] == doctest::Approx(-1.0f));
}

TEST_CASE("xinput sticks inside the deadzone read zero" * doctest::test_suite("unit"))
{
    Pad pad = Neutral();
    pad.sThumbLX = 6000;
    pad.sThumbRY = -6000;
    Mapped m = MapPad(pad, 0.2f);
    CHECK(m.afAxis[0] == 0.0f);
    CHECK(m.afAxis[4] == 0.0f);
}

TEST_CASE("xinput sticks are rescaled past the deadzone" * doctest::test_suite("unit"))
{
    float fX, fY;
    ApplyStickDeadzone((short)(0.2f * 32767.0f) + 1, 0, 0.2f, fX, fY);
    CHECK(fX >= 0.0f);
    CHECK(fX < 0.001f);

    ApplyStickDeadzone(32767, 0, 0.2f, fX, fY);
    CHECK(fX == doctest::Approx(1.0f));
    CHECK(fY == 0.0f);

    ApplyStickDeadzone(-32768, 0, 0.2f, fX, fY);
    CHECK(fX == doctest::Approx(-1.0f));

    ApplyStickDeadzone(23170, 23170, 0.2f, fX, fY);
    CHECK(fX == doctest::Approx(0.7071f).epsilon(0.01));
    CHECK(fY == doctest::Approx(0.7071f).epsilon(0.01));
    CHECK(sqrtf(fX * fX + fY * fY) <= 1.0001f);
}

TEST_CASE("xinput deadzone follows the option scale and is clamped" * doctest::test_suite("unit"))
{
    const float fOuter = (1.0f - kOuterDeadZone) * 32767.0f;
    float fX, fY;
    ApplyStickDeadzone(1000, 0, 0.0f, fX, fY);
    CHECK(fX == doctest::Approx(1000.0f / fOuter));

    ApplyStickDeadzone(1000, 0, -1.0f, fX, fY);
    CHECK(fX == doctest::Approx(1000.0f / fOuter));

    ApplyStickDeadzone(32000, 0, 5.0f, fX, fY);
    CHECK(fX > 0.0f);
}

TEST_CASE("xinput outer deadzone reaches full deflection early" * doctest::test_suite("unit"))
{
    float fX, fY;
    // 95% of travel is already full, and it never goes past 1
    ApplyStickDeadzone((short)(0.95f * 32767.0f) + 1, 0, 0.1f, fX, fY);
    CHECK(fX == doctest::Approx(1.0f));
    ApplyStickDeadzone(32767, 0, 0.1f, fX, fY);
    CHECK(fX == doctest::Approx(1.0f));
    // a diagonal that tops out short of the corner still reaches full speed
    ApplyStickDeadzone(23000, 23000, 0.1f, fX, fY); // 99% of the corner
    CHECK(sqrtf(fX * fX + fY * fY) == doctest::Approx(1.0f));
    CHECK(fX == doctest::Approx(fY));
    // halfway is still below full
    ApplyStickDeadzone(16384, 0, 0.1f, fX, fY);
    CHECK(fX > 0.4f);
    CHECK(fX < 0.6f);
}

TEST_CASE("xinput sticks use fixed per-stick deadzones" * doctest::test_suite("unit"))
{
    CHECK(kMoveDeadZone == 0.10f);
    CHECK(kLookDeadZone == 0.08f);
    // a worn stick's drift (5%) reads zero on both sticks
    Pad pad;
    memset(&pad, 0, sizeof(pad));
    pad.sThumbLX = 1638;
    pad.sThumbRX = 1638;
    Mapped out;
    Map(pad, kMoveDeadZone, kLookDeadZone, out);
    CHECK(out.afAxis[0] == 0.0f);
    CHECK(out.afAxis[3] == 0.0f);
}

TEST_CASE("xinput look curve is finer near the centre" * doctest::test_suite("unit"))
{
    CHECK(LookCurve(0.0f) == 0.0f);
    CHECK(LookCurve(-0.5f) == 0.0f);
    CHECK(LookCurve(1.0f) == 1.0f);
    CHECK(LookCurve(1.5f) == 1.0f);
    float fLast = 0.0f;
    for (int i = 1; i <= 20; ++i)
    {
        const float fMag = (float)i / 20.0f;
        const float fCurve = LookCurve(fMag);
        CHECK(fCurve > fLast);
        if (i < 20)
        {
            CHECK(fCurve < fMag);
        }
        fLast = fCurve;
    }
    // with the gain, full turn input comes at about 55% deflection
    CHECK(LookCurve(0.6f) * kLookGain >= 1.0f);
    CHECK(LookCurve(0.5f) * kLookGain < 1.0f);
    CHECK(kLookGain == 2.7f);
}

TEST_CASE("xinput turn boost follows stick tilt with no wind-up" * doctest::test_suite("unit"))
{
    CHECK(TurnBoost(0.0f) == 0.0f);
    CHECK(TurnBoost(-1.0f) == 0.0f);
    CHECK(TurnBoost(kBoostStart) == 0.0f);
    CHECK(TurnBoost(kBoostFull) == 1.0f);
    CHECK(TurnBoost(1.0f) == 1.0f);
    CHECK(TurnBoost(2.0f) == 1.0f);
    float fLast = 0.0f;
    for (int i = 0; i <= 40; ++i)
    {
        const float fBoost = TurnBoost((float)i / 40.0f);
        CHECK(fBoost >= fLast);
        CHECK(fBoost >= 0.0f);
        CHECK(fBoost <= 1.0f);
        fLast = fBoost;
    }
    // full tilt is full speed at once; half tilt keeps the base speed for
    // fine aiming
    CHECK(TurnBoost(LookCurve(1.0f)) == 1.0f);
    CHECK(TurnBoost(LookCurve(0.95f)) == 1.0f);
    CHECK(TurnBoost(LookCurve(0.5f)) == 0.0f);
    CHECK(TurnBoost(LookCurve(0.8f)) > 0.0f);
    CHECK(TurnBoost(LookCurve(0.8f)) < 1.0f);
}

TEST_CASE("xinput pad keeps full turn speed in slow soldier states" * doctest::test_suite("unit"))
{
    CHECK(PadTurnFactor(0.6f) == 1.0f); // retail roll default
    CHECK(PadTurnFactor(0.2f) == 1.0f);
    CHECK(PadTurnFactor(0.0f) == 1.0f);
    CHECK(PadTurnFactor(1.0f) == 1.0f);
    CHECK(PadTurnFactor(1.5f) == 1.5f); // a class that turns faster keeps it
}

TEST_CASE("xinput pad yaw tops out a little under the class top speed" * doctest::test_suite("unit"))
{
    // soldier defaults: base (MaxPitchSpeed) 1, top (MaxTurnSpeed) 2
    CHECK(PadYawRate(1.0f, 2.0f, 0.0f, 1.0f) == doctest::Approx(1.0f));
    CHECK(PadYawRate(1.0f, 2.0f, 1.0f, 1.0f) == doctest::Approx(1.62f));
    CHECK(PadYawRate(1.0f, 2.0f, 1.0f, 1.0f) < 2.0f);
    CHECK(PadYawRate(1.0f, 2.0f, 1.0f, 1.5f) == doctest::Approx(2.43f));
    CHECK(PadYawRate(1.0f, 2.0f, 0.0f, 0.5f) == doctest::Approx(0.5f));
    float fLast = 0.0f;
    for (int i = 0; i <= 20; ++i)
    {
        const float fRate = PadYawRate(1.0f, 2.0f, (float)i / 20.0f, 1.0f);
        CHECK(fRate >= fLast);
        fLast = fRate;
    }
}

TEST_CASE("xinput pad pitch rises with the boost to near yaw" * doctest::test_suite("unit"))
{
    // fine aim keeps the base speed; retail full-tilt pitch stayed at it
    CHECK(PadPitchRate(1.0f, 2.0f, 0.0f, 1.0f) == doctest::Approx(1.0f));
    CHECK(PadPitchRate(1.0f, 2.0f, 1.0f, 1.0f) == doctest::Approx(1.44f));
    CHECK(PadPitchRate(1.0f, 2.0f, 1.0f, 1.0f) > 1.0f);
    CHECK(PadPitchRate(1.0f, 2.0f, 1.0f, 1.0f) < PadYawRate(1.0f, 2.0f, 1.0f, 1.0f));
    CHECK(PadPitchRate(1.0f, 2.0f, 0.5f, 1.0f) == doctest::Approx(1.22f));
    CHECK(PadPitchRate(1.0f, 2.0f, 1.0f, 1.25f) == doctest::Approx(1.8f));
    float fLast = 0.0f;
    for (int i = 0; i <= 20; ++i)
    {
        const float fRate = PadPitchRate(1.0f, 2.0f, (float)i / 20.0f, 1.0f);
        CHECK(fRate >= fLast);
        fLast = fRate;
    }
}

TEST_CASE("xinput joystick sensitivity scales the look speed with the default unchanged" * doctest::test_suite("unit"))
{
    CHECK(LookSpeedScale(kLookSpeedDefaultLevel) == 1.0f);
    CHECK(LookSpeedScale(4) == 1.0f);
    CHECK(gPadLookLevel == kLookSpeedDefaultLevel); // until display.ini says otherwise
    CHECK(LookSpeedScale(1) == doctest::Approx(0.625f));
    CHECK(LookSpeedScale(10) == doctest::Approx(1.75f));
    CHECK(LookSpeedScale(5) == doctest::Approx(1.125f));
    // out of range clamps
    CHECK(LookSpeedScale(0) == LookSpeedScale(1));
    CHECK(LookSpeedScale(-3) == LookSpeedScale(1));
    CHECK(LookSpeedScale(11) == LookSpeedScale(10));
    CHECK(LookSpeedScale(255) == LookSpeedScale(10));
    float fLast = 0.0f;
    for (int i = kLookSpeedMinLevel; i <= kLookSpeedMaxLevel; ++i)
    {
        CHECK(LookSpeedScale(i) > fLast);
        fLast = LookSpeedScale(i);
    }
}

TEST_CASE("xinput look curve leaves movement linear and full look full" * doctest::test_suite("unit"))
{
    Pad pad = Neutral();
    pad.sThumbLX = 16384;
    pad.sThumbRX = 16384;
    Mapped m;
    Map(pad, 0.0f, 0.0f, m);
    CHECK(m.afAxis[0] == doctest::Approx(16384.0f / ((1.0f - kOuterDeadZone) * 32767.0f)));
    CHECK(m.afAxis[3] == doctest::Approx(LookCurve(m.afAxis[0])));
    CHECK(m.afAxis[3] < m.afAxis[0]);

    pad = Neutral();
    pad.sThumbRX = 32767;
    Map(pad, 0.1f, 0.08f, m);
    CHECK(m.afAxis[3] == doctest::Approx(1.0f));
}

TEST_CASE("xinput y axes are down-positive like directinput" * doctest::test_suite("unit"))
{
    Pad pad = Neutral();
    pad.sThumbLY = 32767;
    pad.sThumbRY = -32768;
    Mapped m = MapPad(pad);
    CHECK(m.afAxis[1] == doctest::Approx(-1.0f));
    CHECK(m.afAxis[4] == doctest::Approx(1.0f));

    pad = Neutral();
    pad.sThumbRX = 32767;
    CHECK(MapPad(pad).afAxis[3] == doctest::Approx(1.0f));
}

TEST_CASE("xinput motor speed clamps to the xinput range" * doctest::test_suite("unit"))
{
    CHECK(MotorSpeed(0.0f) == 0);
    CHECK(MotorSpeed(-1.0f) == 0);
    CHECK(MotorSpeed(0.5f) == 32768);
    CHECK(MotorSpeed(1.0f) == 65535);
    CHECK(MotorSpeed(7.0f) == 65535);
    volatile float fZero = 0.0f;
    CHECK(MotorSpeed(fZero / fZero) == 0);
}

TEST_CASE("xinput device paths are recognised by the ig marker" * doctest::test_suite("unit"))
{
    CHECK(IsXInputDevicePath("\\\\?\\HID#VID_045E&PID_028E&IG_00#3&1b2c3d4e&0&0000#{4d1e55b2}"));
    CHECK(IsXInputDevicePath("\\\\?\\hid#vid_2dc8&pid_310a&ig_00#a&1&0#{4d1e55b2}"));
    CHECK(!IsXInputDevicePath("\\\\?\\HID#VID_054C&PID_09CC&MI_03#7&2a&0&0000#{4d1e55b2}"));
    CHECK(!IsXInputDevicePath(""));
    CHECK(!IsXInputDevicePath(0));
}

TEST_CASE("xinput vid and pid are parsed from a device path" * doctest::test_suite("unit"))
{
    unsigned short usVid = 0, usPid = 0;
    CHECK(ParseVidPid("\\\\?\\HID#VID_045E&PID_028E&IG_00#3", usVid, usPid));
    CHECK(usVid == 0x045E);
    CHECK(usPid == 0x028E);

    CHECK(ParseVidPid("\\\\?\\hid#vid_2dc8&pid_310a&ig_00", usVid, usPid));
    CHECK(usVid == 0x2DC8);
    CHECK(usPid == 0x310A);

    CHECK(!ParseVidPid("\\\\?\\HID#VID_045E&IG_00", usVid, usPid));
    CHECK(!ParseVidPid("\\\\?\\HID#VID_04XE&PID_028E", usVid, usPid));
    CHECK(!ParseVidPid(0, usVid, usPid));
}

TEST_CASE("xinput legacy layout is the original console style" * doctest::test_suite("unit"))
{
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActPrimaryFire) == kSlotRightTrigger);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActSecondaryFire) == kSlotLeftTrigger);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActJump) == kBitA);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActCrouch) == kBitB);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActReload) == kBitX);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActUse) == kBitY);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActSprint) == kBitLS);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActZoom) == kBitRS);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActPrimaryNext) == kBitRB);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActSecondaryNext) == kBitLB);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActStrafeAxis) == kSlotLeftRight);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActMoveAxis) == kSlotLeftUp);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActTurnAxis) == kSlotRightRight);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActPitchAxis) == kSlotRightUp);
}

TEST_CASE("xinput modern layout aims on LT and throws on RB" * doctest::test_suite("unit"))
{
    CHECK(DefaultSlot(kLayoutModern, 0, kActPrimaryFire) == kSlotRightTrigger);
    CHECK(DefaultSlot(kLayoutModern, 0, kActZoom) == kSlotLeftTrigger);
    CHECK(DefaultSlot(kLayoutModern, 0, kActSecondaryFire) == kBitRB);
    CHECK(DefaultSlot(kLayoutModern, 0, kActSecondaryNext) == kBitLB);
    CHECK(DefaultSlot(kLayoutModern, 0, kActPrimaryNext) == kBitY);
    // reload and use come from X's tap/hold, not a slot
    CHECK(DefaultSlot(kLayoutModern, 0, kActReload) == -1);
    CHECK(DefaultSlot(kLayoutModern, 0, kActUse) == -1);
    // the rest matches Legacy
    CHECK(DefaultSlot(kLayoutModern, 0, kActJump) == kBitA);
    CHECK(DefaultSlot(kLayoutModern, 0, kActCrouch) == kBitB);
    CHECK(DefaultSlot(kLayoutModern, 0, kActSprint) == kBitLS);
    CHECK(DefaultSlot(kLayoutModern, 0, kActView) == kBitDPadDown);
    CHECK(DefaultSlot(kLayoutModern, 0, kActSquadCommand) == kBitDPadUp);
    // the player list and the map come from Back's tap/hold, in both layouts
    CHECK(DefaultSlot(kLayoutModern, 0, kActPlayerList) == -1);
    CHECK(DefaultSlot(kLayoutLegacy, 1, kActPlayerList) == -1);
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActMap) == -1);
    // heroes keep their exclusions
    CHECK(DefaultSlot(kLayoutModern, 3, kActZoom) == -1);
    CHECK(DefaultSlot(kLayoutModern, 3, kActPrimaryNext) == -1);
    // vehicles get the same buttons
    CHECK(DefaultSlot(kLayoutModern, 1, kActSecondaryFire) == kBitRB);
    CHECK(DefaultSlot(kLayoutModern, 2, kActZoom) == kSlotLeftTrigger);
}

TEST_CASE("xinput every layout and mode can move aim and fire" * doctest::test_suite("unit"))
{
    for (int iLayout = 0; iLayout < kLayoutCount; ++iLayout)
    {
        for (int iMode = 0; iMode < kModeCount; ++iMode)
        {
            CHECK(DefaultSlot(iLayout, iMode, kActMoveAxis) >= 0);
            CHECK(DefaultSlot(iLayout, iMode, kActTurnAxis) >= 0);
            CHECK(DefaultSlot(iLayout, iMode, kActPitchAxis) >= 0);
            CHECK(DefaultSlot(iLayout, iMode, kActPrimaryFire) >= 0);
            CHECK(DefaultSlot(iLayout, iMode, kActSecondaryFire) >= 0);
        }
        CHECK(DefaultSlot(iLayout, 3, kActZoom) == -1);
        CHECK(DefaultSlot(iLayout, -1, kActJump) == -1);
        CHECK(DefaultSlot(iLayout, kModeCount, kActJump) == -1);
        CHECK(DefaultSlot(iLayout, 0, kActionCount) == -1);
    }
    CHECK(DefaultSlot(kLayoutLegacy, 0, kActUse) == kBitY);
    CHECK(DefaultSlot(-1, 0, kActJump) == -1);
    CHECK(DefaultSlot(kLayoutCount, 0, kActJump) == -1);
}

TEST_CASE("xinput layouts never bind one input twice in a mode" * doctest::test_suite("unit"))
{
    for (int iLayout = 0; iLayout < kLayoutCount; ++iLayout)
    {
        for (int iMode = 0; iMode < kModeCount; ++iMode)
        {
            bool abUsed[kSlotCount];
            memset(abUsed, 0, sizeof(abUsed));
            // Modern's X is the tap/hold reload/use
            if (iLayout == kLayoutModern)
            {
                abUsed[kBitX] = true;
            }
            for (int iAction = 0; iAction < kActionCount; ++iAction)
            {
                const int iSlot = DefaultSlot(iLayout, iMode, iAction);
                if (iSlot < 0)
                {
                    continue;
                }
                REQUIRE(iSlot < kSlotCount);
                CHECK(!abUsed[iSlot]);
                abUsed[iSlot] = true;
            }
        }
    }
}

TEST_CASE("xinput modern x taps reload and holds use" * doctest::test_suite("unit"))
{
    HoldTap x = {};
    // quick tap: reload on release, never use
    CHECK(HoldTapStep(x, true, 1000) == kHoldNone);
    CHECK(HoldTapStep(x, true, 1200) == kHoldNone);
    CHECK(HoldTapStep(x, false, 1250) == kHoldTap);
    CHECK(HoldTapStep(x, false, 1300) == kHoldNone);

    // hold: use once at the hold time, and no reload on release
    CHECK(HoldTapStep(x, true, 2000) == kHoldNone);
    CHECK(HoldTapStep(x, true, 2000 + kUseHoldMs - 1) == kHoldNone);
    CHECK(HoldTapStep(x, true, 2000 + kUseHoldMs) == kHoldHeld);
    CHECK(HoldTapStep(x, true, 2000 + kUseHoldMs + 400) == kHoldNone);
    CHECK(HoldTapStep(x, false, 3000) == kHoldNone);
    CHECK(kUseHoldMs == 500);
}

TEST_CASE("xinput back taps the player list and holds the map" * doctest::test_suite("unit"))
{
    for (int iLayout = 0; iLayout < kLayoutCount; ++iLayout)
    {
        for (int iMode = 0; iMode < kModeCount; ++iMode)
        {
            CHECK(strcmp(ActionLabel(iLayout, iMode, kActPlayerList), "Tap Back") == 0);
            CHECK(strcmp(ActionLabel(iLayout, iMode, kActMap), "Hold Back (Back closes it)") == 0);
            CHECK(ActionGlyph(iLayout, iMode, kActPlayerList) == kGlyphBack);
            CHECK(ActionGlyph(iLayout, iMode, kActMap) == kGlyphBack);
            // nothing else is on Back
            for (int iAction = 0; iAction < kActionCount; ++iAction)
            {
                CHECK(DefaultSlot(iLayout, iMode, iAction) != kBitBack);
            }
        }
    }

    HoldTap back = {};
    // a tap: the player list on release
    CHECK(BackStep(back, true, false, 1000) == kBackNone);
    CHECK(BackStep(back, false, false, 1200) == kBackPlayerList);
    // a hold: the map once at the hold time, nothing on release
    CHECK(BackStep(back, true, false, 2000) == kBackNone);
    CHECK(BackStep(back, true, false, 2000 + kUseHoldMs) == kBackMap);
    CHECK(BackStep(back, true, true, 2000 + kUseHoldMs + 300) == kBackNone); // the map is up now
    CHECK(BackStep(back, false, true, 3000) == kBackNone);
    // with the map open a press closes it at once, and its release is no player list
    CHECK(BackStep(back, true, true, 4000) == kBackMap);
    CHECK(BackStep(back, false, false, 4100) == kBackNone);
    // held while closing: still just the one map toggle
    CHECK(BackStep(back, true, true, 5000) == kBackMap);
    CHECK(BackStep(back, true, false, 5000 + kUseHoldMs * 2) == kBackNone);
    CHECK(BackStep(back, false, false, 7000) == kBackNone);
    // closed again: a tap is the player list
    CHECK(BackStep(back, true, false, 8000) == kBackNone);
    CHECK(BackStep(back, false, false, 8100) == kBackPlayerList);
}

TEST_CASE("xinput modern hold to aim drives the zoom toggle" * doctest::test_suite("unit"))
{
    AimZoom s = {};
    // nothing held, not aiming: no presses
    CHECK(!AimZoomPress(s, false, false, false, true));
    // LT down: one press to aim, then a gap frame
    CHECK(AimZoomPress(s, true, false, false, true));
    CHECK(!AimZoomPress(s, true, false, false, true));
    // aiming and held: steady, no presses
    CHECK(!AimZoomPress(s, true, false, true, true));
    CHECK(!AimZoomPress(s, true, false, true, true));
    // RS on a two-level scope: one press to step the level
    CHECK(AimZoomPress(s, true, true, true, true));
    CHECK(!AimZoomPress(s, true, false, true, true));
    CHECK(!AimZoomPress(s, true, false, true, true));
    // LT released while aiming: press out (twice if the first went min -> max)
    CHECK(AimZoomPress(s, false, false, true, true));
    CHECK(!AimZoomPress(s, false, false, true, true));
    CHECK(AimZoomPress(s, false, false, true, true));
    CHECK(!AimZoomPress(s, false, false, false, true));
    CHECK(!AimZoomPress(s, false, false, false, true));

    // single-level scope: RS does nothing
    AimZoom one = {};
    CHECK(AimZoomPress(one, true, false, false, false));
    CHECK(!AimZoomPress(one, true, false, false, false));
    CHECK(!AimZoomPress(one, true, true, true, false));
    CHECK(!AimZoomPress(one, true, false, true, false));

    // an RS click during the gap frame is kept for the next one
    AimZoom gap = {};
    CHECK(AimZoomPress(gap, true, false, false, true));
    CHECK(!AimZoomPress(gap, true, true, true, true));
    CHECK(AimZoomPress(gap, true, false, true, true));

    // RS without LT is ignored
    AimZoom free = {};
    CHECK(!AimZoomPress(free, false, true, false, true));
    CHECK(!AimZoomPress(free, false, false, false, true));
}

TEST_CASE("xinput menu navigation" * doctest::test_suite("unit"))
{
    CHECK(GuiForSlot(kBitA) == kGuiAccept);
    CHECK(GuiForSlot(kBitB) == kGuiBack);
    CHECK(GuiForSlot(kBitStart) == kGuiStart);
    CHECK(GuiForSlot(kBitDPadUp) == kGuiUp);
    CHECK(GuiForSlot(kBitDPadDown) == kGuiDown);
    CHECK(GuiForSlot(kBitDPadLeft) == kGuiLeft);
    CHECK(GuiForSlot(kBitDPadRight) == kGuiRight);
    CHECK(GuiForSlot(kBitLS) == -1);
    CHECK(GuiForSlot(kSlotLeftUp) == -1);
    CHECK(GuiForSlot(kBitLB) == kGuiLeftTrigger);
    CHECK(GuiForSlot(kBitRB) == kGuiRightTrigger);
    CHECK(GuiForSlot(kSlotLeftTrigger) == kGuiLeftTrigger2);
    CHECK(GuiForSlot(kSlotRightTrigger) == kGuiRightTrigger2);
    CHECK(GuiForSlot(kSlotLeftTrigger + 1) == -1);
}

TEST_CASE("xinput left stick feeds the menu axes right and up positive" * doctest::test_suite("unit"))
{
    Pad pad = Neutral();
    pad.sThumbLY = 32767;
    Mapped m = MapPad(pad);
    float fX, fY;
    GuiAxes(m.afAxis, fX, fY);
    CHECK(fY == doctest::Approx(1.0f));
    CHECK(fX == 0.0f);

    pad = Neutral();
    pad.sThumbLY = -32768;
    pad.sThumbLX = 0;
    m = MapPad(pad);
    GuiAxes(m.afAxis, fX, fY);
    CHECK(fY == doctest::Approx(-1.0f));

    pad = Neutral();
    pad.sThumbLX = 32767;
    m = MapPad(pad);
    GuiAxes(m.afAxis, fX, fY);
    CHECK(fX == doctest::Approx(1.0f));

    pad = Neutral();
    pad.sThumbLX = 3000;
    m = MapPad(pad);
    GuiAxes(m.afAxis, fX, fY);
    CHECK(fX == 0.0f);
    CHECK(fY == 0.0f);
}

TEST_CASE("xinput menu input is dpad a b start bumpers triggers or a pushed left stick" * doctest::test_suite("unit"))
{
    float afRest[8];
    memset(afRest, 0, sizeof(afRest));
    CHECK(!IsMenuInput(0, afRest));
    CHECK(IsMenuInput((unsigned __int64)1 << kBitA, afRest));
    CHECK(IsMenuInput((unsigned __int64)1 << kBitB, afRest));
    CHECK(IsMenuInput((unsigned __int64)1 << kBitStart, afRest));
    CHECK(IsMenuInput((unsigned __int64)1 << kBitDPadDown, afRest));
    CHECK(IsMenuInput((unsigned __int64)1 << kBitLB, afRest));
    CHECK(IsMenuInput((unsigned __int64)1 << kBitRB, afRest));
    CHECK(!IsMenuInput((unsigned __int64)1 << kBitLS, afRest));
    CHECK(!IsMenuInput((unsigned __int64)1 << kBitBack, afRest));

    float afTrigger[8];
    memset(afTrigger, 0, sizeof(afTrigger));
    afTrigger[5] = 0.6f;
    CHECK(IsMenuInput(0, afTrigger));
    afTrigger[5] = 0.0f;
    afTrigger[6] = 0.4f;
    CHECK(!IsMenuInput(0, afTrigger));
    afTrigger[6] = 1.0f;
    CHECK(IsMenuInput(0, afTrigger));

    float afStick[8];
    memset(afStick, 0, sizeof(afStick));
    afStick[1] = -0.6f;
    CHECK(IsMenuInput(0, afStick));
    afStick[1] = -0.4f;
    CHECK(!IsMenuInput(0, afStick));

    // The right stick is the cursor, never menu navigation.
    memset(afStick, 0, sizeof(afStick));
    afStick[3] = 1.0f;
    afStick[4] = 1.0f;
    CHECK(!IsMenuInput(0, afStick));
}

TEST_CASE("xinput pad and mouse hand the menus back and forth" * doctest::test_suite("unit"))
{
    gPadOwnsCursor = false;
    gMouseResync = false;

    MouseTookMenus();
    CHECK(!gPadOwnsCursor);
    CHECK(!gMouseResync);

    PadTookMenus();
    CHECK(gPadOwnsCursor);
    CHECK(!gMouseResync);

    MouseTookMenus();
    CHECK(!gPadOwnsCursor);
    CHECK(gMouseResync);

    gMouseResync = false;
    MouseTookMenus();
    CHECK(!gMouseResync);
}

TEST_CASE("xinput menu trigger is a steady on off with hysteresis" * doctest::test_suite("unit"))
{
    CHECK(!GuiTriggerDown(0.0f, false));
    CHECK(!GuiTriggerDown(0.49f, false));
    CHECK(GuiTriggerDown(0.5f, false));
    CHECK(GuiTriggerDown(1.0f, false));

    // Held and wobbling: stays down until it is clearly released.
    bool bDown = false;
    const float afHeld[] = { 0.6f, 0.93f, 0.91f, 0.95f, 0.4f, 0.3f, 0.26f };
    for (int i = 0; i < (int)(sizeof(afHeld) / sizeof(afHeld[0])); ++i)
    {
        bDown = GuiTriggerDown(afHeld[i], bDown);
        CHECK(bDown);
    }
    bDown = GuiTriggerDown(0.25f, bDown);
    CHECK(!bDown);
    bDown = GuiTriggerDown(0.45f, bDown);
    CHECK(!bDown);
}

TEST_CASE("xinput menu stick steps once per push" * doctest::test_suite("unit"))
{
    bool bHeld = false;
    CHECK(GuiStickStep(0.0f, bHeld) == 0);
    CHECK(GuiStickStep(0.45f, bHeld) == 0); // not far enough yet
    CHECK(GuiStickStep(0.8f, bHeld) == -1); // up
    CHECK(bHeld);
    CHECK(GuiStickStep(1.0f, bHeld) == 0);  // held: no repeat
    CHECK(GuiStickStep(0.35f, bHeld) == 0); // still latched
    CHECK(GuiStickStep(0.1f, bHeld) == 0);  // released
    CHECK(!bHeld);
    CHECK(GuiStickStep(-0.9f, bHeld) == 1); // down
    CHECK(GuiStickStep(-0.9f, bHeld) == 0);
}

TEST_CASE("xinput menu index wraps" * doctest::test_suite("unit"))
{
    CHECK(WrapIndex(0, 1, 6) == 1);
    CHECK(WrapIndex(5, 1, 6) == 0);
    CHECK(WrapIndex(0, -1, 6) == 5);
    CHECK(WrapIndex(3, -1, 6) == 2);
    CHECK(WrapIndex(0, 1, 1) == 0);
    CHECK(WrapIndex(2, 1, 0) == 2); // nothing to pick
}

TEST_CASE("xinput double tap crouch while moving rolls" * doctest::test_suite("unit"))
{
    RollTap tap = {};
    // tap, tap within the window while moving: rolls on the second tap only
    CHECK(!RollDoubleTap(tap, true, 1000, 1.0f));
    CHECK(!RollDoubleTap(tap, false, 1100, 1.0f));
    CHECK(RollDoubleTap(tap, true, 1200, 1.0f));
    // a third quick tap starts over instead of rolling again
    CHECK(!RollDoubleTap(tap, true, 1300, 1.0f));

    // too slow: the second tap is a new first tap
    RollTap slow = {};
    CHECK(!RollDoubleTap(slow, true, 0, 1.0f));
    CHECK(!RollDoubleTap(slow, true, kRollTapMs + 1, 1.0f));
    CHECK(RollDoubleTap(slow, true, kRollTapMs + 100, 1.0f));

    // standing still (or barely moving): crouch taps never roll
    RollTap still = {};
    CHECK(!RollDoubleTap(still, true, 0, 0.0f));
    CHECK(!RollDoubleTap(still, true, 100, 0.5f));
    CHECK(!RollDoubleTap(still, true, 200, kRollStick - 0.01f));
    CHECK(RollDoubleTap(still, true, 300, kRollStick));
}

TEST_CASE("xinput modern rs locks on unless a two-level scope is up" * doctest::test_suite("unit"))
{
    RsLock state = {};
    // a press with no scope up locks on for as long as it is held
    CHECK(RsLockTarget(state, true, false));
    CHECK(RsLockTarget(state, true, false));
    CHECK_FALSE(RsLockTarget(state, false, false));
    // a press with the scope up steps the zoom (AimZoomPress) and never locks on, even if the
    // scope drops while RS is still held
    CHECK_FALSE(RsLockTarget(state, true, true));
    CHECK_FALSE(RsLockTarget(state, true, false));
    CHECK_FALSE(RsLockTarget(state, false, false));
    // and a press that started as lock-on stays lock-on if the scope comes up mid-hold
    CHECK(RsLockTarget(state, true, false));
    CHECK(RsLockTarget(state, true, true));
}

TEST_CASE("xinput controller page labels every button" * doctest::test_suite("unit"))
{
    CHECK(strcmp(SlotLabel(kBitA), "A") == 0);
    CHECK(strcmp(SlotLabel(kBitRB), "RB") == 0);
    CHECK(strcmp(SlotLabel(kBitDPadUp), "D-pad Up") == 0);
    CHECK(strcmp(SlotLabel(kSlotLeftTrigger), "LT") == 0);
    CHECK(strcmp(SlotLabel(kSlotRightTrigger), "RT") == 0);
    CHECK(strcmp(SlotLabel(kSlotLeftUp), "Left stick (up/down)") == 0);
    CHECK(strcmp(SlotLabel(kSlotRightRight), "Right stick (left/right)") == 0);
    CHECK(!SlotLabel(-1));
    CHECK(!SlotLabel(20));
    // every slot a layout uses has a label
    for (int iLayout = 0; iLayout < kLayoutCount; ++iLayout)
    {
        for (int iMode = 0; iMode < kModeCount; ++iMode)
        {
            for (int iAction = 0; iAction < kActionCount; ++iAction)
            {
                const int iSlot = DefaultSlot(iLayout, iMode, iAction);
                if (iSlot >= 0)
                {
                    CHECK(SlotLabel(iSlot) != (const char*)0);
                }
            }
        }
    }
}

TEST_CASE("xinput controller page names each layout's actions" * doctest::test_suite("unit"))
{
    // Modern: aim on LT, throw on RB, swap on Y, X tap/hold
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActPrimaryFire), "RT") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActSecondaryFire), "RB") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActPrimaryNext), "Y") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActReload), "Tap X") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActUse), "Hold X") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActZoom), "Hold LT (RS: next zoom)") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 1, kActZoom), "LT") == 0); // vehicles toggle
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActLockTarget), "Click RS") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 2, kActLockTarget), "Click RS") == 0); // flyers
    CHECK(!ActionLabel(kLayoutLegacy, 1, kActLockTarget)); // RS is zoom there
    // Legacy: the console layout
    CHECK(strcmp(ActionLabel(kLayoutLegacy, 0, kActSecondaryFire), "LT") == 0);
    CHECK(strcmp(ActionLabel(kLayoutLegacy, 0, kActReload), "X") == 0);
    CHECK(strcmp(ActionLabel(kLayoutLegacy, 0, kActUse), "Y") == 0);
    CHECK(strcmp(ActionLabel(kLayoutLegacy, 0, kActZoom), "Click RS") == 0);
    // roll on foot only, in both
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActRoll), "Double-tap B + move") == 0);
    CHECK(strcmp(ActionLabel(kLayoutLegacy, 3, kActRoll), "Double-tap B + move") == 0);
    CHECK(!ActionLabel(kLayoutModern, 1, kActRoll));
    // sticks, and what the pad leaves unbound
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActMoveAxis), "Left stick (up/down)") == 0);
    CHECK(strcmp(ActionLabel(kLayoutLegacy, 2, kActTurnAxis), "Right stick (left/right)") == 0);
    CHECK(strcmp(ActionLabel(kLayoutModern, 0, kActMap), "Hold Back (Back closes it)") == 0); // Back's hold
    CHECK(!ActionLabel(kLayoutModern, 3, kActZoom)); // jedi
}

TEST_CASE("xinput prompts show each layout's button for a console token" * doctest::test_suite("unit"))
{
    // the console's buttons are the actions they stood for
    CHECK(TokenGlyph("btnr1", kLayoutModern, 0) == kGlyphRT);
    CHECK(TokenGlyph("btnl1", kLayoutModern, 0) == kGlyphRB);
    CHECK(TokenGlyph("btnl1", kLayoutLegacy, 0) == kGlyphLT);
    CHECK(TokenGlyph("btnr2", kLayoutModern, 0) == kGlyphY);
    CHECK(TokenGlyph("btnr2", kLayoutLegacy, 0) == kGlyphRB);
    CHECK(TokenGlyph("btna", kLayoutModern, 2) == kGlyphA);
    CHECK(TokenGlyph("btnb", kLayoutModern, 1) == kGlyphX);  // hold X to get in or out
    CHECK(TokenGlyph("btnb", kLayoutLegacy, 1) == kGlyphY);
    CHECK(TokenGlyph("btn_analog_l_down", kLayoutModern, 0) == kGlyphLS);
    CHECK(TokenGlyph("btn_analog_r_down", kLayoutModern, 0) == kGlyphLT); // aim
    CHECK(TokenGlyph("btn_analog_r_down", kLayoutLegacy, 0) == kGlyphRS);
    CHECK(TokenGlyph("btn_analog_l", kLayoutModern, 0) == kGlyphLeftStick);
    CHECK(TokenGlyph("BTN_ANALOG_R", kLayoutModern, 0) == kGlyphRightStick); // case-insensitive
    // the game's own tokens
    CHECK(TokenGlyph("GuiAccept", kLayoutModern, 0) == kGlyphA);
    CHECK(TokenGlyph("GuiCancel", kLayoutLegacy, 0) == kGlyphB);
    CHECK(TokenGlyph("crouchButton", kLayoutModern, 0) == kGlyphB);
    CHECK(TokenGlyph("useButton", kLayoutLegacy, 0) == kGlyphY);
    CHECK(TokenGlyph("OptionL", kLayoutModern, 0) == kTokenRetail);
    // lock-on: RS on Modern, unbound on Legacy (RS zooms)
    CHECK(TokenGlyph("btnmisc", kLayoutModern, 2) == kGlyphRS);
    // unbound on the pad, or unknown
    CHECK(TokenGlyph("btnmisc", kLayoutLegacy, 0) == -1);
    CHECK(TokenGlyph("Map", kLayoutModern, 0) == kGlyphBack); // Back's hold
    CHECK(TokenGlyph("nosuchtoken", kLayoutModern, 0) == -1);
    CHECK(TokenGlyph(0, kLayoutModern, 0) == -1);
    // every token has a name, and every glyph a label (the D-pad is drawn)
    for (int i = 0; i < TokenCount(); ++i)
    {
        CHECK(TokenName(i) != (const char*)0);
    }
    CHECK(strcmp(GlyphText(kGlyphA), "A") == 0);
    CHECK(strcmp(GlyphText(kGlyphStart), "START") == 0);
    CHECK(strcmp(GlyphText(kGlyphCount), "") == 0);
    CHECK(GlyphForSlot(kSlotRightTrigger) == kGlyphRT);
    CHECK(GlyphForSlot(kBitDPadLeft) == kGlyphDPadLeft);
    CHECK(GlyphForSlot(-1) == -1);
}

TEST_CASE("xinput prompts only replace text whose tokens all resolve" * doctest::test_suite("unit"))
{
    const unsigned short usFire[] = { '{', 'b', 't', 'n', 'r', '1', '}', ' ', 't', 'o', ' ', 'f', 'i', 'r', 'e', 0 };
    const unsigned short usLock[] = { '{', 'b', 't', 'n', 'm', 'i', 's', 'c', '}', 0 };
    const unsigned short usPlain[] = { 'h', 'i', 0 };
    const unsigned short usOpen[] = { '{', 'b', 't', 'n', 'a', 0 };
    const unsigned short usOption[] = { '{', 'O', 'p', 't', 'i', 'o', 'n', 'L', '}', '{', 'b', 't', 'n', 'a', '}', 0 };
    CHECK(PromptTextResolves(usFire, kLayoutModern, 0));
    CHECK(PromptTextResolves(usLock, kLayoutModern, 1));
    CHECK_FALSE(PromptTextResolves(usLock, kLayoutLegacy, 1));
    CHECK(PromptTextResolves(usPlain, kLayoutModern, 0));
    CHECK_FALSE(PromptTextResolves(usOpen, kLayoutModern, 0));
    CHECK(PromptTextResolves(usOption, kLayoutLegacy, 0));
    CHECK_FALSE(PromptTextResolves(0, kLayoutModern, 0));
}

TEST_CASE("xinput prompts follow the pad in game or in the menus" * doctest::test_suite("unit"))
{
    gPadPromptsGame = false;
    gPadOwnsCursor = false;
    CHECK_FALSE(PadPromptsActive());
    gPadPromptsGame = true;
    CHECK(PadPromptsActive());
    gPadPromptsGame = false;
    gPadOwnsCursor = true;
    CHECK(PadPromptsActive());
    gPadOwnsCursor = false;
}
