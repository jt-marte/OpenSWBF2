#pragma once

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <stddef.h>

// VTABLE: SWBF2 0x00772b94
class RedJoystick
{
public:

    RedJoystick();

    virtual ~RedJoystick();

    // SYNTHETIC: SWBF2 0x006c5340
    // RedJoystick::`scalar deleting destructor'

    void Init(unsigned char ucPortNum);
    bool pcCreateDevice(DIDEVICEINSTANCEA const* pInstance);

    void SetDeadZoneScale(float fScale);

    void Insert();
    void Remove();
    void Cleanup();
    void Update(float fDeltaTime);
    void SetHeavyEngine(float f);
    void SetLightEngine(float f);

    void InitXInput(int iSlot);
    // A DualSense / DualShock 4 read over HID (SonyHid slot iSlot); it plays
    // as an XInput pad, so IsXInput() is true for it too.
    void InitSonyPad(int iSlot);
    bool IsXInput() const { return m_xinputSlot >= 0 || m_sonySlot >= 0; }
    bool IsSonyPad() const { return m_sonySlot >= 0; }

    unsigned char m_portNum;
    bool m_connected;
    float m_fAxis[8];
    float m_fAnalogButtons[48];
    int m_iHeavyEngine;
    int m_iLightEngine;
    unsigned __int64 m_uiDownButtons;
    unsigned __int64 m_uiPressedButtons;
    unsigned __int64 m_uiReleasedButtons;
    IDirectInputDevice8A* pc_device;
    DIJOYSTATE2 pc_state;

    float mPreviousAxisX;
    float mPreviousAxisY;

    float mDeadZoneScale;

    int m_xinputSlot;
    int m_sonySlot;
    unsigned long m_xinputPacket;
    float m_xinputProbeTimer;
    unsigned short m_usSentHeavy;
    unsigned short m_usSentLight;

private:
    void UpdateXInput(float fDeltaTime);
    void UpdateButtonEdges(unsigned __int64 uiOldButtons);
    void UpdateCursorFromStick(float fDeltaTime, float fX, float fY);
    void SendRumble(bool bForceOff);
};
 
 
 
 
 
