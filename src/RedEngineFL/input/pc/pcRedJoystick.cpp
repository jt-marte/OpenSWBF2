#include <math.h>
#include <string.h>

#include "pcRedInputManager.h"
#include "pcXInputPad.h"
#include "RedEngineFL/Interface/RedInterfaceScreen.h"
#include "RedEngineFL/Util/RedLog.h"

#include <XInput.h>

extern HWND hGlobalWnd;

static_assert(XInputPad::kPadA == XINPUT_GAMEPAD_A && XInputPad::kPadY == XINPUT_GAMEPAD_Y &&
              XInputPad::kPadDPadUp == XINPUT_GAMEPAD_DPAD_UP &&
              XInputPad::kPadRightShoulder == XINPUT_GAMEPAD_RIGHT_SHOULDER &&
              XInputPad::kTriggerThreshold == XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
              "XInputPad mirrors XINPUT_GAMEPAD");

typedef DWORD(WINAPI* XInputGetStateFn)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD(WINAPI* XInputSetStateFn)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

static XInputGetStateFn sXInputGetState;
static XInputSetStateFn sXInputSetState;

// Loaded at runtime so the exe still starts where no XInput DLL exists; newest first.
bool RedInputManager::pcLoadXInput()
{
    static bool sbTried;
    if (sbTried)
    {
        return sXInputGetState != 0;
    }
    sbTried = true;

    static const char* const kDlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (int i = 0; i < (int)(sizeof(kDlls) / sizeof(kDlls[0])); ++i)
    {
        HMODULE hDll = LoadLibraryA(kDlls[i]);
        if (!hDll)
        {
            continue;
        }
        sXInputGetState = (XInputGetStateFn)GetProcAddress(hDll, "XInputGetState");
        sXInputSetState = (XInputSetStateFn)GetProcAddress(hDll, "XInputSetState");
        if (sXInputGetState)
        {
            LOG(LogCore, Inform, "XInput: using %s", kDlls[i]);
            return true;
        }
        sXInputSetState = 0;
        FreeLibrary(hDll);
    }
    LOG(LogCore, Warning, "XInput: no XInput DLL found, controllers use DirectInput only");
    return false;
}

BOOL CALLBACK EnumObjectsCallback(DIDEVICEOBJECTINSTANCEA const* pObject, void* pvRef);

// FUNCTION: SWBF2 0x006c54a0
void RedJoystick::Insert()
{
    if (m_connected || IsXInput())
    {
        return;
    }
    memset(&pc_state, 0, sizeof(pc_state));
    pc_device->Unacquire();
    pc_device->SetCooperativeLevel(hGlobalWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    HRESULT result;
    do
    {
        result = pc_device->Acquire();
    } while (result == DIERR_INPUTLOST);
    m_connected = result == DI_OK;
}

// FUNCTION: SWBF2 0x006c5540
// Retail PC left these empty. The level holds until the next Update sends it,
// and Rumble::Update refreshes it every frame while an effect is running.
void RedJoystick::SetHeavyEngine(float f)
{
    m_iHeavyEngine = XInputPad::MotorSpeed(f);
}

// FUNCTION: SWBF2 0x006c5550
void RedJoystick::SetLightEngine(float f)
{
    m_iLightEngine = XInputPad::MotorSpeed(f);
}

// FUNCTION: SWBF2 0x006c5470
void RedJoystick::Init(unsigned char ucPortNum)
{

    pc_device = 0;
    m_portNum = ucPortNum;
    m_connected = false;
    mPreviousAxisX = 1000.0f;
    mPreviousAxisY = 1000.0f;

    m_xinputSlot = -1;
    m_xinputPacket = 0;
    m_xinputProbeTimer = 0.0f;
    m_iHeavyEngine = 0;
    m_iLightEngine = 0;
    m_usSentHeavy = 0;
    m_usSentLight = 0;
}

void RedJoystick::InitXInput(int iSlot)
{
    m_xinputSlot = iSlot;
    m_connected = false;
    m_xinputPacket = 0xFFFFFFFF;
    m_xinputProbeTimer = 0.0f;
    m_uiDownButtons = 0;
    m_uiPressedButtons = 0;
    m_uiReleasedButtons = 0;
    memset(m_fAxis, 0, sizeof(m_fAxis));
}

// FUNCTION: SWBF2 0x006c5a60
bool RedJoystick::pcCreateDevice(DIDEVICEINSTANCEA const* pInstance)
{

    mPreviousAxisX = 1000.0f;
    mPreviousAxisY = 1000.0f;

    IDirectInput8A* pDI = RedInputManager::pcGetDIInterface();

    if (pc_device)
    {
        return false;
    }

    if (pDI->CreateDevice(pInstance->guidInstance, &pc_device, 0) != DI_OK)
    {
        return false;
    }

    if (SUCCEEDED(pc_device->SetDataFormat(&c_dfDIJoystick2))
        && SUCCEEDED(pc_device->SetCooperativeLevel(hGlobalWnd,
                                                    DISCL_EXCLUSIVE | DISCL_FOREGROUND))
        && SUCCEEDED(pc_device->EnumObjects(EnumObjectsCallback, this, DIDFT_ALL)))
    {
        return true;
    }

    pc_device->Release();
    pc_device = 0;
    return false;
}

// FUNCTION: SWBF2 0x006c53a0
BOOL CALLBACK EnumObjectsCallback(DIDEVICEOBJECTINSTANCEA const* pObject, void* pvRef)
{
    RedJoystick* pJoystick = (RedJoystick*)pvRef;

    if (pObject->dwType & DIDFT_AXIS)
    {

        DIPROPRANGE diprg;
        diprg.diph.dwSize = sizeof(DIPROPRANGE);
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        diprg.diph.dwObj = pObject->dwType;
        diprg.diph.dwHow = DIPH_BYID;
        diprg.lMin = -16384;
        diprg.lMax = 16384;
        pJoystick->pc_device->SetProperty(DIPROP_RANGE, &diprg.diph);

        DIPROPDWORD dipdw;
        dipdw.diph.dwSize = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj = pObject->dwType;
        dipdw.diph.dwHow = DIPH_BYID;
        dipdw.dwData = (DWORD)(pJoystick->mDeadZoneScale * 16384.0f);
        pJoystick->pc_device->SetProperty(DIPROP_DEADZONE, &dipdw.diph);

        pJoystick->pc_device->SetProperty(DIPROP_RANGE, &diprg.diph);
    }

    return DIENUM_CONTINUE;
}

// FUNCTION: SWBF2 0x006c5310
RedJoystick::RedJoystick()
{
    mDeadZoneScale = 0.2f;
    m_xinputSlot = -1;
}

// FUNCTION: SWBF2 0x006c5330
RedJoystick::~RedJoystick()
{
}

namespace FLRenderer
{
void SetCursorPos(float fX, float fY);
}

// FUNCTION: SWBF2 0x006c5560
void RedJoystick::Update(float fDeltaTime)
{
    if (IsXInput())
    {
        UpdateXInput(fDeltaTime);
        return;
    }

    if (!pc_device)
    {
        return;
    }

    HRESULT hr = pc_device->Poll();
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
    {
        Remove();
        Insert();
        if (FAILED(pc_device->Poll()))
        {
            return;
        }
    }

    if (FAILED(pc_device->GetDeviceState(sizeof(DIJOYSTATE2), &pc_state)))
    {
        return;
    }

    unsigned __int64 uiOldButtons = m_uiDownButtons;
    m_uiDownButtons = 0;
    for (int i = 0; i < 32; ++i)
    {
        if (pc_state.rgbButtons[i] & 0x80)
        {
            // Retail used int 1 << i: button 32 sign-extended into every POV bit.
            m_uiDownButtons |= (unsigned __int64)1 << i;
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        int iBit = 32 + i * 4;
        switch (pc_state.rgdwPOV[i])
        {
        case 0:     m_uiDownButtons |= (unsigned __int64)1 << iBit; break;
        case 4500:  m_uiDownButtons |= ((unsigned __int64)1 << iBit) | ((unsigned __int64)1 << (iBit + 1)); break;
        case 9000:  m_uiDownButtons |= (unsigned __int64)1 << (iBit + 1); break;
        case 13500: m_uiDownButtons |= ((unsigned __int64)1 << (iBit + 1)) | ((unsigned __int64)1 << (iBit + 2)); break;
        case 18000: m_uiDownButtons |= (unsigned __int64)1 << (iBit + 2); break;
        case 22500: m_uiDownButtons |= ((unsigned __int64)1 << (iBit + 2)) | ((unsigned __int64)1 << (iBit + 3)); break;
        case 27000: m_uiDownButtons |= (unsigned __int64)1 << (iBit + 3); break;
        case 31500: m_uiDownButtons |= ((unsigned __int64)1 << iBit) | ((unsigned __int64)1 << (iBit + 3)); break;
        }
    }

    UpdateButtonEdges(uiOldButtons);

    const float fScale = 1.0f / 16384.0f;
    m_fAxis[0] = pc_state.lX * fScale;
    m_fAxis[1] = pc_state.lY * fScale;
    m_fAxis[2] = pc_state.lZ * fScale;
    m_fAxis[3] = pc_state.lRx * fScale;
    m_fAxis[4] = pc_state.lRy * fScale;
    m_fAxis[5] = pc_state.lRz * fScale;
    m_fAxis[6] = pc_state.rglSlider[0] * fScale;
    m_fAxis[7] = pc_state.rglSlider[1] * fScale;

    UpdateCursorFromStick(fDeltaTime, pc_state.lX * fScale, pc_state.lY * fScale);
}

void RedJoystick::UpdateButtonEdges(unsigned __int64 uiOldButtons)
{
    unsigned __int64 uiChanged = uiOldButtons ^ m_uiDownButtons;
    m_uiPressedButtons = m_uiDownButtons & uiChanged;
    m_uiReleasedButtons = uiOldButtons & uiChanged;
}

// A stick pushed past 0.4 takes the menu cursor (DirectInput pads: left
// stick; XInput pads: right stick).
void RedJoystick::UpdateCursorFromStick(float fDeltaTime, float fX, float fY)
{
    RedMouse* pMouse = RedInputManager::GetMouse(0);
    if (!pMouse)
    {
        return;
    }

    if (mPreviousAxisX > 10.0f)
    {
        mPreviousAxisX = fX;
    }
    if (mPreviousAxisY > 10.0f)
    {
        mPreviousAxisY = fY;
    }

    if ((fabsf(fX) > 0.4f && mPreviousAxisX != 0.0f && fabsf(fX - mPreviousAxisX) > 0.03f)
        || (fabsf(fY) > 0.4f && mPreviousAxisY != 0.0f && fabsf(fY - mPreviousAxisY) > 0.03f))
    {
        pMouse->pCursorImage = this;
    }

    if (pMouse->pCursorImage == this)
    {
        // OpenSWBF2: in UI pixels a second, so the cursor crosses the menus
        // as fast at 4K as at 1080p (RedInterfaceScreen::GetUiScale).
        const float fSpeed = XInputPad::kCursorSpeed * RedInterfaceScreen::GetUiScale();
        int iDX = (int)(fX * fDeltaTime * fSpeed);
        pMouse->m_X += iDX;
        pMouse->m_dX = iDX;
        int iDY = (int)(fY * fDeltaTime * fSpeed);
        pMouse->m_Y += iDY;
        pMouse->m_dY = iDY;

        int iX = pMouse->m_X;
        if (iX < 0)
        {
            iX = 0;
        }
        else if (iX > (int)RedInterfaceScreen::GetScreenWidth())
        {
            iX = RedInterfaceScreen::GetScreenWidth();
        }
        pMouse->m_X = iX;

        int iY = pMouse->m_Y;
        if (iY < 0)
        {
            iY = 0;
        }
        else if (iY > (int)RedInterfaceScreen::GetScreenHeight())
        {
            iY = RedInterfaceScreen::GetScreenHeight();
        }
        pMouse->m_Y = iY;

        if (HwCursor::Shown())
        {
            pMouse->SetCursorPos(iX, iY);
        }
        else
        {
            FLRenderer::SetCursorPos((float)iX, (float)iY);
        }
    }

    mPreviousAxisX = fX;
    mPreviousAxisY = fY;
    // pCursorImage is cleared when the real mouse moves.
    XInputPad::gStickCursor = pMouse->pCursorImage != 0;
}

// FUNCTION: SWBF2 0x006c5370
void RedJoystick::Cleanup()
{
    if (IsXInput())
    {
        SendRumble(true);
    }
    if (pc_device)
    {
        pc_device->Unacquire();
        pc_device->Release();
        pc_device = 0;
    }
    m_connected = false;
}

// FUNCTION: SWBF2 0x006c5510
void RedJoystick::Remove()
{
    if (IsXInput())
    {
        SendRumble(true);
    }
    if (pc_device)
    {
        pc_device->Unacquire();
        pc_device->SetCooperativeLevel(hGlobalWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
    }
    m_connected = false;
}

void RedJoystick::UpdateXInput(float fDeltaTime)
{
    // Rumble::StopAllRumble calls Update(0) mid-frame just to flush the motors;
    // re-polling there would eat this frame's pressed/released edges.
    if (fDeltaTime == 0.0f)
    {
        if (m_connected)
        {
            SendRumble(false);
        }
        return;
    }

    if (!m_connected)
    {
        // XInputGetState on an empty slot is slow, so only re-probe now and then.
        m_xinputProbeTimer -= fDeltaTime;
        if (m_xinputProbeTimer > 0.0f)
        {
            return;
        }
        m_xinputProbeTimer = 2.0f;
    }

    XINPUT_STATE state;
    memset(&state, 0, sizeof(state));
    if (!sXInputGetState || sXInputGetState((DWORD)m_xinputSlot, &state) != ERROR_SUCCESS)
    {
        if (m_connected)
        {
            LOG(LogCore, Inform, "XInput: pad %d disconnected", m_xinputSlot);
            m_connected = false;
            m_xinputProbeTimer = 2.0f;
            m_uiReleasedButtons = m_uiDownButtons;
            m_uiDownButtons = 0;
            m_uiPressedButtons = 0;
            memset(m_fAxis, 0, sizeof(m_fAxis));
            m_usSentHeavy = 0;
            m_usSentLight = 0;
        }
        return;
    }

    if (!m_connected)
    {
        LOG(LogCore, Inform, "XInput: pad %d connected", m_xinputSlot);
        m_connected = true;
    }
    m_xinputPacket = state.dwPacketNumber;

    XInputPad::Pad pad;
    pad.wButtons = state.Gamepad.wButtons;
    pad.bLeftTrigger = state.Gamepad.bLeftTrigger;
    pad.bRightTrigger = state.Gamepad.bRightTrigger;
    pad.sThumbLX = state.Gamepad.sThumbLX;
    pad.sThumbLY = state.Gamepad.sThumbLY;
    pad.sThumbRX = state.Gamepad.sThumbRX;
    pad.sThumbRY = state.Gamepad.sThumbRY;

    // XInput keeps reporting while the game is in the background; DirectInput
    // pads are DISCL_FOREGROUND, so match that and go quiet.
    const bool bFocused = GetForegroundWindow() == hGlobalWnd;
    if (!bFocused)
    {
        memset(&pad, 0, sizeof(pad));
    }

    XInputPad::Mapped mapped;
    XInputPad::Map(pad, mDeadZoneScale, mapped);

    unsigned __int64 uiOldButtons = m_uiDownButtons;
    m_uiDownButtons = mapped.uiButtons;
    UpdateButtonEdges(uiOldButtons);
    memcpy(m_fAxis, mapped.afAxis, sizeof(m_fAxis));

    SendRumble(!bFocused);

    // Console-style menus: the D-pad and left stick move the highlight (fed to
    // the GUI as xAxis/yAxis in pcFLJoystick), which hides the cursor. The
    // right stick drives the cursor for the few mouse-only PC widgets.
    if (XInputPad::IsMenuInput(m_uiPressedButtons, m_fAxis))
    {
        XInputPad::PadTookMenus();
    }
    else if (fabsf(m_fAxis[3]) > 0.4f || fabsf(m_fAxis[4]) > 0.4f)
    {
        XInputPad::MouseTookMenus();
    }
    UpdateCursorFromStick(fDeltaTime, m_fAxis[3], m_fAxis[4]);
}

void RedJoystick::SendRumble(bool bForceOff)
{
    const bool bOff = bForceOff || XInputPad::gNoRumble;
    const unsigned short usHeavy = bOff ? 0 : (unsigned short)m_iHeavyEngine;
    const unsigned short usLight = bOff ? 0 : (unsigned short)m_iLightEngine;
    m_iHeavyEngine = 0;
    m_iLightEngine = 0;

    if (!sXInputSetState || (usHeavy == m_usSentHeavy && usLight == m_usSentLight))
    {
        return;
    }

    XINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = usHeavy;
    vibration.wRightMotorSpeed = usLight;
    if (sXInputSetState((DWORD)m_xinputSlot, &vibration) == ERROR_SUCCESS)
    {
        m_usSentHeavy = usHeavy;
        m_usSentLight = usLight;
    }
}
