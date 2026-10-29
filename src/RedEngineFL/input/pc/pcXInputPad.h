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
// curve: the stock tuning felt half as fast as it should on a pad. Values
// still clamp to +-1, so full turn speed comes at half deflection.
const float kLookGain = 2.0f;

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

void ApplyStickDeadzone(short sX, short sY, float fDeadZone, float& fX, float& fY);

float TriggerValue(unsigned char bTrigger);

void Map(const Pad& pad, float fDeadZone, Mapped& out);

unsigned short MotorSpeed(float fLevel);

bool IsXInputDevicePath(const char* pcPath);

bool ParseVidPid(const char* pcPath, unsigned short& usVid, unsigned short& usPid);

// Default pad layout: raw slot for an action (ePROCESSEDINPUT value) in an
// input mode (0 soldier, 1 vehicle, 2 flyer, 3 jedi, 4 turret), or -1.
const int kModeCount = 5;
const int kActionCount = 43;
int DefaultSlot(int iMode, int iAction);

// True when the player's bindings for a mode already use joystick inputs, in
// which case their own layout wins over the defaults. pKeys is
// ActionKey[kActionCount] viewed as its three shorts.
bool HasPadBindings(const unsigned short (*pKeys)[3]);

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
