#include <stdio.h>
#include <string.h>

#include "LuaCallbacks_PC.h"
#include "LuaHelper.h"
#include "LuaVideoArgs.h"
#include "FLGuiManager.h"
#include "FLInputManager.h"
#include "PlayerProfile.h"
#include "ColorControl.h"
#include "LuaFL/include/lauxlib.h"
#include "PebbleFL/Common/PblHashTable.h"
#include "PebbleFL/Common/PblUnicode.h"
#include "RedEngineFL/Graphics/PC/pcRedAutoSettings.h"
#include "RedEngineFL/Graphics/PC/pcRedRenderer.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"
#include "RedEngineFL/Util/RedLocalizeDB.h"
#include "RedEngineFL/Util/RedLog.h"

#include "RedEngineFL/input/RedKeyboard.h"
#include "RedEngineFL/Interface/RedHotSpot.h"
#include "RedEngineFL/Interface/RedInterfaceElement.h"
#include "RedEngineFL/Interface/RedInterfaceScreen.h"
#include "RedEngineFL/input/pc/pcXInputPad.h"
#include "RedEngineFL/Application/pc/pcFLRender.h"

// GLOBAL: SWBF2 0x01e5794c
int markMouseX;
// GLOBAL: SWBF2 0x01e57964
int markMouseY;

namespace Lua_Callbacks
{

// FUNCTION: SWBF2 0x00599580
int ScriptCB_SetReticuleTransparency(lua_State*)
{
    luaL_checknumber(LuaHelper::mState, 1);
    return 0;
}

// FUNCTION: SWBF2 0x005995a0
int ScriptCB_GetReticuleTransparency(lua_State*)
{
    lua_pushnumber(LuaHelper::mState, 1.0f);
    return 1;
}

// FUNCTION: SWBF2 0x0059b1b0
int ScriptCB_SetMouseMark(lua_State*)
{
    const MouseCoords& coords = FLInputManager::s_instance.GetPlayerGuiInputs(0)->mMouseCoords;
    markMouseX = coords.mPosX;
    markMouseY = coords.mPosY;
    return 0;
}

// FUNCTION: SWBF2 0x0059b1e0
int ScriptCB_ResetMouseMark(lua_State*)
{
    markMouseX = -1;
    markMouseY = -1;
    return 0;
}

// FUNCTION: SWBF2 0x0059b200
int ScriptCB_CheckMouseMark(lua_State*)
{
    const MouseCoords& coords = FLInputManager::s_instance.GetPlayerGuiInputs(0)->mMouseCoords;
    lua_pushboolean(LuaHelper::mState,
                    markMouseX != coords.mPosX || markMouseY != coords.mPosY);
    return 1;
}

// FUNCTION: SWBF2 0x00599330
int ScriptCB_SetMouseSensitivity(lua_State*)
{
    lua_gettop(LuaHelper::mState);
    RawControllerInputs* pJoystick = FLInputManager::s_instance.GetPlayerJoystick(
        GuiManager::GetJoystickFromViewport(GuiManager::mCurViewport));
    float fSensitivity = luaL_checknumber(LuaHelper::mState, 1);
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile && pProfile->mGameOptions.mMouseSensitivity[pJoystick->GetInputMode()] != fSensitivity)
    {
        pProfile->mGameOptions.mMouseSensitivity[pJoystick->GetInputMode()] = fSensitivity;
        pProfile->UpdateProfCRC();
        pProfile->ApplySettings();
    }
    return 0;
}

// FUNCTION: SWBF2 0x005993c0
int ScriptCB_SetJoySensitivity(lua_State*)
{
    lua_gettop(LuaHelper::mState);
    int iSensitivity = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iAxis = (int)luaL_checknumber(LuaHelper::mState, 2);
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile)
    {
        RawControllerInputs* pJoystick = FLInputManager::s_instance.GetPlayerJoystick(
            GuiManager::GetJoystickFromViewport(GuiManager::mCurViewport));
        unsigned char* pValue = 0;
        if (iAxis == 0)
        {
            pValue = &pProfile->mGameOptions.mJoystickSensitivityX[pJoystick->GetInputMode()];
        }
        if (iAxis == 1)
        {
            pValue = &pProfile->mGameOptions.mJoystickSensitivityY[pJoystick->GetInputMode()];
        }
        if (pValue && *pValue != (iAxis == 1 ? iSensitivity : (unsigned char)iSensitivity))
        {
            *pValue = (unsigned char)iSensitivity;
            pProfile->UpdateProfCRC();
            pProfile->ApplySettings();
        }
    }
    return 0;
}

// FUNCTION: SWBF2 0x00599470
int ScriptCB_SetDeadZone(lua_State*)
{
    lua_gettop(LuaHelper::mState);
    unsigned char iDeadZone = (unsigned char)(int)luaL_checknumber(LuaHelper::mState, 1);
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile && pProfile->mGameOptions.mDeadZone != iDeadZone)
    {
        pProfile->mGameOptions.mDeadZone = iDeadZone;
        pProfile->UpdateProfCRC();
        pProfile->ApplySettings();
    }
    return 0;
}

// FUNCTION: SWBF2 0x005994d0
int ScriptCB_IsJoystickEnabled(lua_State*)
{
    lua_gettop(LuaHelper::mState);
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    lua_pushboolean(LuaHelper::mState, pProfile ? pProfile->mPCOptions.mJoyEnabled : true);
    return 1;
}

// FUNCTION: SWBF2 0x00599510
int ScriptCB_SetJoystickEnabled(lua_State*)
{
    lua_gettop(LuaHelper::mState);
    bool bEnabled = lua_isnumber(LuaHelper::mState, 1) || lua_isstring(LuaHelper::mState, 1);
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile && pProfile->mPCOptions.mJoyEnabled != bEnabled)
    {
        pProfile->mPCOptions.mJoyEnabled = bEnabled;
        pProfile->UpdateProfCRC();
        pProfile->ApplySettings();
    }
    return 0;
}

// FUNCTION: SWBF2 0x0059abb0
int ScriptCB_GetProfileJoinIP(lua_State*)
{
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    lua_pushstring(LuaHelper::mState, pProfile ? pProfile->mPCOptions.mJoinIp : "");
    return 1;
}

// FUNCTION: SWBF2 0x0059abe0
int ScriptCB_SetProfileJoinIP(lua_State*)
{
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile && lua_gettop(LuaHelper::mState) == 1 && lua_isstring(LuaHelper::mState, 1))
    {
        const char* pcIp = luaL_checklstring(LuaHelper::mState, 1, 0);
        memset(pProfile->mPCOptions.mJoinIp, 0, sizeof(pProfile->mPCOptions.mJoinIp));
        strlcpy(pProfile->mPCOptions.mJoinIp, pcIp, sizeof(pProfile->mPCOptions.mJoinIp));
        pProfile->UpdateProfCRC();
    }
    return 0;
}

// FUNCTION: SWBF2 0x0059ac50
int ScriptCB_ResetPCVideoOptionsToDefault(lua_State*)
{
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile)
    {
        pProfile->mPCOptions.ResetToDefault();
        pProfile->ApplySettings();
        pProfile->UpdateProfCRC();
    }
    return 0;
}

// FUNCTION: SWBF2 0x00599bd0
int ScriptCB_GetIdealResolution(lua_State*)
{
    int iWidth, iHeight;
    RedAutoSettings::GetIdealResolution(iWidth, iHeight);
    lua_pushnumber(LuaHelper::mState, (float)iWidth);
    lua_pushnumber(LuaHelper::mState, (float)iHeight);
    return 2;
}

// FUNCTION: SWBF2 0x00599c30
int ScriptCB_GetCustomResolution(lua_State*)
{
    int iPreset = 0;
    if (lua_gettop(LuaHelper::mState) == 1)
    {
        iPreset = (int)luaL_checknumber(LuaHelper::mState, 1) - 1;
    }
    int iWidth, iHeight;
    RedAutoSettings::GetCustomPresetResolution(iPreset, iWidth, iHeight);
    lua_pushnumber(LuaHelper::mState, (float)iWidth);
    lua_pushnumber(LuaHelper::mState, (float)iHeight);
    return 2;
}

// FUNCTION: SWBF2 0x0059ac80
int ScriptCB_IsVideoModeSupported(lua_State*)
{
    int iArgs = lua_gettop(LuaHelper::mState);
    if ((unsigned)(iArgs - 2) > 2 || !lua_isnumber(LuaHelper::mState, 1) ||
        !lua_isnumber(LuaHelper::mState, 2))
    {
        return 0;
    }
    int iWidth = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iHeight = (int)luaL_checknumber(LuaHelper::mState, 2);
    int iType = 0, iQuality = 0;
    if (iArgs > 2)
    {
        if (lua_isnumber(LuaHelper::mState, 3))
        {
            iType = (int)luaL_checknumber(LuaHelper::mState, 3);
        }
        if (iArgs > 3 && lua_isnumber(LuaHelper::mState, 4))
        {
            iQuality = (int)luaL_checknumber(LuaHelper::mState, 4);
        }
    }
    lua_pushboolean(LuaHelper::mState, RedVideoConfig::VidMode_Usable(
        iWidth, iHeight, (D3DMULTISAMPLE_TYPE)iType, iQuality));
    return 1;
}

// FUNCTION: SWBF2 0x0059ad70
int ScriptCB_IsMultisamplingModeForcedFromDriver(lua_State*)
{
    if (lua_gettop(LuaHelper::mState) > 0)
    {
        return 0;
    }
    lua_pushboolean(LuaHelper::mState, RedVideoConfig::VidMode_IsDriverMultisampling());
    return 1;
}

// FUNCTION: SWBF2 0x0059adb0
int ScriptCB_IsMultisamplingModeSupported(lua_State*)
{
    int iArgs = lua_gettop(LuaHelper::mState);
    if ((unsigned)(iArgs - 1) > 1 || !lua_isnumber(LuaHelper::mState, 1))
    {
        return 0;
    }
    int iType = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iQuality = iArgs == 2 && lua_isnumber(LuaHelper::mState, 2)
        ? (int)luaL_checknumber(LuaHelper::mState, 2) : 0;
    lua_pushboolean(LuaHelper::mState, RedVideoConfig::VidMode_MultisamplingModeUseable(
        (D3DMULTISAMPLE_TYPE)iType, iQuality));
    return 1;
}

// FUNCTION: SWBF2 0x0059ae50
int ScriptCB_GetMaxScreenSizeForMultisamplingMode(lua_State*)
{
    int iArgs = lua_gettop(LuaHelper::mState);
    if ((unsigned)(iArgs - 1) > 1 || !lua_isnumber(LuaHelper::mState, 1))
    {
        return 0;
    }
    int iType = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iQuality = iArgs == 2 && lua_isnumber(LuaHelper::mState, 2)
        ? (int)luaL_checknumber(LuaHelper::mState, 2) : 0;
    int iWidth = 0, iHeight = 0;
    if (RedVideoConfig::VidMode_GetMaxScreenSizeForMultisamplingMode(
        (D3DMULTISAMPLE_TYPE)iType, iQuality, iWidth, iHeight))
    {
        lua_pushnumber(LuaHelper::mState, (float)iWidth);
        lua_pushnumber(LuaHelper::mState, (float)iHeight);
    }
    else { lua_pushnil(LuaHelper::mState); lua_pushnil(LuaHelper::mState); }
    return 2;
}

// FUNCTION: SWBF2 0x0059af60
int ScriptCB_GetMaxMultisamplingModeForScreenSize(lua_State*)
{
    if (lua_gettop(LuaHelper::mState) != 2 || !lua_isnumber(LuaHelper::mState, 1) ||
        !lua_isnumber(LuaHelper::mState, 2))
    {
        return 0;
    }
    int iWidth = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iHeight = (int)luaL_checknumber(LuaHelper::mState, 2);
    D3DMULTISAMPLE_TYPE eType;
    unsigned long ulQuality;
    if (RedVideoConfig::VidMode_GetMaxMultisamplingModeForScreenSize(iWidth, iHeight, eType, ulQuality))
    {
        lua_pushnumber(LuaHelper::mState, (float)eType);
        lua_pushnumber(LuaHelper::mState, (float)ulQuality);
    }
    else { lua_pushnil(LuaHelper::mState); lua_pushnil(LuaHelper::mState); }
    return 2;
}

// FUNCTION: SWBF2 0x0059b060
int ScriptCB_GetNumMultisamplingQualityModes(lua_State*)
{
    if (lua_gettop(LuaHelper::mState) > 0)
    {
        return 0;
    }
    lua_pushnumber(LuaHelper::mState, (float)RedVideoConfig::VidMode_GetNumMultiSampleQualityModes());
    return 1;
}

// FUNCTION: SWBF2 0x0059b0a0
int ScriptCB_GetMultisamplingModeDescription(lua_State*)
{
    int iArgs = lua_gettop(LuaHelper::mState);
    if ((unsigned)(iArgs - 1) > 1 || !lua_isnumber(LuaHelper::mState, 1))
    {
        return 0;
    }
    int iType = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iQuality = iArgs == 2 && lua_isnumber(LuaHelper::mState, 2)
        ? (int)luaL_checknumber(LuaHelper::mState, 2) : 0;
    const char* pcDesc = RedVideoConfig::VidMode_GetMultiSampleDescription((D3DMULTISAMPLE_TYPE)iType, iQuality);
    if (pcDesc && *pcDesc && *pcDesc != 'U')
    {
        lua_pushstring(LuaHelper::mState, pcDesc);
    }
    else
    {
        lua_pushnil(LuaHelper::mState);
    }
    return 1;
}

// FUNCTION: SWBF2 0x0059a070
int ScriptCB_GetCustomVideoOptions(lua_State*)
{
    int iPreset = 0;
    if (lua_gettop(LuaHelper::mState) == 1)
    {
        iPreset = (int)luaL_checknumber(LuaHelper::mState, 1) - 1;
    }
    const RenderOptionsT opts = *RedAutoSettings::GetCustomPreset(iPreset);
    lua_pushnumber(LuaHelper::mState, (float)(opts.mGraphicsTier + 1));
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, (float)(opts.textureDetail + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.textureQuality + 1));
    lua_pushnumber(LuaHelper::mState, opts.mFarSceneRangeFactor);
    lua_pushnumber(LuaHelper::mState, opts.mNearSceneFadeFactor);
    lua_pushnumber(LuaHelper::mState, (float)(opts.waterQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.terrainQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.particleQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.shadowQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.specularQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.mEnableHDR + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.bumpMapping + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.motionBlur + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.distortion + 1));
    return 16;
}

// FUNCTION: SWBF2 0x0059a320
int ScriptCB_GetAutodetectPCVideoOptions(lua_State*)
{
    const RenderOptionsT opts = RedAutoSettings::GetRenderOptions();
    if (ColorControl::s_pInstance)
    {
        ColorControl::s_pInstance->SetLevel((ControlT)0, opts.mGammaContrast);
        ColorControl::s_pInstance->SetLevel((ControlT)1, opts.mGammaBrightness);
        ColorControl::s_pInstance->Update(0.0f);
    }
    lua_pushnumber(LuaHelper::mState, (float)(opts.masterQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.mGraphicsTier + 1));
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, (float)(opts.textureDetail + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.textureQuality + 1));
    lua_pushnumber(LuaHelper::mState, opts.mGammaBrightness);
    lua_pushnumber(LuaHelper::mState, opts.mGammaContrast);
    lua_pushnumber(LuaHelper::mState, opts.mFarSceneRangeFactor);
    lua_pushnumber(LuaHelper::mState, opts.mNearSceneFadeFactor);
    lua_pushnumber(LuaHelper::mState, (float)(opts.waterQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.terrainQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.particleQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.shadowQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.specularQuality + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.mEnableHDR + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.bumpMapping + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.motionBlur + 1));
    lua_pushnumber(LuaHelper::mState, (float)(opts.distortion + 1));
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, 1.0f);
    return 21;
}

static int ScriptCB_EnableCursor(lua_State* L);
static int ScriptCB_SetPCDisplayAdapter(lua_State* L);
static int ScriptCB_GetDisplayOptions(lua_State* L);
static int ScriptCB_SetDisplayOptions(lua_State* L);
static int ScriptCB_PollDisplayConfirm(lua_State* L);
static int ScriptCB_EndDisplayConfirm(lua_State* L);
static int ScriptCB_MarkDisplayOptions(lua_State* L);
static int ScriptCB_RestoreDisplayOptions(lua_State* L);
static int ScriptCB_SaveDisplayOptions(lua_State* L);

// True while an XInput pad drives the menus (cursor hidden); shell screens
// then highlight their first button on entry, as on console.
static int ScriptCB_IsPadNavigating(lua_State*)
{
    lua_pushboolean(LuaHelper::mState, XInputPad::gPadOwnsCursor);
    return 1;
}

// True while a pad stick drives the cursor; the PC tabs ignore it so only the
// bumpers and triggers switch them.
static int ScriptCB_IsStickCursor(lua_State*)
{
    lua_pushboolean(LuaHelper::mState, XInputPad::gStickCursor);
    return 1;
}

// Screen position of an element's hotspot centre (nothing if it has none), so
// a pad can hop between the mouse-only PC widgets.
static int ScriptCB_IFObj_GetHotSpotCenter(lua_State*)
{
    if (lua_type(LuaHelper::mState, 1) != LUA_TLIGHTUSERDATA)
    {
        return 0;
    }
    const RedInterfaceElement* pElement = (const RedInterfaceElement*)lua_topointer(LuaHelper::mState, 1);
    float fX, fY;
    if (!pElement || !pElement->m_pHotSpot || !pElement->m_pHotSpot->GetScreenCenter(fX, fY))
    {
        return 0;
    }
    // in UI pixels, like the mouse coordinates Lua gets (GuiManager::UpdateCursor)
    const float fUiScale = RedInterfaceScreen::GetUiScale();
    lua_pushnumber(LuaHelper::mState, fX / fUiScale);
    lua_pushnumber(LuaHelper::mState, fY / fUiScale);
    return 2;
}

// Controller layout for Options -> Game: 0 Modern, 1 Legacy.
static int ScriptCB_GetPadLayout(lua_State*)
{
    lua_pushnumber(LuaHelper::mState, (float)XInputPad::gPadLayout);
    return 1;
}

// Applies at once and saves to display.ini.
static int ScriptCB_SetPadLayout(lua_State*)
{
    const int iLayout = (int)luaL_checknumber(LuaHelper::mState, 1) == XInputPad::kLayoutLegacy
                            ? XInputPad::kLayoutLegacy
                            : XInputPad::kLayoutModern;
    if (iLayout != XInputPad::gPadLayout)
    {
        XInputPad::gPadLayout = iLayout;
        if (!DisplayCfg::SavePadLayout((unsigned int)iLayout))
        {
            LOG(LogCore, Warning, "OPENSWBF2 PAD layout %d not saved", iLayout);
        }
    }
    return 0;
}

// Controller sensitivity for Options -> Game: level, step, min, max, default.
static int ScriptCB_GetPadLookSpeed(lua_State*)
{
    lua_pushnumber(LuaHelper::mState, (float)XInputPad::gPadLookLevel);
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, (float)XInputPad::kLookSpeedMinLevel);
    lua_pushnumber(LuaHelper::mState, (float)XInputPad::kLookSpeedMaxLevel);
    lua_pushnumber(LuaHelper::mState, (float)XInputPad::kLookSpeedDefaultLevel);
    return 5;
}

// Applies at once and saves to display.ini.
static int ScriptCB_SetPadLookSpeed(lua_State*)
{
    int iLevel = (int)luaL_checknumber(LuaHelper::mState, 1);
    if (iLevel < XInputPad::kLookSpeedMinLevel)
    {
        iLevel = XInputPad::kLookSpeedMinLevel;
    }
    else if (iLevel > XInputPad::kLookSpeedMaxLevel)
    {
        iLevel = XInputPad::kLookSpeedMaxLevel;
    }
    if (iLevel != XInputPad::gPadLookLevel)
    {
        XInputPad::gPadLookLevel = iLevel;
        if (!DisplayCfg::SavePadLookSpeed((unsigned int)iLevel))
        {
            LOG(LogCore, Warning, "OPENSWBF2 PAD look speed %d not saved", iLevel);
        }
    }
    return 0;
}

// GLOBAL: SWBF2 0x007e8270
static const CallbackEntryT sPCCallbacks[51] =
{
    { "ScriptCB_GetMouseSensitivity", ScriptCB_GetMouseSensitivity },
    { "ScriptCB_GetJoySensitivity", ScriptCB_GetJoySensitivity },
    { "ScriptCB_GetDeadZone", ScriptCB_GetDeadZone },
    { "ScriptCB_SetMouseSensitivity", ScriptCB_SetMouseSensitivity },
    { "ScriptCB_SetJoySensitivity", ScriptCB_SetJoySensitivity },
    { "ScriptCB_SetDeadZone", ScriptCB_SetDeadZone },
    { "ScriptCB_IsJoystickEnabled", ScriptCB_IsJoystickEnabled },
    { "ScriptCB_SetJoystickEnabled", ScriptCB_SetJoystickEnabled },
    { "ScriptCB_SetReticuleTransparency", ScriptCB_SetReticuleTransparency },
    { "ScriptCB_GetReticuleTransparency", ScriptCB_GetReticuleTransparency },
    { "ScriptCB_FillResolutionTable", ScriptCB_FillResolutionTable },
    { "ScriptCB_FillMultisampleTable", ScriptCB_FillMultisampleTable },
    { "ScriptCB_SetResolution", ScriptCB_SetResolution },
    { "ScriptCB_GetIdealResolution", ScriptCB_GetIdealResolution },
    { "ScriptCB_GetCustomResolution", ScriptCB_GetCustomResolution },
    { "ScriptCB_GetPCVideoOptions", ScriptCB_GetPCVideoOptions },
    { "ScriptCB_GetAutodetectPCVideoOptions", ScriptCB_GetAutodetectPCVideoOptions },
    { "ScriptCB_GetCustomVideoOptions", ScriptCB_GetCustomVideoOptions },
    { "ScriptCB_SetPCVideoOptions", ScriptCB_SetPCVideoOptions },
    { "ScriptCB_SetPCBrightnessContrast", ScriptCB_SetPCBrightnessContrast },
    { "ScriptCB_IsPCVideoFixedFunction", ScriptCB_IsPCVideoFixedFunction },
    { "ScriptCB_GetProfileJoinIP", ScriptCB_GetProfileJoinIP },
    { "ScriptCB_SetProfileJoinIP", ScriptCB_SetProfileJoinIP },
    { "ScriptCB_ResetPCVideoOptionsToDefault", ScriptCB_ResetPCVideoOptionsToDefault },
    { "IsVideoModeSupported", ScriptCB_IsVideoModeSupported },
    { "IsMultisamplingModeForcedFromDriver", ScriptCB_IsMultisamplingModeForcedFromDriver },
    { "IsMultisamplingModeSupported", ScriptCB_IsMultisamplingModeSupported },
    { "GetMaxScreenSizeForMultisamplingMode", ScriptCB_GetMaxScreenSizeForMultisamplingMode },
    { "GetMaxMultisamplingModeForScreenSize", ScriptCB_GetMaxMultisamplingModeForScreenSize },
    { "GetNumMultisamplingQualityModes", ScriptCB_GetNumMultisamplingQualityModes },
    { "GetMultisamplingModeDescription", ScriptCB_GetMultisamplingModeDescription },
    { "ScriptCB_SetMouseMark", ScriptCB_SetMouseMark },
    { "ScriptCB_ResetMouseMark", ScriptCB_ResetMouseMark },
    { "ScriptCB_CheckMouseMark", ScriptCB_CheckMouseMark },
    { "ScriptCB_GetKeyboardPCFlags", ScriptCB_GetKeyboardPCFlags },
    { "ScriptCB_EnableCursor", ScriptCB_EnableCursor },
    { "ScriptCB_SetPCDisplayAdapter", ScriptCB_SetPCDisplayAdapter },
    { "ScriptCB_GetDisplayOptions", ScriptCB_GetDisplayOptions },
    { "ScriptCB_SetDisplayOptions", ScriptCB_SetDisplayOptions },
    { "ScriptCB_PollDisplayConfirm", ScriptCB_PollDisplayConfirm },
    { "ScriptCB_EndDisplayConfirm", ScriptCB_EndDisplayConfirm },
    { "ScriptCB_MarkDisplayOptions", ScriptCB_MarkDisplayOptions },
    { "ScriptCB_RestoreDisplayOptions", ScriptCB_RestoreDisplayOptions },
    { "ScriptCB_SaveDisplayOptions", ScriptCB_SaveDisplayOptions },
    { "ScriptCB_IsPadNavigating", ScriptCB_IsPadNavigating },
    { "ScriptCB_IsStickCursor", ScriptCB_IsStickCursor },
    { "ScriptCB_IFObj_GetHotSpotCenter", ScriptCB_IFObj_GetHotSpotCenter },
    { "ScriptCB_GetPadLayout", ScriptCB_GetPadLayout },
    { "ScriptCB_SetPadLayout", ScriptCB_SetPadLayout },
    { "ScriptCB_GetPadLookSpeed", ScriptCB_GetPadLookSpeed },
    { "ScriptCB_SetPadLookSpeed", ScriptCB_SetPadLookSpeed },
};

// FUNCTION: SWBF2 0x00599170
int ScriptCB_GetMouseSensitivity(lua_State* L)
{
    lua_gettop(LuaHelper::mState);
    float fSensitivity = 0.0f;
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile)
    {
        int iMode = FLInputManager::s_instance.GetPlayerJoystick(
            GuiManager::GetJoystickFromViewport(GuiManager::mCurViewport))->GetInputMode();
        fSensitivity = pProfile->mGameOptions.mMouseSensitivity[iMode];
    }
    lua_pushnumber(LuaHelper::mState, fSensitivity);
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, 100.0f);
    return 3;
}

// FUNCTION: SWBF2 0x00599200
int ScriptCB_GetJoySensitivity(lua_State* L)
{
    lua_gettop(LuaHelper::mState);
    int iAxis = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iSensitivity = 0;
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile)
    {
        int iMode = FLInputManager::s_instance.GetPlayerJoystick(
            GuiManager::GetJoystickFromViewport(GuiManager::mCurViewport))->GetInputMode();
        if (iAxis == 0)
        {
            iSensitivity = pProfile->mGameOptions.mJoystickSensitivityX[iMode];
        }
        else
        {
            iSensitivity = pProfile->mGameOptions.mJoystickSensitivityY[iMode];
        }
    }
    lua_pushnumber(LuaHelper::mState, (float)iSensitivity);
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, 10.0f);
    return 3;
}

// FUNCTION: SWBF2 0x005995c0
static void AddResolution(int iIndex, unsigned short const* pusLabel, int iResX, int iResY)
{
    lua_newtable(LuaHelper::mState);
    LuaHelper::TableWriteVal("label", pusLabel);
    lua_pushstring(LuaHelper::mState, "data");
    lua_newtable(LuaHelper::mState);
    LuaHelper::TableWriteVal("resX", iResX);
    LuaHelper::TableWriteVal("resY", iResY);
    lua_rawset(LuaHelper::mState, -3);
    lua_rawseti(LuaHelper::mState, -2, iIndex);
}

// FUNCTION: SWBF2 0x00599640
static void AddMultisampleMode(int iIndex, unsigned short const* pusLabel, int iType, int iQuality)
{
    lua_newtable(LuaHelper::mState);
    LuaHelper::TableWriteVal("label", pusLabel);
    lua_pushstring(LuaHelper::mState, "data");
    lua_newtable(LuaHelper::mState);
    LuaHelper::TableWriteVal("multisampleType", iType);
    LuaHelper::TableWriteVal("multisampleQuality", iQuality);
    lua_rawset(LuaHelper::mState, -3);
    lua_rawseti(LuaHelper::mState, -2, iIndex);
}

// FUNCTION: SWBF2 0x005996c0
int ScriptCB_FillResolutionTable(lua_State* L)
{
    lua_pushnil(LuaHelper::mState);
    lua_setglobal(LuaHelper::mState, "ifs_opt_pcvideo_reslistbox_contents");
    DoPeriodicLuaGC(false);
    lua_newtable(LuaHelper::mState);
    lua_gettop(LuaHelper::mState);
    lua_gettop(LuaHelper::mState);

    int iMultiSample = lua_isnumber(LuaHelper::mState, 1) ? LuaHelper::GetInt(1) : 0;
    int iMultiSampleQuality = lua_isnumber(LuaHelper::mState, 2) ? LuaHelper::GetInt(2) : 0;

    unsigned int uiAll = RedRenderer::pcGetValidDisplayModes(gAdapter, 0, 0, 800, 600);
    DisplayLogic::DisplayModeRec* pAll = uiAll ? new DisplayLogic::DisplayModeRec[uiAll] : 0;
    if (pAll)
    {
        RedRenderer::pcGetValidDisplayModes(gAdapter, pAll, uiAll, 800, 600);
    }
    unsigned int uiNumModes = pAll ? DisplayLogic::UniqueResolutions(pAll, uiAll, 800, 600, 0, 0) : 0;
    DisplayLogic::DisplayModeRec* modes =
        uiNumModes ? new DisplayLogic::DisplayModeRec[uiNumModes] : 0;
    if (modes)
    {
        DisplayLogic::UniqueResolutions(pAll, uiAll, 800, 600, modes, uiNumModes);
    }
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();

    int iCurSel = 1;
    int iIndex = 1;
    for (unsigned int i = 0; i < uiNumModes; ++i)
    {
        unsigned short usLabel[256];
        PblUnicode::usnprintf(usLabel, 255, "%d x %d", modes[i].w, modes[i].h);
        usLabel[255] = 0;

        if (!RedVideoConfig::VidMode_Usable(modes[i].w, modes[i].h,
                                            (D3DMULTISAMPLE_TYPE)iMultiSample,
                                            iMultiSampleQuality))
        {
            continue;
        }

        AddResolution(iIndex, usLabel, modes[i].w, modes[i].h);
        if (pProfile
            && modes[i].w == pProfile->mPCOptions.mResolutionX
            && modes[i].h == pProfile->mPCOptions.mResolutionY)
        {
            iCurSel = iIndex;
        }
        ++iIndex;
    }

    delete[] modes;
    delete[] pAll;

    lua_setglobal(LuaHelper::mState, "ifs_opt_pcvideo_reslistbox_contents");
    DoPeriodicLuaGC(false);
    lua_pushnumber(LuaHelper::mState, (float)iCurSel);
    return 1;
}

// FUNCTION: SWBF2 0x005998c0
int ScriptCB_FillMultisampleTable(lua_State* L)
{
    lua_pushnil(LuaHelper::mState);
    lua_setglobal(LuaHelper::mState, "ifs_opt_pcvideo_fsaalistbox_contents");
    DoPeriodicLuaGC(false);
    lua_newtable(LuaHelper::mState);

    bool bWindowed = DisplayLogic::D3DWindowed(RedRenderer::m_displayMode);
    if (lua_isboolean(LuaHelper::mState, 3))
    {
        bWindowed = lua_toboolean(LuaHelper::mState, 3) != 0;
    }

    unsigned int auiLevels[DisplayLogic::kMultisampleTypes];
    RedRenderer::pcGetMultiSampleLevels(RedRenderer::pcGetRunningAdapter(), bWindowed, auiLevels);
    DisplayLogic::MultisampleEntry aEntries[DisplayLogic::kMultisampleTypes];
    const unsigned int uiEntries =
        DisplayLogic::MultisampleList(auiLevels, aEntries, DisplayLogic::kMultisampleTypes);

    unsigned int uiWanted = 1;
    for (unsigned int uiType = 2; uiType < DisplayLogic::kMultisampleTypes; ++uiType)
    {
        uiWanted += auiLevels[uiType] < DisplayLogic::kMultisampleTypes
                        ? auiLevels[uiType]
                        : DisplayLogic::kMultisampleTypes;
    }
    static bool sTruncLogged = false;
    if (uiWanted > DisplayLogic::kMultisampleTypes && !sTruncLogged)
    {
        sTruncLogged = true;
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY msaa list truncated wanted=%u shown=%u",
            uiWanted, uiEntries);
    }
    static int sLastEntries = -1;
    static int sLastWindowed = -1;
    if ((int)uiEntries != sLastEntries || (bWindowed ? 1 : 0) != sLastWindowed)
    {
        sLastEntries = (int)uiEntries;
        sLastWindowed = bWindowed ? 1 : 0;
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY msaa list entries=%d windowed=%d",
            sLastEntries, sLastWindowed);
    }

    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    int iCurSel = 1;
    for (unsigned int i = 0; i < uiEntries; ++i)
    {
        const int iType = (int)aEntries[i].uiType;
        const int iQuality = (int)aEntries[i].uiQuality;
        unsigned short const* pusLabel;
        unsigned short usDesc[32];
        if (iType == D3DMULTISAMPLE_NONE)
        {
            char acKey[256];
            _snprintf(acKey, 255, "common.off");
            acKey[255] = 0;
            pusLabel = RedLocalizeDB::Find(PblHash(acKey), false);
        }
        else
        {
            char acDesc[32];
            if (!DisplayLogic::MultisampleLabel(aEntries[i].uiType, aEntries[i].uiQuality, acDesc,
                                                sizeof(acDesc)))
            {
                acDesc[0] = '\0';
            }
            acDesc[sizeof(acDesc) - 1] = '\0';
            unsigned int uiLen = 0;
            while (uiLen < 31 && acDesc[uiLen] != '\0')
            {
                usDesc[uiLen] = (unsigned char)acDesc[uiLen];
                ++uiLen;
            }
            usDesc[uiLen] = 0;
            pusLabel = usDesc;
        }

        if (pProfile && iType == pProfile->mPCOptions.mMultiSample
            && (unsigned long)iQuality == pProfile->mPCOptions.mMultiSampleQuality)
        {
            iCurSel = (int)i + 1;
        }

        AddMultisampleMode((int)i + 1, pusLabel, iType, iQuality);
    }

    lua_setglobal(LuaHelper::mState, "ifs_opt_pcvideo_fsaalistbox_contents");
    DoPeriodicLuaGC(false);
    lua_pushnumber(LuaHelper::mState, (float)iCurSel);
    return 1;
}

// FUNCTION: SWBF2 0x00599b50
int ScriptCB_SetResolution(lua_State* L)
{
    lua_gettop(LuaHelper::mState);
    int iResX = (int)luaL_checknumber(LuaHelper::mState, 1);
    int iResY = (int)luaL_checknumber(LuaHelper::mState, 2);
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile
        && (pProfile->mPCOptions.mResolutionX != iResX
            || pProfile->mPCOptions.mResolutionY != iResY))
    {
        pProfile->mPCOptions.mResolutionX = (unsigned short)iResX;
        pProfile->mPCOptions.mResolutionY = (unsigned short)iResY;
        pProfile->UpdateProfCRC();
    }
    return 0;
}

// FUNCTION: SWBF2 0x00599cc0
int ScriptCB_GetPCVideoOptions(lua_State* L)
{
    int iMasterQuality = 0;
    int iUnknown0038 = 0;
    int iTextureDetail = 0;
    int iTextureQuality = 0;
    float fUnknown0018 = 0.5f;
    float fUnknown001C = 0.5f;
    float fUnknown0024 = 0.5f;
    float fUnknown0020 = 0.5f;
    int iWaterQuality = 0;
    int iTerrainQuality = 0;
    int iParticleQuality = 0;
    int iShadowQuality = 0;
    int iSpecularQuality = 0;
    int iUnknown0028 = 0;
    int iBumpMapping = 0;
    int iMotionBlur = 0;
    int iDistortion = 0;
    int iWindowed = 0;
    int iVSync = 0;

    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile)
    {
        const PlayerProfile::tPCOpt& rPC = pProfile->mPCOptions;
        const RenderOptionsT& rOpts = rPC.renderOptions;
        iMasterQuality = rOpts.masterQuality;
        iUnknown0038 = rOpts.mGraphicsTier;
        iTextureDetail = rOpts.textureDetail;
        iTextureQuality = rOpts.textureQuality;
        fUnknown0018 = rOpts.mGammaBrightness;
        fUnknown001C = rOpts.mGammaContrast;
        fUnknown0024 = rOpts.mFarSceneRangeFactor;
        fUnknown0020 = rOpts.mNearSceneFadeFactor;
        iWaterQuality = rOpts.waterQuality;
        iTerrainQuality = rOpts.terrainQuality;
        iParticleQuality = rOpts.particleQuality;
        iShadowQuality = rOpts.shadowQuality;
        iSpecularQuality = rOpts.specularQuality;
        iUnknown0028 = rOpts.mEnableHDR;
        iBumpMapping = rOpts.bumpMapping;
        iMotionBlur = rOpts.motionBlur;
        iDistortion = rOpts.distortion;
        iWindowed = rPC.mWindowed;
        iVSync = rPC.mVSync;

        if (fUnknown0018 < 0.2f)
        {
            fUnknown0018 = 0.2f;
        }
        else if (fUnknown0018 > 0.8f)
        {
            fUnknown0018 = 0.8f;
        }
        if (fUnknown001C < 0.2f)
        {
            fUnknown001C = 0.2f;
        }
        else if (fUnknown001C > 0.8f)
        {
            fUnknown001C = 0.8f;
        }
        if (fUnknown0024 < 0.0f)
        {
            fUnknown0024 = 0.0f;
        }
        else if (fUnknown0024 > 1.0f)
        {
            fUnknown0024 = 1.0f;
        }
        if (fUnknown0020 < 0.0f)
        {
            fUnknown0020 = 0.0f;
        }
        else if (fUnknown0020 > 1.0f)
        {
            fUnknown0020 = 1.0f;
        }
    }

    float afVal[LuaVideoArgs::kLegacyVideoGetValues];
    afVal[0] = (float)(iMasterQuality + 1);
    afVal[1] = (float)(iUnknown0038 + 1);
    afVal[2] = (float)(iTextureDetail + 1);
    afVal[3] = (float)(iTextureQuality + 1);
    afVal[4] = fUnknown0018;
    afVal[5] = fUnknown001C;
    afVal[6] = fUnknown0024;
    afVal[7] = fUnknown0020;
    afVal[8] = (float)(iWaterQuality + 1);
    afVal[9] = (float)(iTerrainQuality + 1);
    afVal[10] = (float)(iParticleQuality + 1);
    afVal[11] = (float)(iShadowQuality + 1);
    afVal[12] = (float)(iSpecularQuality + 1);
    afVal[13] = (float)(iUnknown0028 + 1);
    afVal[14] = (float)(iBumpMapping + 1);
    afVal[15] = (float)(iMotionBlur + 1);
    afVal[16] = (float)(iDistortion + 1);
    afVal[17] = (float)(iWindowed + 1);
    afVal[18] = (float)(iVSync + 1);
    return LuaVideoArgs::PushLegacyVideoGet(LuaHelper::mState, afVal);
}

// FUNCTION: SWBF2 0x0059a5e0
int ScriptCB_SetPCVideoOptions(lua_State* L)
{
    float afArg[LuaVideoArgs::kLegacyVideoSetArgs];
    if (!LuaVideoArgs::ReadLegacyVideoSet(LuaHelper::mState, afArg))
    {
        return 0;
    }

    RenderOptionsT opts;
    opts.masterQuality = (int)afArg[0] - 1;
    opts.mGraphicsTier = (int)afArg[1] - 1;
    opts.textureDetail = (QualityT)((int)afArg[2] - 1);
    opts.textureQuality = (TextureQualityT)((int)afArg[3] - 1);
    D3DMULTISAMPLE_TYPE eMultiSample = (D3DMULTISAMPLE_TYPE)(int)afArg[4];
    unsigned long ulMultiSampleQuality = (int)afArg[5];
    opts.waterQuality = (QualityT)((int)afArg[6] - 1);
    opts.terrainQuality = (QualityT)((int)afArg[7] - 1);
    opts.particleQuality = (QualityT)((int)afArg[8] - 1);
    opts.shadowQuality = (Quality2T)((int)afArg[9] - 1);
    opts.specularQuality = (Quality2T)((int)afArg[10] - 1);
    opts.mEnableHDR = (int)afArg[11] - 1 != 0;
    opts.bumpMapping = (int)afArg[12] - 1 != 0;
    opts.motionBlur = (int)afArg[13] - 1 != 0;
    opts.distortion = (int)afArg[14] - 1 != 0;
    bool bWindowed = (int)afArg[15] - 1 != 0;
    opts.mGammaBrightness = afArg[16];
    opts.mGammaContrast = afArg[17];
    opts.mFarSceneRangeFactor = afArg[18];
    opts.mNearSceneFadeFactor = afArg[19];
    bool bVSync = (int)afArg[20] - 1 != 0;

    if (opts.mGammaBrightness < 0.2)
    {
        opts.mGammaBrightness = 0.2;
    }
    else if (opts.mGammaBrightness > 0.80000001)
    {
        opts.mGammaBrightness = 0.80000001;
    }
    if (opts.mGammaContrast < 0.2)
    {
        opts.mGammaContrast = 0.2;
    }
    else if (opts.mGammaContrast > 0.80000001)
    {
        opts.mGammaContrast = 0.80000001;
    }
    if (opts.mFarSceneRangeFactor < 0.0f)
    {
        opts.mFarSceneRangeFactor = 0.0f;
    }
    else if (opts.mFarSceneRangeFactor > 1.0f)
    {
        opts.mFarSceneRangeFactor = 1.0f;
    }
    if (opts.mNearSceneFadeFactor < 0.0f)
    {
        opts.mNearSceneFadeFactor = 0.0f;
    }
    else if (opts.mNearSceneFadeFactor > 1.0f)
    {
        opts.mNearSceneFadeFactor = 1.0f;
    }

    RedAutoSettings::_SetGeneration(&opts);

    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (!pProfile)
    {
        return 0;
    }

    PlayerProfile::tPCOpt& rPC = pProfile->mPCOptions;
    RenderOptionsT& rOpts = rPC.renderOptions;
    if (!RedVideoConfig::VidMode_Usable(rPC.mResolutionX, rPC.mResolutionY,
                                        eMultiSample, ulMultiSampleQuality))
    {
        ulMultiSampleQuality = 0;
        eMultiSample = D3DMULTISAMPLE_NONE;
    }

    bool bChanged = false;
    if (opts.masterQuality != rOpts.masterQuality) { rOpts.masterQuality = opts.masterQuality; bChanged = true; }
    if (opts.mGraphicsTier != rOpts.mGraphicsTier) { rOpts.mGraphicsTier = opts.mGraphicsTier; bChanged = true; }
    if (opts.textureDetail != rOpts.textureDetail) { rOpts.textureDetail = opts.textureDetail; bChanged = true; }
    if (opts.textureQuality != rOpts.textureQuality) { rOpts.textureQuality = opts.textureQuality; bChanged = true; }
    if (eMultiSample != rPC.mMultiSample) { rPC.mMultiSample = eMultiSample; bChanged = true; }
    if (ulMultiSampleQuality != rPC.mMultiSampleQuality) { rPC.mMultiSampleQuality = ulMultiSampleQuality; bChanged = true; }
    if (opts.waterQuality != rOpts.waterQuality) { rOpts.waterQuality = opts.waterQuality; bChanged = true; }
    if (opts.terrainQuality != rOpts.terrainQuality) { rOpts.terrainQuality = opts.terrainQuality; bChanged = true; }
    if (opts.particleQuality != rOpts.particleQuality) { rOpts.particleQuality = opts.particleQuality; bChanged = true; }
    if (opts.shadowQuality != rOpts.shadowQuality) { rOpts.shadowQuality = opts.shadowQuality; bChanged = true; }
    if (opts.specularQuality != rOpts.specularQuality) { rOpts.specularQuality = opts.specularQuality; bChanged = true; }
    if (opts.mEnableHDR != rOpts.mEnableHDR) { rOpts.mEnableHDR = opts.mEnableHDR; bChanged = true; }
    if (opts.bumpMapping != rOpts.bumpMapping) { rOpts.bumpMapping = opts.bumpMapping; bChanged = true; }
    if (opts.motionBlur != rOpts.motionBlur) { rOpts.motionBlur = opts.motionBlur; bChanged = true; }
    if (opts.distortion != rOpts.distortion) { rOpts.distortion = opts.distortion; bChanged = true; }
    if (bWindowed != rPC.mWindowed) { rPC.mWindowed = bWindowed; bChanged = true; }
    if (bVSync != rPC.mVSync) { rPC.mVSync = bVSync; bChanged = true; }
    if (opts.mGammaBrightness != rOpts.mGammaBrightness) { rOpts.mGammaBrightness = opts.mGammaBrightness; bChanged = true; }
    if (opts.mGammaContrast != rOpts.mGammaContrast) { rOpts.mGammaContrast = opts.mGammaContrast; bChanged = true; }
    if (opts.mFarSceneRangeFactor != rOpts.mFarSceneRangeFactor) { rOpts.mFarSceneRangeFactor = opts.mFarSceneRangeFactor; bChanged = true; }
    if (opts.mNearSceneFadeFactor != rOpts.mNearSceneFadeFactor) { rOpts.mNearSceneFadeFactor = opts.mNearSceneFadeFactor; bChanged = true; }

    if (bChanged)
    {
        pProfile->UpdateProfCRC();
        rPC.ApplyPostLoadSettings();
    }
    PlayerProfile::ApplyLiveDisplay();
    return 0;
}

// FUNCTION: SWBF2 0x0059ab00
int ScriptCB_SetPCBrightnessContrast(lua_State* L)
{
    if (lua_gettop(LuaHelper::mState) == 2)
    {
        float fBrightness = luaL_checknumber(LuaHelper::mState, 1);
        float fContrast = luaL_checknumber(LuaHelper::mState, 2);
        if (ColorControl::s_pInstance)
        {
            ColorControl::s_pInstance->SetLevel(COLORCONTROL_GAMMA_CONTRAST, fContrast);
            ColorControl::s_pInstance->SetLevel(COLORCONTROL_GAMMA_BRIGHTNESS, fBrightness);
            ColorControl::s_pInstance->Update(0.0f);
        }
    }
    return 0;
}

// FUNCTION: SWBF2 0x0059ab80
int ScriptCB_IsPCVideoFixedFunction(lua_State* L)
{
    lua_pushboolean(LuaHelper::mState, RedRenderer::pcGetRenderCaps().pipeline == PIPELINE_FIXED);
    return 1;
}

// FUNCTION: SWBF2 0x0059b170
void InitPC()
{
    for (int i = 0; i < 51; ++i)
    {
        LuaHelper::Register(sPCCallbacks[i].pcName, sPCCallbacks[i].pfnCallback, 0);
    }
}

}

// FUNCTION: SWBF2 0x005992c0
int Lua_Callbacks::ScriptCB_GetDeadZone(lua_State* L)
{
    lua_gettop(LuaHelper::mState);
    int iDeadZone = 5;
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (pProfile)
    {
        iDeadZone = pProfile->mGameOptions.mDeadZone;
    }
    lua_pushnumber(LuaHelper::mState, (float)iDeadZone);
    lua_pushnumber(LuaHelper::mState, 1.0f);
    lua_pushnumber(LuaHelper::mState, 10.0f);
    return 3;
}

// FUNCTION: SWBF2 0x0059b250
int Lua_Callbacks::ScriptCB_GetKeyboardPCFlags(lua_State* L)
{
    bool bShift = false;
    bool bCtrl = false;
    RedKeyboard* pKeyboard = FLInputManager::s_instance.GetPlayerJoystick(0)->GetKeyboard();
    if (pKeyboard)
    {
        const unsigned short usFlags = pKeyboard->GetFlags();
        bShift = (usFlags & 2) != 0;
        bCtrl = (usFlags & 1) != 0;
    }
    lua_pushboolean(LuaHelper::mState, bShift);
    lua_pushboolean(LuaHelper::mState, bCtrl);
    return 2;
}

// FUNCTION: SWBF2 0x0059b2b0
int Lua_Callbacks::ScriptCB_EnableCursor(lua_State* L)
{
    bool bEnable = true;
    if (lua_gettop(LuaHelper::mState) > 0)
    {
        bEnable = lua_toboolean(LuaHelper::mState, 1) != 0;
    }
    FLRenderer::EnableCursor(bEnable);
    return 0;
}

int Lua_Callbacks::ScriptCB_SetPCDisplayAdapter(lua_State* L)
{
    if (lua_gettop(LuaHelper::mState) != 1 || !lua_isnumber(LuaHelper::mState, 1))
    {
        lua_pushboolean(LuaHelper::mState, false);
        return 1;
    }
    double dIndex = (double)lua_tonumber(LuaHelper::mState, 1);
    unsigned int uiIndex = 0xFFFFFFFFu;
    if (dIndex >= 0.0 && dIndex < 4096.0 && dIndex == (double)(unsigned int)dIndex)
    {
        uiIndex = (unsigned int)dIndex;
    }
    lua_pushboolean(LuaHelper::mState, RedRenderer::pcRequestAdapter(uiIndex));
    return 1;
}

extern bool pcWindowed;

static bool ReadMenuUInt(int iTable, const char* pcKey, unsigned int uiMax, unsigned int& uiOut)
{
    lua_pushstring(LuaHelper::mState, pcKey);
    lua_gettable(LuaHelper::mState, iTable);
    bool bOk = false;
    unsigned int uiValue = 0;
    if (lua_isnumber(LuaHelper::mState, -1))
    {
        double dValue = (double)lua_tonumber(LuaHelper::mState, -1);
        if (dValue == dValue && dValue >= 0.0 && dValue <= (double)uiMax
            && dValue == (double)(unsigned int)dValue)
        {
            uiValue = (unsigned int)dValue;
            bOk = true;
        }
    }
    lua_pop(LuaHelper::mState, 1);
    if (bOk)
    {
        uiOut = uiValue;
    }
    return bOk;
}

static bool ReadMenuSize(int iIndex, unsigned int& uiOut)
{
    double dValue = (double)lua_tonumber(LuaHelper::mState, iIndex);
    if (dValue >= 1.0 && dValue <= 16384.0 && dValue == (double)(unsigned int)dValue)
    {
        uiOut = (unsigned int)dValue;
        return true;
    }
    return false;
}

static void AddDisplayEntry(int iIndex, const unsigned short* pusLabel, int iData)
{
    lua_newtable(LuaHelper::mState);
    LuaHelper::TableWriteVal("label", pusLabel);
    LuaHelper::TableWriteVal("data", iData);
    lua_rawseti(LuaHelper::mState, -2, iIndex);
}

int Lua_Callbacks::ScriptCB_GetDisplayOptions(lua_State* L)
{
    static bool sBuiltLogged = false;
    if (!sBuiltLogged)
    {
        sBuiltLogged = true;
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menu options built adapters=%u",
            RedRenderer::pcGetAdapterCount());
    }

    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (!pProfile)
    {
        return 0;
    }
    const PlayerProfile::tPCOpt& rPC = pProfile->mPCOptions;
    const DisplayCfg::DisplaySettings& rDisplay = DisplayCfg::gDisplay;

    unsigned int uiResX = rPC.mResolutionX;
    unsigned int uiResY = rPC.mResolutionY;
    if (lua_gettop(LuaHelper::mState) == 2 && lua_isnumber(LuaHelper::mState, 1)
        && lua_isnumber(LuaHelper::mState, 2))
    {
        unsigned int uiArgX = 0;
        unsigned int uiArgY = 0;
        if (ReadMenuSize(1, uiArgX) && ReadMenuSize(2, uiArgY))
        {
            uiResX = uiArgX;
            uiResY = uiArgY;
        }
    }

    const unsigned int uiRunning = RedRenderer::pcGetRunningAdapter();
    unsigned int uiNextAdapter = uiRunning;
    if (DisplayCfg::gCliDisplay.uiSet & DisplayCfg::kAdapter)
    {
        uiNextAdapter = DisplayCfg::gCliDisplay.uiAdapter;
    }
    else if (DisplayCfg::GetPendingAdapter().bSet)
    {
        uiNextAdapter = DisplayCfg::GetPendingAdapter().uiIndex;
    }
    else if (DisplayCfg::gIniDisplay.uiSet & DisplayCfg::kAdapter)
    {
        uiNextAdapter = DisplayCfg::gIniDisplay.uiAdapter;
    }

    const DisplayLogic::DisplayMode eMode =
        DisplayLogic::ResolveMode(rDisplay.bBorderless, pcWindowed, rPC.mWindowed != 0);
    const unsigned int uiVsync = DisplayLogic::EffectiveVsync(
        (rDisplay.uiSet & DisplayCfg::kVsync) != 0, rDisplay.uiVsync, rPC.mVSync != 0);

    lua_newtable(LuaHelper::mState);
    LuaHelper::TableWriteVal("mode", (int)eMode + 1);
    LuaHelper::TableWriteVal("adapter", (int)uiNextAdapter + 1);
    LuaHelper::TableWriteVal("running", (int)uiRunning + 1);
    LuaHelper::TableWriteVal("clirefresh", (rDisplay.uiCliMask & DisplayCfg::kRefresh) ? (int)rDisplay.uiRefresh : 0);
    LuaHelper::TableWriteVal("vsync", (int)uiVsync + 1);
    LuaHelper::TableWriteVal("fps", (int)rDisplay.uiFps);
    LuaHelper::TableWriteVal("futureframes", rDisplay.bNoWaitQuery ? 2 : 1);
    LuaHelper::TableWriteVal("cliMask", (int)rDisplay.uiCliMask);

    unsigned short usLabel[256];

    lua_pushstring(LuaHelper::mState, "adapters");
    lua_newtable(LuaHelper::mState);
    const unsigned int uiAdapters = RedRenderer::pcGetAdapterCount();
    if (uiAdapters == 0)
    {
        PblUnicode::usnprintf(usLabel, 255, "Monitor 1");
        usLabel[255] = 0;
        AddDisplayEntry(1, usLabel, 0);
    }
    for (unsigned int i = 0; i < uiAdapters; ++i)
    {
        char acName[128];
        if (!RedRenderer::pcGetAdapterLabel(i, acName, sizeof(acName)))
        {
            _snprintf(acName, sizeof(acName), "Monitor %d", (int)i + 1);
        }
        acName[sizeof(acName) - 1] = '\0';
        unsigned int uiLen = 0;
        while (uiLen < 255 && acName[uiLen] != '\0')
        {
            usLabel[uiLen] = (unsigned char)acName[uiLen];
            ++uiLen;
        }
        usLabel[uiLen] = 0;
        AddDisplayEntry((int)i + 1, usLabel, (int)i);
    }
    lua_rawset(LuaHelper::mState, -3);

    unsigned int auiRates[64];
    unsigned int uiRates = 0;
    unsigned int uiAll = RedRenderer::pcGetValidDisplayModes(uiRunning, 0, 0, 0, 0);
    DisplayLogic::DisplayModeRec* pAll = uiAll ? new DisplayLogic::DisplayModeRec[uiAll] : 0;
    if (pAll)
    {
        unsigned int uiFilled = RedRenderer::pcGetValidDisplayModes(uiRunning, pAll, uiAll, 0, 0);
        if (uiFilled < uiAll)
        {
            uiAll = uiFilled;
        }
        uiRates = DisplayLogic::RefreshRates(pAll, uiAll, uiResX, uiResY, auiRates, 64);
    }
    delete[] pAll;
    if (uiRates > 64)
    {
        uiRates = 64;
    }

    lua_pushstring(LuaHelper::mState, "refreshes");
    lua_newtable(LuaHelper::mState);
    PblUnicode::usnprintf(usLabel, 255, "Default");
    usLabel[255] = 0;
    AddDisplayEntry(1, usLabel, 0);
    for (unsigned int i = 0; i < uiRates; ++i)
    {
        PblUnicode::usnprintf(usLabel, 255, "%d Hz", (int)auiRates[i]);
        usLabel[255] = 0;
        AddDisplayEntry((int)i + 2, usLabel, (int)auiRates[i]);
    }
    lua_rawset(LuaHelper::mState, -3);

    unsigned int auiCaps[16];
    unsigned int uiCaps = DisplayLogic::FrameCapList(rDisplay.uiFps, auiCaps, 16);
    if (uiCaps > 16)
    {
        uiCaps = 16;
    }

    lua_pushstring(LuaHelper::mState, "fpscaps");
    lua_newtable(LuaHelper::mState);
    for (unsigned int i = 0; i < uiCaps; ++i)
    {
        if (auiCaps[i] == 0)
        {
            PblUnicode::usnprintf(usLabel, 255, "Off");
        }
        else
        {
            PblUnicode::usnprintf(usLabel, 255, "%d", (int)auiCaps[i]);
        }
        usLabel[255] = 0;
        AddDisplayEntry((int)i + 1, usLabel, (int)auiCaps[i]);
    }
    lua_rawset(LuaHelper::mState, -3);

    return 1;
}

int Lua_Callbacks::ScriptCB_SetDisplayOptions(lua_State* L)
{
    bool bOk = lua_gettop(LuaHelper::mState) == 1 && lua_istable(LuaHelper::mState, 1);
    unsigned int uiMode = 0;
    unsigned int uiRefresh = 0;
    unsigned int uiVsync = 0;
    unsigned int uiFps = 0;
    unsigned int uiFutureFrames = 0;
    bOk = bOk && ReadMenuUInt(1, "mode", DisplayCfg::kModeMax + 1, uiMode) && uiMode >= 1;
    bOk = bOk && ReadMenuUInt(1, "refresh", DisplayCfg::kRefreshMax, uiRefresh);
    bOk = bOk && ReadMenuUInt(1, "vsync", DisplayCfg::kVsyncMax + 1, uiVsync) && uiVsync >= 1;
    bOk = bOk && ReadMenuUInt(1, "fps", DisplayCfg::kFpsMax, uiFps);
    bOk = bOk && ReadMenuUInt(1, "futureframes", 2, uiFutureFrames) && uiFutureFrames >= 1;

    DisplayCfg::MenuFields kFields;
    kFields.uiMode = bOk ? uiMode - 1 : 0;
    kFields.uiRefresh = uiRefresh;
    kFields.uiVsync = bOk ? uiVsync - 1 : 0;
    kFields.uiFps = uiFps;
    kFields.bWaitQuery = uiFutureFrames == 1;
    bOk = bOk && DisplayCfg::ValidateMenu(kFields);

    if (!bOk)
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY menu options rejected");
        lua_pushboolean(LuaHelper::mState, false);
        return 1;
    }
    lua_pushboolean(LuaHelper::mState, PlayerProfile::ApplyMenuDisplay(kFields));
    return 1;
}

int Lua_Callbacks::ScriptCB_PollDisplayConfirm(lua_State* L)
{
    unsigned int uiState = 0;
    unsigned int uiSecondsLeft = 0;
    unsigned int uiResult = 0;
    unsigned int uiKeepSave = 0;
    PlayerProfile::PollDisplayConfirm(uiState, uiSecondsLeft, uiResult, uiKeepSave);
    lua_pushnumber(LuaHelper::mState, (float)uiState);
    lua_pushnumber(LuaHelper::mState, (float)uiSecondsLeft);
    lua_pushnumber(LuaHelper::mState, (float)uiResult);
    lua_pushnumber(LuaHelper::mState, (float)uiKeepSave);
    return 4;
}

int Lua_Callbacks::ScriptCB_EndDisplayConfirm(lua_State* L)
{
    if (lua_gettop(LuaHelper::mState) != 1 || !lua_isboolean(LuaHelper::mState, 1))
    {
        return 0;
    }
    PlayerProfile::EndDisplayConfirm(lua_toboolean(LuaHelper::mState, 1) != 0);
    return 0;
}

int Lua_Callbacks::ScriptCB_MarkDisplayOptions(lua_State* L)
{
    PlayerProfile::MarkDisplay();
    return 0;
}

int Lua_Callbacks::ScriptCB_RestoreDisplayOptions(lua_State* L)
{
    PlayerProfile::RestoreDisplay();
    return 0;
}

int Lua_Callbacks::ScriptCB_SaveDisplayOptions(lua_State* L)
{
    lua_pushnumber(LuaHelper::mState, (float)PlayerProfile::SaveDisplay());
    return 1;
}
