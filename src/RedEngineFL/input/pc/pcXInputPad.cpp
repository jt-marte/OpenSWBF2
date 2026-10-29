#include "pcXInputPad.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace XInputPad
{

bool gDisabled;
bool gNoRumble;
bool gPadOwnsCursor;
bool gMouseResync;
bool gStickCursor;

static const float kStickMax = 32767.0f;

void ApplyStickDeadzone(short sX, short sY, float fDeadZone, float& fX, float& fY)
{
    if (fDeadZone < 0.0f)
    {
        fDeadZone = 0.0f;
    }
    else if (fDeadZone > 0.9f)
    {
        fDeadZone = 0.9f;
    }

    const float fRawX = (float)sX;
    const float fRawY = (float)sY;
    const float fMag = sqrtf(fRawX * fRawX + fRawY * fRawY);
    const float fInner = fDeadZone * kStickMax;
    if (fMag <= fInner || fMag <= 0.0f)
    {
        fX = 0.0f;
        fY = 0.0f;
        return;
    }

    // Radial: the dead area is a circle and the live range is rescaled so the
    // first step past it starts at zero instead of jumping to the deadzone.
    float fScaled = ((fMag < kStickMax ? fMag : kStickMax) - fInner) / (kStickMax - fInner);
    fX = fRawX / fMag * fScaled;
    fY = fRawY / fMag * fScaled;
}

float TriggerValue(unsigned char bTrigger)
{
    if (bTrigger <= kTriggerThreshold)
    {
        return 0.0f;
    }
    return (float)(bTrigger - kTriggerThreshold) / (float)(255 - kTriggerThreshold);
}

void Map(const Pad& pad, float fDeadZone, Mapped& out)
{
    static const struct { unsigned short wPad; int iBit; } kButtons[] =
    {
        { kPadA, kBitA }, { kPadB, kBitB }, { kPadX, kBitX }, { kPadY, kBitY },
        { kPadLeftShoulder, kBitLB }, { kPadRightShoulder, kBitRB },
        { kPadBack, kBitBack }, { kPadStart, kBitStart },
        { kPadLeftThumb, kBitLS }, { kPadRightThumb, kBitRS },
        { kPadDPadUp, kBitDPadUp }, { kPadDPadRight, kBitDPadRight },
        { kPadDPadDown, kBitDPadDown }, { kPadDPadLeft, kBitDPadLeft }
    };

    out.uiButtons = 0;
    for (int i = 0; i < (int)(sizeof(kButtons) / sizeof(kButtons[0])); ++i)
    {
        if (pad.wButtons & kButtons[i].wPad)
        {
            out.uiButtons |= (unsigned __int64)1 << kButtons[i].iBit;
        }
    }

    float fLX, fLY, fRX, fRY;
    ApplyStickDeadzone(pad.sThumbLX, pad.sThumbLY, fDeadZone, fLX, fLY);
    ApplyStickDeadzone(pad.sThumbRX, pad.sThumbRY, fDeadZone, fRX, fRY);
    const float fLT = TriggerValue(pad.bLeftTrigger);
    const float fRT = TriggerValue(pad.bRightTrigger);

    // XInput Y is up-positive, DirectInput's is down-positive.
    out.afAxis[0] = fLX;
    out.afAxis[1] = -fLY;
    out.afAxis[2] = fLT - fRT;
    out.afAxis[3] = fRX;
    out.afAxis[4] = -fRY;
    out.afAxis[5] = fLT;
    out.afAxis[6] = fRT;
    out.afAxis[7] = 0.0f;
}

unsigned short MotorSpeed(float fLevel)
{
    if (!(fLevel > 0.0f))
    {
        return 0;
    }
    if (fLevel >= 1.0f)
    {
        return 65535;
    }
    return (unsigned short)(fLevel * 65535.0f + 0.5f);
}

static bool FindNoCase(const char* pcText, const char* pcNeedle, const char** ppcAfter)
{
    const size_t uiLen = strlen(pcNeedle);
    for (const char* p = pcText; *p; ++p)
    {
        if (_strnicmp(p, pcNeedle, uiLen) == 0)
        {
            if (ppcAfter)
            {
                *ppcAfter = p + uiLen;
            }
            return true;
        }
    }
    return false;
}

// Windows names the HID interface of every XInput pad with "IG_"; that is the
// documented way to tell them apart from other DirectInput controllers.
bool IsXInputDevicePath(const char* pcPath)
{
    return pcPath && FindNoCase(pcPath, "IG_", 0);
}

static bool ParseHex4(const char* pc, unsigned short& usOut)
{
    unsigned int uiValue = 0;
    for (int i = 0; i < 4; ++i)
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
    usOut = (unsigned short)uiValue;
    return true;
}

bool ParseVidPid(const char* pcPath, unsigned short& usVid, unsigned short& usPid)
{
    const char* pcVid;
    const char* pcPid;
    if (!pcPath || !FindNoCase(pcPath, "VID_", &pcVid) || !FindNoCase(pcPath, "PID_", &pcPid))
    {
        return false;
    }
    return ParseHex4(pcVid, usVid) && ParseHex4(pcPid, usPid);
}

int DefaultSlot(int iMode, int iAction)
{
    if (iMode < 0 || iMode >= kModeCount || iAction < 0 || iAction >= kActionCount)
    {
        return -1;
    }

    const bool bJedi = iMode == 3;
    switch (iAction)
    {
    case kActStrafeAxis:    return kSlotLeftRight;
    case kActMoveAxis:      return kSlotLeftUp;
    case kActTurnAxis:      return kSlotRightRight;
    case kActPitchAxis:     return kSlotRightUp;
    case kActPrimaryFire:   return kSlotRightTrigger;
    case kActSecondaryFire: return kSlotLeftTrigger;
    case kActSprint:        return kBitLS;
    case kActJump:          return kBitA;
    case kActCrouch:        return kBitB;
    case kActReload:        return kBitX;
    case kActUse:           return kBitY;
    case kActZoom:          return bJedi ? -1 : kBitRS;
    case kActView:          return bJedi ? -1 : kBitDPadDown;
    case kActSquadCommand:  return kBitDPadUp;
    case kActAcceptHero:    return kBitDPadRight;
    case kActDeclineHero:   return kBitDPadLeft;
    case kActPrimaryNext:   return bJedi ? -1 : kBitRB;
    case kActSecondaryNext: return kBitLB;
    case kActPlayerList:    return kBitBack;
    default:                return -1;
    }
}

bool HasPadBindings(const unsigned short (*pKeys)[3])
{
    for (int iAction = 0; iAction < kActionCount; ++iAction)
    {
        for (int j = 0; j < 2; ++j)
        {
            const unsigned short usKey = pKeys[iAction][j];
            // Same encoding as RawInputToActionKey: (raw + 1) << 8.
            if (usKey > 0xff && ((usKey >> 8) - 1) < kSlotCount)
            {
                return true;
            }
        }
    }
    return false;
}

int GuiForSlot(int iSlot)
{
    switch (iSlot)
    {
    case kBitA:         return kGuiAccept;
    case kBitB:         return kGuiBack;
    case kBitX:         return kGuiMisc1;
    case kBitY:         return kGuiMisc2;
    case kBitLB:        return kGuiLeftTrigger;
    case kBitRB:        return kGuiRightTrigger;
    case kBitBack:      return kGuiSelect;
    case kBitStart:     return kGuiStart;
    case kBitDPadUp:    return kGuiUp;
    case kBitDPadDown:  return kGuiDown;
    case kBitDPadLeft:  return kGuiLeft;
    case kBitDPadRight: return kGuiRight;
    // The shell's tab manager cycles the lower tab row on these.
    case kSlotLeftTrigger:  return kGuiLeftTrigger2;
    case kSlotRightTrigger: return kGuiRightTrigger2;
    default:            return -1;
    }
}

bool GuiTriggerDown(float fValue, bool bWasDown)
{
    return bWasDown ? fValue > 0.25f : fValue >= 0.5f;
}

int GuiStickStep(float fAxisY, bool& rbHeld)
{
    if (fabsf(fAxisY) < 0.3f)
    {
        rbHeld = false;
        return 0;
    }
    if (rbHeld || fabsf(fAxisY) < 0.5f)
    {
        return 0;
    }
    rbHeld = true;
    return fAxisY > 0.0f ? -1 : 1;
}

int WrapIndex(int iCur, int iStep, int iCount)
{
    if (iCount < 1)
    {
        return iCur;
    }
    int iNew = (iCur + iStep) % iCount;
    return iNew < 0 ? iNew + iCount : iNew;
}

bool RollDoubleTap(RollTap& rState, bool bPressed, unsigned int uiNowMs, float fMoveMagnitude)
{
    if (!bPressed)
    {
        return false;
    }
    if (rState.bArmed && uiNowMs - rState.uiTapMs <= kRollTapMs && fMoveMagnitude >= kRollStick)
    {
        rState.bArmed = false;
        return true;
    }
    rState.bArmed = true;
    rState.uiTapMs = uiNowMs;
    return false;
}

void GuiAxes(const float* afAxis, float& fX, float& fY)
{
    fX = afAxis[0];
    fY = -afAxis[1];
}

bool IsMenuInput(unsigned __int64 uiPressedButtons, const float* afAxis)
{
    const unsigned __int64 uiMenuButtons =
        ((unsigned __int64)1 << kBitA) | ((unsigned __int64)1 << kBitB) |
        ((unsigned __int64)1 << kBitStart) | ((unsigned __int64)1 << kBitDPadUp) |
        ((unsigned __int64)1 << kBitDPadRight) | ((unsigned __int64)1 << kBitDPadDown) |
        ((unsigned __int64)1 << kBitDPadLeft) | ((unsigned __int64)1 << kBitLB) |
        ((unsigned __int64)1 << kBitRB);
    if (uiPressedButtons & uiMenuButtons)
    {
        return true;
    }
    if (afAxis[5] >= 0.5f || afAxis[6] >= 0.5f)
    {
        return true;
    }
    float fX, fY;
    GuiAxes(afAxis, fX, fY);
    return fabsf(fX) >= 0.5f || fabsf(fY) >= 0.5f;
}

void PadTookMenus()
{
    gPadOwnsCursor = true;
}

void MouseTookMenus()
{
    if (gPadOwnsCursor)
    {
        gPadOwnsCursor = false;
        gMouseResync = true;
    }
}

}
