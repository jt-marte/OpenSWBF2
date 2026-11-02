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
bool gBumperDown;
int gPadLayout = kLayoutModern;
bool gPadAimActive;
bool gPadAimHeld;
bool gPadSecondZoom;
bool gPadLocalAiming;
bool gPadLocalTwoLevel;
bool gPadScopeStepsZoom;

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
    // first step past it starts at zero instead of jumping to the deadzone,
    // and ends at full deflection a little before the stick's travel does.
    const float fOuter = (1.0f - kOuterDeadZone) * kStickMax;
    float fScaled = ((fMag < fOuter ? fMag : fOuter) - fInner) / (fOuter - fInner);
    fX = fRawX / fMag * fScaled;
    fY = fRawY / fMag * fScaled;
}

float LookCurve(float fMagnitude)
{
    if (!(fMagnitude > 0.0f))
    {
        return 0.0f;
    }
    if (fMagnitude >= 1.0f)
    {
        return 1.0f;
    }
    return powf(fMagnitude, kLookExponent);
}

bool gPadLookActive;
float gPadLookMag;

float PadTurnFactor(float fClassFactor)
{
    return fClassFactor < 1.0f ? 1.0f : fClassFactor;
}

float TurnBoost(float fLookMag)
{
    if (!(fLookMag > kBoostStart))
    {
        return 0.0f;
    }
    if (fLookMag >= kBoostFull)
    {
        return 1.0f;
    }
    return (fLookMag - kBoostStart) / (kBoostFull - kBoostStart);
}

unsigned char BoostStep(float fBoost)
{
    if (!(fBoost > 0.0f))
    {
        return 0;
    }
    if (fBoost >= 1.0f)
    {
        return (unsigned char)kBoostSteps;
    }
    return (unsigned char)(fBoost * (float)kBoostSteps + 0.5f);
}

float BoostFromStep(unsigned char ucStep)
{
    return ucStep >= kBoostSteps ? 1.0f : (float)ucStep / (float)kBoostSteps;
}

int gPadLookLevel = kLookSpeedDefaultLevel;

float PadYawRate(float fBase, float fTop, float fBoost, float fScale)
{
    return ((fTop * kPadYawTop - fBase) * fBoost + fBase) * fScale;
}

float PadPitchRate(float fBase, float fTop, float fBoost, float fScale)
{
    return ((fTop * kPadPitchTop - fBase) * fBoost + fBase) * fScale;
}

float LookSpeedScale(int iLevel)
{
    if (iLevel < kLookSpeedMinLevel)
    {
        iLevel = kLookSpeedMinLevel;
    }
    else if (iLevel > kLookSpeedMaxLevel)
    {
        iLevel = kLookSpeedMaxLevel;
    }
    return 1.0f + (float)(iLevel - kLookSpeedDefaultLevel) * kLookSpeedStep;
}

float TriggerValue(unsigned char bTrigger)
{
    if (bTrigger <= kTriggerThreshold)
    {
        return 0.0f;
    }
    return (float)(bTrigger - kTriggerThreshold) / (float)(255 - kTriggerThreshold);
}

void Map(const Pad& pad, float fMoveDeadZone, float fLookDeadZone, Mapped& out)
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
    ApplyStickDeadzone(pad.sThumbLX, pad.sThumbLY, fMoveDeadZone, fLX, fLY);
    ApplyStickDeadzone(pad.sThumbRX, pad.sThumbRY, fLookDeadZone, fRX, fRY);
    // Look gets the response curve, radially so diagonals keep their angle;
    // movement stays linear.
    const float fLookMag = sqrtf(fRX * fRX + fRY * fRY);
    if (fLookMag > 0.0f)
    {
        const float fCurve = LookCurve(fLookMag) / fLookMag;
        fRX *= fCurve;
        fRY *= fCurve;
    }
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

int DefaultSlot(int iLayout, int iMode, int iAction)
{
    if (iLayout < 0 || iLayout >= kLayoutCount || iMode < 0 || iMode >= kModeCount || iAction < 0 ||
        iAction >= kActionCount)
    {
        return -1;
    }

    const bool bJedi = iMode == 3;
    const bool bModern = iLayout == kLayoutModern;
    switch (iAction)
    {
    case kActStrafeAxis:    return kSlotLeftRight;
    case kActMoveAxis:      return kSlotLeftUp;
    case kActTurnAxis:      return kSlotRightRight;
    case kActPitchAxis:     return kSlotRightUp;
    case kActPrimaryFire:   return kSlotRightTrigger;
    case kActSecondaryFire: return bModern ? kBitRB : kSlotLeftTrigger;
    case kActSprint:        return kBitLS;
    case kActJump:          return kBitA;
    case kActCrouch:        return kBitB;
    case kActReload:        return bModern ? -1 : kBitX;
    case kActUse:           return bModern ? -1 : kBitY;
    case kActZoom:          return bJedi ? -1 : (bModern ? kSlotLeftTrigger : kBitRS);
    case kActView:          return bJedi ? -1 : kBitDPadDown;
    case kActSquadCommand:  return kBitDPadUp;
    case kActAcceptHero:    return kBitDPadRight;
    case kActDeclineHero:   return kBitDPadLeft;
    case kActPrimaryNext:   return bJedi ? -1 : (bModern ? kBitY : kBitRB);
    case kActSecondaryNext: return kBitLB;
    // Back is the player list on a tap and the map on a hold (pcFLJoystick's PadDefaultsProcess)
    default:                return -1;
    }
}

const char* SlotLabel(int iSlot)
{
    switch (iSlot)
    {
    case kBitA:             return "A";
    case kBitB:             return "B";
    case kBitX:             return "X";
    case kBitY:             return "Y";
    case kBitLB:            return "LB";
    case kBitRB:            return "RB";
    case kBitBack:          return "Back";
    case kBitStart:         return "Start";
    case kBitLS:            return "Click LS";
    case kBitRS:            return "Click RS";
    case kBitDPadUp:        return "D-pad Up";
    case kBitDPadRight:     return "D-pad Right";
    case kBitDPadDown:      return "D-pad Down";
    case kBitDPadLeft:      return "D-pad Left";
    case kSlotLeftRight:
    case kSlotLeftLeft:     return "Left stick (left/right)";
    case kSlotLeftDown:
    case kSlotLeftUp:       return "Left stick (up/down)";
    case kSlotRightRight:
    case kSlotRightLeft:    return "Right stick (left/right)";
    case kSlotRightDown:
    case kSlotRightUp:      return "Right stick (up/down)";
    case kSlotLeftTrigger:  return "LT";
    case kSlotRightTrigger: return "RT";
    default:                return 0;
    }
}

const char* ActionLabel(int iLayout, int iMode, int iAction)
{
    const bool bModern = iLayout == kLayoutModern;
    const bool bOnFoot = iMode == 0 || iMode == 3;
    if (bModern && iAction == kActReload)
    {
        return "Tap X";
    }
    if (bModern && iAction == kActUse)
    {
        return "Hold X";
    }
    if (bModern && iAction == kActZoom && iMode == 0)
    {
        return "Hold LT (RS: next zoom)";
    }
    if (bModern && iAction == kActLockTarget)
    {
        return "Click RS"; // RsLockTarget
    }
    if (iAction == kActRoll)
    {
        return bOnFoot ? "Double-tap B + move" : 0;
    }
    if (iAction == kActPlayerList)
    {
        return "Tap Back";
    }
    if (iAction == kActMap)
    {
        return "Hold Back (Back closes it)";
    }
    return SlotLabel(DefaultSlot(iLayout, iMode, iAction));
}

int GlyphForSlot(int iSlot)
{
    switch (iSlot)
    {
    case kBitA:             return kGlyphA;
    case kBitB:             return kGlyphB;
    case kBitX:             return kGlyphX;
    case kBitY:             return kGlyphY;
    case kBitLB:            return kGlyphLB;
    case kBitRB:            return kGlyphRB;
    case kBitBack:          return kGlyphBack;
    case kBitStart:         return kGlyphStart;
    case kBitLS:            return kGlyphLS;
    case kBitRS:            return kGlyphRS;
    case kBitDPadUp:        return kGlyphDPadUp;
    case kBitDPadRight:     return kGlyphDPadRight;
    case kBitDPadDown:      return kGlyphDPadDown;
    case kBitDPadLeft:      return kGlyphDPadLeft;
    case kSlotLeftRight:
    case kSlotLeftLeft:
    case kSlotLeftDown:
    case kSlotLeftUp:       return kGlyphLeftStick;
    case kSlotRightRight:
    case kSlotRightLeft:
    case kSlotRightDown:
    case kSlotRightUp:      return kGlyphRightStick;
    case kSlotLeftTrigger:  return kGlyphLT;
    case kSlotRightTrigger: return kGlyphRT;
    default:                return -1;
    }
}

const char* GlyphText(int iGlyph)
{
    static const char* const kText[kGlyphCount] = {
        "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS", "RS",
        "BACK", "START", "", "", "", "", "L", "R",
    };
    return iGlyph >= 0 && iGlyph < kGlyphCount ? kText[iGlyph] : "";
}

int ActionGlyph(int iLayout, int iMode, int iAction)
{
    const bool bModern = iLayout == kLayoutModern;
    if (bModern && (iAction == kActReload || iAction == kActUse))
    {
        return kGlyphX;
    }
    if (bModern && iAction == kActZoom && iMode == 0)
    {
        return kGlyphLT;
    }
    if (bModern && iAction == kActLockTarget)
    {
        return kGlyphRS;
    }
    if (iAction == kActRoll)
    {
        return (iMode == 0 || iMode == 3) ? kGlyphB : -1;
    }
    if (iAction == kActPlayerList || iAction == kActMap)
    {
        return kGlyphBack;
    }
    return GlyphForSlot(DefaultSlot(iLayout, iMode, iAction));
}

namespace
{

const int kTokLeftStick = -10;
const int kTokRightStick = -11;
const int kTokAccept = -12;
const int kTokCancel = -13;
const int kTokRetailGlyph = -14;

struct Token
{
    const char* pcName;
    int iAction;
};

const Token kTokens[] = {
    // the console's buttons, as the actions they stood for in its hints
    { "btnr1", kActPrimaryFire },
    { "btnl1", kActSecondaryFire },
    { "btnr2", kActPrimaryNext },
    { "btnl2", kActSecondaryNext },
    { "btna", kActJump },
    { "btnb", kActUse },
    { "btn2", kActUse },
    { "btnmisc", kActLockTarget },
    { "btnmisc2", kActCrouch },
    { "btn_analog_l_down", kActSprint },
    { "btn_analog_r_down", kActZoom },
    { "btn_select", kActPlayerList },
    { "btn_analog_l", kTokLeftStick },
    { "btn_analog_r", kTokRightStick },
    // the game's own action and menu tokens
    { "primaryFireButton", kActPrimaryFire },
    { "secondaryFireButton", kActSecondaryFire },
    { "sprintButton", kActSprint },
    { "jumpButton", kActJump },
    { "crouchButton", kActCrouch },
    { "zoomButton", kActZoom },
    { "viewButton", kActView },
    { "reloadButton", kActReload },
    { "useButton", kActUse },
    { "squadCommandButton", kActSquadCommand },
    { "acceptHero", kActAcceptHero },
    { "declineHero", kActDeclineHero },
    { "lockTargetButton", kActLockTarget },
    { "primaryNextButton", kActPrimaryNext },
    { "primaryPrevButton", kActPrimaryPrev },
    { "secondaryNextButton", kActSecondaryNext },
    { "secondaryPrevButton", kActSecondaryPrev },
    { "playerList", kActPlayerList },
    { "chat", kActChat },
    { "friendChat", kActFriendChat },
    { "Map", kActMap },
    { "GuiAccept", kTokAccept },
    { "GuiCancel", kTokCancel },
    { "OptionL", kTokRetailGlyph },
    { "OptionR", kTokRetailGlyph },
};

}

int TokenCount()
{
    return (int)(sizeof(kTokens) / sizeof(kTokens[0]));
}

const char* TokenName(int iIndex)
{
    return iIndex >= 0 && iIndex < TokenCount() ? kTokens[iIndex].pcName : 0;
}

int TokenGlyph(const char* pcToken, int iLayout, int iMode)
{
    if (!pcToken)
    {
        return -1;
    }
    for (int i = 0; i < TokenCount(); ++i)
    {
        if (_stricmp(pcToken, kTokens[i].pcName) != 0)
        {
            continue;
        }
        switch (kTokens[i].iAction)
        {
        case kTokLeftStick:   return kGlyphLeftStick;
        case kTokRightStick:  return kGlyphRightStick;
        case kTokAccept:      return kGlyphA;
        case kTokCancel:      return kGlyphB;
        case kTokRetailGlyph: return kTokenRetail;
        default:              return ActionGlyph(iLayout, iMode, kTokens[i].iAction);
        }
    }
    return -1;
}

bool PromptTextResolves(const unsigned short* pusText, int iLayout, int iMode)
{
    if (!pusText)
    {
        return false;
    }
    for (const unsigned short* p = pusText; *p; ++p)
    {
        if (*p != '{')
        {
            continue;
        }
        char acToken[64];
        int iLen = 0;
        ++p;
        while (*p && *p != '}' && iLen < (int)sizeof(acToken) - 1)
        {
            acToken[iLen++] = (char)(*p < 0x80 ? *p : '?');
            ++p;
        }
        acToken[iLen] = '\0';
        if (*p != '}' || TokenGlyph(acToken, iLayout, iMode) == -1)
        {
            return false;
        }
    }
    return true;
}

bool gPadPromptsGame;
int gPadPromptMode;

bool PadPromptsActive()
{
    return gPadPromptsGame || gPadOwnsCursor;
}

int HoldTapStep(HoldTap& rState, bool bDown, unsigned int uiNowMs)
{
    if (bDown)
    {
        if (!rState.bDown)
        {
            rState.bDown = true;
            rState.bFired = false;
            rState.uiDownMs = uiNowMs;
            return kHoldNone;
        }
        if (!rState.bFired && uiNowMs - rState.uiDownMs >= kUseHoldMs)
        {
            rState.bFired = true;
            return kHoldHeld;
        }
        return kHoldNone;
    }
    if (rState.bDown)
    {
        rState.bDown = false;
        if (!rState.bFired)
        {
            return kHoldTap;
        }
    }
    return kHoldNone;
}

int BackStep(HoldTap& rState, bool bDown, bool bMapOpen, unsigned int uiNowMs)
{
    if (bDown && !rState.bDown && bMapOpen)
    {
        rState.bDown = true;
        rState.bFired = true;
        rState.uiDownMs = uiNowMs;
        return kBackMap;
    }
    switch (HoldTapStep(rState, bDown, uiNowMs))
    {
    case kHoldTap:
        return kBackPlayerList;
    case kHoldHeld:
        return kBackMap;
    default:
        return kBackNone;
    }
}

bool AimZoomPress(AimZoom& rState, bool bAimHeld, bool bSecondPressed, bool bAiming, bool bTwoLevel)
{
    if (!bAimHeld)
    {
        rState.bSecondPending = false;
    }
    else if (bSecondPressed)
    {
        rState.bSecondPending = true;
    }
    // presses must be edges: leave a frame up between them
    if (rState.bPressedLast)
    {
        rState.bPressedLast = false;
        return false;
    }

    bool bPress = false;
    if (bAimHeld != bAiming)
    {
        bPress = true;
    }
    else if (bAiming && rState.bSecondPending)
    {
        rState.bSecondPending = false;
        bPress = bTwoLevel;
    }
    rState.bPressedLast = bPress;
    return bPress;
}

bool RsLockTarget(RsLock& rState, bool bRSDown, bool bScopeStepsZoom)
{
    if (bRSDown && !rState.bDown)
    {
        rState.bForZoom = bScopeStepsZoom;
    }
    rState.bDown = bRSDown;
    return bRSDown && !rState.bForZoom;
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
