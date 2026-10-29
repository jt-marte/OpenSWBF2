#pragma once

// Pure XInput mapping logic shared by the game and the tests. No Windows or
// engine headers: values mirror XINPUT_GAMEPAD, the RawControllerInputs slot
// layout and the ePROCESSEDINPUT / eGUIINPUT enums (pinned by static_asserts
// in pcRedJoystick.cpp and pcFLJoystick.cpp).
namespace XInputPad
{

const int kMaxPads = 4;

// XINPUT_GAMEPAD::wButtons bits.
enum PadButton
{
    kPadDPadUp = 0x0001,
    kPadDPadDown = 0x0002,
    kPadDPadLeft = 0x0004,
    kPadDPadRight = 0x0008,
    kPadStart = 0x0010,
    kPadBack = 0x0020,
    kPadLeftThumb = 0x0040,
    kPadRightThumb = 0x0080,
    kPadLeftShoulder = 0x0100,
    kPadRightShoulder = 0x0200,
    kPadA = 0x1000,
    kPadB = 0x2000,
    kPadX = 0x4000,
    kPadY = 0x8000
};

const unsigned char kTriggerThreshold = 30;

// Look (turn/pitch) gain on the right stick, ahead of the game's sensitivity
// curve: the stock tuning felt far too slow on a pad. Values still clamp to
// +-1; with LookCurve full turn input comes at ~55% deflection.
const float kLookGain = 2.7f;

// Stick-driven menu cursor speed, in UI pixels per second (retail: 512).
const float kCursorSpeed = 1024.0f;

struct Pad
{
    unsigned short wButtons;
    unsigned char bLeftTrigger;
    unsigned char bRightTrigger;
    short sThumbLX;
    short sThumbLY;
    short sThumbRX;
    short sThumbRY;
};

// RedJoystick fields an XInput pad fills, in the layout Windows' DirectInput
// driver gives the same pad, so bindings saved against it keep working.
// Buttons: A B X Y LB RB Back Start LS RS = bits 0-9, D-pad = POV bits 32-35.
// Axes: 0 LX, 1 LY (down +), 2 LT-RT, 3 RX, 4 RY (down +), 5 LT, 6 RT.
struct Mapped
{
    unsigned __int64 uiButtons;
    float afAxis[8];
};

enum Bit
{
    kBitA = 0,
    kBitB = 1,
    kBitX = 2,
    kBitY = 3,
    kBitLB = 4,
    kBitRB = 5,
    kBitBack = 6,
    kBitStart = 7,
    kBitLS = 8,
    kBitRS = 9,
    kBitDPadUp = 32,
    kBitDPadRight = 33,
    kBitDPadDown = 34,
    kBitDPadLeft = 35
};

// RawControllerInputs slots: button i -> i, axis a -> 48 + 2a (+) / 49 + 2a (-).
enum Slot
{
    kSlotLeftRight = 48,
    kSlotLeftLeft = 49,
    kSlotLeftDown = 50,
    kSlotLeftUp = 51,
    kSlotRightRight = 54,
    kSlotRightLeft = 55,
    kSlotRightDown = 56,
    kSlotRightUp = 57,
    kSlotLeftTrigger = 58,
    kSlotRightTrigger = 60,
    kSlotCount = 64
};

// Radial deadzone: zero inside fDeadZone (0..0.9 of full deflection), then
// rescaled to reach 1 at 1 - kOuterDeadZone, since many sticks top out short
// of full travel, diagonals especially.
const float kOuterDeadZone = 0.05f;
void ApplyStickDeadzone(short sX, short sY, float fDeadZone, float& fX, float& fY);

// Fixed per-stick deadzones. The retail deadzone slider (gone from Options ->
// Controls, which is for keyboard and mouse) gave 0.01 at its default, fine on
// hall-effect sticks but drifting on worn ones.
const float kMoveDeadZone = 0.10f;
const float kLookDeadZone = 0.08f;

// Look response: the right stick's magnitude to the power kLookExponent, so
// small corrections are finer; kLookGain brings full turn input back at about
// 55% deflection.
const float kLookExponent = 1.6f;
float LookCurve(float fMagnitude);

// Turn speed boost (the game's mTurnBuildup, 0 = base speed, 1 = top speed)
// from how far the right stick is pushed, instead of the retail ramp that
// needed about a second at full tilt. fLookMag is the stick magnitude after
// deadzone and LookCurve: no boost up to kBoostStart (~60% tilt), full boost
// from kBoostFull (~94% tilt), linear in between.
const float kBoostStart = 0.45f;
const float kBoostFull = 0.9f;
float TurnBoost(float fLookMag);

// A soldier state's turn factor for the local pad player. Mouse turn input
// runs to +-16, so the class's per-state factors (roll 0.6, sprint 0.2, jump
// 0.5 by default) barely slow a mouse; a pad tops out at 1 and was crippled.
// Never below 1.
float PadTurnFactor(float fClassFactor);

// Soldier look speeds for the local pad player (rad/s, before the zoom
// scale), from the class's base (MaxPitchSpeed, 1.0 by default) and top
// (MaxTurnSpeed, 2.0) speeds, the turn boost (0..1) and the look scale. Both
// start at the base, so fine aim matches retail. At full tilt yaw reaches
// kPadYawTop of the top speed (1.62 for a soldier); retail pitch never left
// the base, half of yaw, and now reaches kPadPitchTop of it (1.44).
const float kPadYawTop = 0.81f;
const float kPadPitchTop = 0.72f;
float PadYawRate(float fBase, float fTop, float fBoost, float fScale);
float PadPitchRate(float fBase, float fTop, float fBoost, float fScale);

// The controller sensitivity slider under the layout in Options -> Game
// (levels 1..10, default 4, saved in display.ini). On foot it scales the look
// speed (LookSpeedScale: 1.0 at the default, the tuning above, kLookSpeedStep
// per level, 0.625 at 1 and 1.75 at 10); in vehicles it is the retail
// sensitivity curve (AdjustValueForSensitivity), which only reshapes input
// that clamps at 1 and so never changed a soldier's top speed.
const int kLookSpeedDefaultLevel = 4;
const int kLookSpeedMinLevel = 1;
const int kLookSpeedMaxLevel = 10;
const float kLookSpeedStep = 0.125f;
float LookSpeedScale(int iLevel);
extern int gPadLookLevel;

// Published by the input layer each frame for PlayerController and
// EntitySoldier: an XInput pad is connected, and its right stick magnitude
// (after deadzone and curve).
extern bool gPadLookActive;
extern float gPadLookMag;

float TriggerValue(unsigned char bTrigger);

void Map(const Pad& pad, float fMoveDeadZone, float fLookDeadZone, Mapped& out);

unsigned short MotorSpeed(float fLevel);

bool IsXInputDevicePath(const char* pcPath);

bool ParseVidPid(const char* pcPath, unsigned short& usVid, unsigned short& usPid);

// Built-in pad layouts. While an XInput pad is connected it always plays with
// one of these; the PC bindings screen is for keyboard and mouse only.
enum PadLayout
{
    kLayoutModern = 0,
    kLayoutLegacy = 1,
    kLayoutCount = 2
};
extern int gPadLayout;

// Raw slot for an action (ePROCESSEDINPUT value) in an input mode (0 soldier,
// 1 vehicle, 2 flyer, 3 jedi, 4 turret) for a layout, or -1. Legacy is the
// original console-style layout; Modern aims on LT, throws on RB, swaps on Y,
// and takes reload and use from X (HoldTapStep) instead of a slot.
const int kModeCount = 5;
const int kActionCount = 43;
int DefaultSlot(int iLayout, int iMode, int iAction);

// Modern X: a tap (released within kUseHoldMs) reloads, a hold uses (get in
// or out of a vehicle). Each press gives one result.
const unsigned int kUseHoldMs = 500;
enum HoldTapResult
{
    kHoldNone = 0,
    kHoldTap = 1,
    kHoldHeld = 2
};
struct HoldTap
{
    bool bDown;
    bool bFired;
    unsigned int uiDownMs;
};
int HoldTapStep(HoldTap& rState, bool bDown, unsigned int uiNowMs);

// Modern hold-to-aim on top of the game's zoom toggle (Weapon::CycleZoom:
// off -> min -> max on two-level scopes -> off). Gives one zoom press, never
// two frames running, whenever the aim state should change: in while LT is
// held, out once it is released; RS while aiming on a two-level scope steps
// to the other level (max -> off, and the held LT brings it back to min).
struct AimZoom
{
    bool bPressedLast;
    bool bSecondPending;
};
bool AimZoomPress(AimZoom& rState, bool bAimHeld, bool bSecondPressed, bool bAiming, bool bTwoLevel);

// Published by the input layer each frame for PlayerController: Modern is in
// effect on a connected XInput pad, LT is held, RS was pressed this frame.
extern bool gPadAimActive;
extern bool gPadAimHeld;
extern bool gPadSecondZoom;

// Menu (eGUIINPUT value) for a raw pad slot, or -1.
int GuiForSlot(int iSlot);

// Analog trigger -> on/off for the menus, with hysteresis (on at 0.5, off
// below 0.25). The GUI treats an unchanged value as "not pressed this frame",
// so feeding it the raw analog value made a held trigger fire repeatedly.
bool GuiTriggerDown(float fValue, bool bWasDown);

// One step per push of the left stick on the menu y axis (up positive): -1 up,
// +1 down, 0 otherwise. rbHeld latches until the stick is back near centre.
int GuiStickStep(float fAxisY, bool& rbHeld);

// iCur moved by iStep within [0, iCount), wrapping; iCur if iCount < 1.
int WrapIndex(int iCur, int iStep, int iCount);

// Roll on the pad: a second press of crouch (B) within kRollTapMs of the
// first, while the move stick is pushed at least kRollStick (the soldier's own
// roll check, CheckMoveControlForRoll, needs 0.9). True for that one press.
const unsigned int kRollTapMs = 300;
const float kRollStick = 0.9f;
struct RollTap
{
    bool bArmed;
    unsigned int uiTapMs;
};
bool RollDoubleTap(RollTap& rState, bool bPressed, unsigned int uiNowMs, float fMoveMagnitude);

// Left stick as the menu axes (eGUIINPUT_xAxis / yAxis, right and up
// positive), which the GUI manager turns into repeating Left/Right/Up/Down.
void GuiAxes(const float* afAxis, float& fX, float& fY);

// Console-style menu use: a fresh D-pad/A/B/Start/LB/RB press, a trigger
// past 0.5, or the left stick past the GUI manager's 0.5 navigation threshold.
bool IsMenuInput(unsigned __int64 uiPressedButtons, const float* afAxis);

// The pad is driving the menus, so the mouse cursor is hidden until the mouse
// moves or clicks. gMouseResync asks the GUI manager to re-hit-test under the
// cursor once the mouse takes over again.
extern bool gPadOwnsCursor;
extern bool gMouseResync;
// A pad stick (not the mouse) is moving the visible cursor.
extern bool gStickCursor;
void PadTookMenus();
void MouseTookMenus();

// Mirrors of the game enums used above.
enum Action
{
    kActPrimaryFire = 0,
    kActSecondaryFire = 1,
    kActSprint = 2,
    kActJump = 3,
    kActCrouch = 4,
    kActZoom = 5,
    kActView = 6,
    kActReload = 7,
    kActUse = 8,
    kActSquadCommand = 9,
    kActAcceptHero = 10,
    kActDeclineHero = 11,
    kActPrimaryNext = 13,
    kActSecondaryNext = 15,
    kActPlayerList = 17,
    kActRoll = 29, // ePROCESSEDINPUT_TakeScreenshot: the PC "Roll" key (special move)
    kActStrafeAxis = 30,
    kActMoveAxis = 31,
    kActTurnAxis = 32,
    kActPitchAxis = 33
};

enum Gui
{
    kGuiAccept = 0,
    kGuiBack = 1,
    kGuiStart = 2,
    kGuiSelect = 3,
    kGuiMisc1 = 4,
    kGuiMisc2 = 5,
    kGuiUp = 6,
    kGuiDown = 7,
    kGuiLeft = 8,
    kGuiRight = 9,
    kGuiLeftTrigger = 10,
    kGuiRightTrigger = 11,
    kGuiLeftTrigger2 = 12,
    kGuiRightTrigger2 = 13
};

// Command line: -noxinput (pure DirectInput) and -norumble.
const unsigned int kHashNoXInput = 0x0BFED71A;
const unsigned int kHashNoRumble = 0x820BCB67;
extern bool gDisabled;
extern bool gNoRumble;

}
