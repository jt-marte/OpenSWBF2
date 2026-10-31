#pragma once

#include "pcXInputPad.h"

// Pure DualSense / DualShock 4 report logic shared by the game and the tests.
// An input report read from the pad's HID device becomes the same
// XINPUT_GAMEPAD-equivalent XInputPad::Pad an Xbox pad gives, so a Sony pad
// plays through the XInput mapping unchanged; XInput motor speeds become the
// pad's rumble output report. No Windows headers: the device side (SetupAPI,
// hid.dll, overlapped reads and writes) is pcSonyHid.cpp.
namespace SonyPad
{

const unsigned short kVidSony = 0x054C;
const unsigned short kPidDualSense = 0x0CE6;
const unsigned short kPidDualSenseEdge = 0x0DF2;
const unsigned short kPidDualShock4 = 0x05C4;
const unsigned short kPidDualShock4v2 = 0x09CC;

// Sony pads get ports after the XInput and DirectInput ones, while ports last.
const int kMaxPads = 4;

enum Model
{
    kModelNone = 0,
    kModelDualShock4 = 1,
    kModelDualSense = 2
};
Model ModelFor(unsigned short usVid, unsigned short usPid);
const char* ModelName(Model eModel);

// VID/PID from a HID device path: USB "VID_054C&PID_0CE6", or Bluetooth
// "VID&0002054C_PID&0CE6" (the first four digits are the vendor ID source).
bool ParseHidPathIds(const char* pcPath, unsigned short& usVid, unsigned short& usPid);

// Report sizes, report ID included.
const unsigned int kUsbInputSize = 64;
const unsigned int kSimpleInputSize = 10;
const unsigned int kBtReportSize = 78;
const unsigned int kDs4UsbOutputSize = 32;
const unsigned int kDualSenseUsbOutputSize = 48;

// One input report as ReadFile returns it (report ID first; Windows pads it
// to the device's InputReportByteLength) to the pad state. Takes:
//   DualSense USB 0x01 (64 bytes) and Bluetooth 0x31 (78, CRC checked);
//   DualShock 4 USB 0x01 and Bluetooth 0x11 (78, CRC checked);
//   either model's Bluetooth simple-mode 0x01 (10 bytes, or 78 padded), sent
//   until the pad is switched to full reports.
// False for anything else (feature or audio reports, short reads, bad CRC):
// keep the last state.
bool ParseReport(Model eModel, const unsigned char* pucData, unsigned int uiLength, XInputPad::Pad& rPad);

// Stick byte (0..255, 128 centre, down is 255 on Y) to an XInput axis
// (-32768..32767, up positive on Y when bInvert).
short StickValue(unsigned char ucRaw, bool bInvert);

// CRC-32 (zlib) of a seed byte followed by the data. Bluetooth reports carry
// it in their last four bytes, over the HID header byte (input 0xA1, output
// 0xA2) and the report.
const unsigned char kCrcSeedInput = 0xA1;
const unsigned char kCrcSeedOutput = 0xA2;
unsigned int Crc32(unsigned char ucSeed, const unsigned char* pucData, unsigned int uiLength);

// The DualSense's own rumble emulation changed with firmware 2.24: older pads
// take the "compatible" mode (at half strength, to match an Xbox pad's feel),
// newer ones and the Edge the improved mode. The firmware version is feature
// report 0x20, bytes 44-45; an unreadable one counts as new, as in SDL.
const unsigned short kFirmwareEnhancedRumble = 0x0224;
bool ParseFirmwareVersion(const unsigned char* pucFeature, unsigned int uiLength, unsigned short& rusVersion);
bool UseEnhancedRumble(unsigned short usPid, bool bFirmwareKnown, unsigned short usFirmware);

// Rumble output report for XInput motor speeds (heavy: left, low-frequency
// motor; light: right). Only the motor fields are marked valid, so the
// lightbar and player LEDs are left as they are. Written into pucOut, zero
// padded to uiSize (Windows writes a whole OutputReportByteLength); returns
// the report's own length, or 0 if uiSize is too small. ucSequence (0..15)
// numbers Bluetooth DualSense reports; the caller bumps it per report.
// DualShock 4: USB 0x05 (32 bytes), Bluetooth 0x11 (78).
// DualSense: USB 0x02 (48 bytes), Bluetooth 0x31 (78).
unsigned int BuildRumbleReport(Model eModel, bool bBluetooth, bool bEnhancedRumble, unsigned char ucSequence,
                               unsigned short usHeavy, unsigned short usLight, unsigned char* pucOut,
                               unsigned int uiSize);

// A VID/PID list as Steam passes it to games in SDL_GAMECONTROLLER_IGNORE_DEVICES
// ("0x054C/0x0CE6,0x045E/0x028E"): the pads Steam Input already turns into a
// virtual XInput pad. True if usVid/usPid is in it.
bool IdListHas(const char* pcList, unsigned short usVid, unsigned short usPid);

// Display.ini NoSonyPad=1 / -nosonypad (and -noxinput): Sony pads stay on the
// plain DirectInput path, for players already using Steam Input or DS4Windows.
extern bool gDisabled;

}
