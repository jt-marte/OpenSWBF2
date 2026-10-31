#pragma once

#include "pcXInputPad.h"

// DualSense and DualShock 4 pads read straight from their HID devices, over
// USB or Bluetooth (pcSonyPad has the report formats). Each slot backs one
// RedJoystick port, which then plays exactly like an XInput pad.
namespace SonyHid
{

// iSlots slots (ports given to Sony pads). Reads Steam Input's ignore list.
void Init(int iSlots);

// Once a frame, before the joysticks: drains each open pad's input reports
// and looks for new pads when the device list changes.
void Update(float fDeltaTime);

// The slot's latest pad state; false while no pad is open in it.
bool GetState(int iSlot, XInputPad::Pad& rPad);

// XInput motor speeds to the slot's pad. False if it can't go out now (no
// pad, or the previous report is still being written): try again next frame.
bool SetRumble(int iSlot, unsigned short usHeavy, unsigned short usLight);

// Sends any last rumble report, then closes every pad.
void Shutdown();

}
