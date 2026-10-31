#include <windows.h>
#include <string.h>
#include <stdio.h>

#include "pcDisplaySettings.h"

#include "RedEngineFL/Util/RedLog.h"
#include "RedEngineFL/input/pc/pcXInputPad.h"
#include "pcDisplayLogic.h"

namespace DisplayCfg
{

DisplaySettings gCliDisplay;
DisplaySettings gIniDisplay;
DisplaySettings gDisplay;

static const char* KeyName(unsigned int uiKey)
{
    switch (uiKey)
    {
    case kBorderless:
        return "Borderless";
    case kAdapter:
        return "Adapter";
    case kRefresh:
        return "Refresh";
    case kVsync:
        return "Vsync";
    case kFps:
        return "Fps";
    case kNoWaitQuery:
        return "NoWaitQuery";
    case kNoDpi:
        return "NoDpi";
    case kDisplayIni:
        return "DisplayIni";
    case kAdapterName:
        return "AdapterName";
    case kSoftwareCursor:
        return "SoftwareCursor";
    case kPadLayout:
        return "PadLayout";
    case kPadLookSpeed:
        return "PadLookSpeed";
    case kHudScale:
        return "HudScale";
    case kFov:
        return "Fov";
    case kCrispFonts:
        return "CrispFonts";
    case kRenderer:
        return "Renderer";
    case kNoSonyPad:
        return "NoSonyPad";
    }
    return "?";
}

static bool ShouldWrite(const DisplaySettings& s, unsigned int uiKey)
{
    return (s.uiSet & uiKey) != 0 && (s.uiCliMask & uiKey) == 0;
}

static bool FullPath(const char* pcPath, char* pcOut)
{
    if (pcPath == 0 || pcPath[0] == '\0' || strlen(pcPath) >= MAX_PATH)
    {
        return false;
    }
    DWORD dwLen = GetFullPathNameA(pcPath, MAX_PATH, pcOut, 0);
    return dwLen > 0 && dwLen < MAX_PATH;
}

static bool PutKey(const char* pcKey, const char* pcValue, const char* pcFile)
{
    return WritePrivateProfileStringA("Display", pcKey, pcValue, pcFile) != 0;
}

void SetDefaults(DisplaySettings& kSettings)
{
    memset(&kSettings, 0, sizeof(kSettings));
    kSettings.uiVsync = 1;
    kSettings.uiFps = 80;
    kSettings.bNoWaitQuery = true;
    kSettings.uiPadLookSpeed = XInputPad::kLookSpeedDefaultLevel;
    kSettings.uiHudScale = DisplayLogic::kHudScaleDefault;
    kSettings.uiFov = DisplayLogic::kFovDefault;
    kSettings.bCrispFonts = true;
    kSettings.uiRenderer = DisplayLogic::kRendererVulkan;
}

bool ParseUInt(const char* pcText, unsigned int uiLo, unsigned int uiHi, unsigned int& uiOut)
{
    if (pcText == 0 || pcText[0] == '\0')
    {
        return false;
    }
    unsigned int uiValue = 0;
    for (const char* pc = pcText; *pc != '\0'; ++pc)
    {
        if (*pc < '0' || *pc > '9')
        {
            return false;
        }
        unsigned int uiDigit = (unsigned int)(*pc - '0');
        if (uiValue > (0xFFFFFFFFu - uiDigit) / 10)
        {
            return false;
        }
        uiValue = uiValue * 10 + uiDigit;
    }
    if (uiValue < uiLo || uiValue > uiHi)
    {
        return false;
    }
    uiOut = uiValue;
    return true;
}

bool ParseBool(const char* pcText, bool& bOut)
{
    if (pcText == 0)
    {
        return false;
    }
    if (strcmp(pcText, "1") == 0 || _stricmp(pcText, "on") == 0 || _stricmp(pcText, "true") == 0)
    {
        bOut = true;
        return true;
    }
    if (strcmp(pcText, "0") == 0 || _stricmp(pcText, "off") == 0 || _stricmp(pcText, "false") == 0)
    {
        bOut = false;
        return true;
    }
    return false;
}

bool ParseVsync(const char* pcText, unsigned int& uiOut)
{
    if (pcText == 0)
    {
        return false;
    }
    if (strcmp(pcText, "0") == 0 || _stricmp(pcText, "off") == 0)
    {
        uiOut = 0;
        return true;
    }
    if (strcmp(pcText, "1") == 0 || _stricmp(pcText, "on") == 0)
    {
        uiOut = 1;
        return true;
    }
    if (strcmp(pcText, "2") == 0 || _stricmp(pcText, "half") == 0)
    {
        uiOut = 2;
        return true;
    }
    return false;
}

bool SetFromText(DisplaySettings& kSettings, unsigned int uiKey, const char* pcValue, const char* pcSource)
{
    bool bValid = false;
    bool bBool = false;
    unsigned int uiValue = 0;
    switch (uiKey)
    {
    case kBorderless:
        bValid = ParseBool(pcValue, bBool);
        if (bValid)
        {
            kSettings.bBorderless = bBool;
        }
        break;
    case kAdapter:
        bValid = ParseUInt(pcValue, 0, kAdapterMax, uiValue);
        if (bValid)
        {
            kSettings.uiAdapter = uiValue;
        }
        break;
    case kRefresh:
        bValid = ParseUInt(pcValue, 0, kRefreshMax, uiValue);
        if (bValid)
        {
            kSettings.uiRefresh = uiValue;
        }
        break;
    case kVsync:
        bValid = ParseVsync(pcValue, uiValue);
        if (bValid)
        {
            kSettings.uiVsync = uiValue;
        }
        break;
    case kFps:
        bValid = ParseUInt(pcValue, 0, kFpsMax, uiValue);
        if (bValid)
        {
            kSettings.uiFps = uiValue;
        }
        break;
    case kNoWaitQuery:
        bValid = ParseBool(pcValue, bBool);
        if (bValid)
        {
            kSettings.bNoWaitQuery = bBool;
        }
        break;
    case kNoDpi:
        bValid = ParseBool(pcValue, bBool);
        if (bValid)
        {
            kSettings.bNoDpi = bBool;
        }
        break;
    case kSoftwareCursor:
        bValid = ParseBool(pcValue, bBool);
        if (bValid)
        {
            kSettings.bSoftwareCursor = bBool;
        }
        break;
    case kNoSonyPad:
        bValid = ParseBool(pcValue, bBool);
        if (bValid)
        {
            kSettings.bNoSonyPad = bBool;
        }
        break;
    case kPadLayout:
        if (pcValue != 0 && (_stricmp(pcValue, "modern") == 0 || strcmp(pcValue, "0") == 0))
        {
            kSettings.uiPadLayout = 0;
            bValid = true;
        }
        else if (pcValue != 0 && (_stricmp(pcValue, "legacy") == 0 || strcmp(pcValue, "1") == 0))
        {
            kSettings.uiPadLayout = 1;
            bValid = true;
        }
        break;
    case kPadLookSpeed:
        bValid = ParseUInt(pcValue, XInputPad::kLookSpeedMinLevel, XInputPad::kLookSpeedMaxLevel, uiValue);
        if (bValid)
        {
            kSettings.uiPadLookSpeed = uiValue;
        }
        break;
    case kHudScale:
        bValid = ParseUInt(pcValue, DisplayLogic::kHudScaleMin, DisplayLogic::kHudScaleMax, uiValue);
        if (bValid)
        {
            kSettings.uiHudScale = uiValue;
        }
        break;
    case kFov:
        bValid = ParseUInt(pcValue, DisplayLogic::kFovMin, DisplayLogic::kFovSavedMax, uiValue);
        if (bValid)
        {
            kSettings.uiFov = uiValue < DisplayLogic::kFovMax ? uiValue : DisplayLogic::kFovMax;
        }
        break;
    case kCrispFonts:
        bValid = ParseBool(pcValue, bBool);
        if (bValid)
        {
            kSettings.bCrispFonts = bBool;
        }
        break;
    case kRenderer:
        if (pcValue != 0 && (_stricmp(pcValue, "vulkan") == 0 || strcmp(pcValue, "0") == 0))
        {
            kSettings.uiRenderer = DisplayLogic::kRendererVulkan;
            bValid = true;
        }
        else if (pcValue != 0 && (_stricmp(pcValue, "d3d9") == 0 || strcmp(pcValue, "1") == 0))
        {
            kSettings.uiRenderer = DisplayLogic::kRendererD3D9;
            bValid = true;
        }
        break;
    case kDisplayIni:
        bValid = pcValue != 0 && pcValue[0] != '\0' && strlen(pcValue) < sizeof(kSettings.acDisplayIni);
        if (bValid)
        {
            strcpy(kSettings.acDisplayIni, pcValue);
        }
        break;
    case kAdapterName:
        bValid = pcValue != 0 && strlen(pcValue) < sizeof(kSettings.acAdapterName);
        if (bValid)
        {
            strcpy(kSettings.acAdapterName, pcValue);
        }
        break;
    }
    if (!bValid)
    {
        LOG(LogRender, Warning, "display: bad %s '%.64s' from %s, using default", KeyName(uiKey),
            pcValue ? pcValue : "", pcSource ? pcSource : "?");
        return false;
    }
    kSettings.uiSet |= uiKey;
    return true;
}

void SetFlag(DisplaySettings& kSettings, unsigned int uiKey)
{
    switch (uiKey)
    {
    case kBorderless:
        kSettings.bBorderless = true;
        break;
    case kNoWaitQuery:
        kSettings.bNoWaitQuery = true;
        break;
    case kNoDpi:
        kSettings.bNoDpi = true;
        break;
    case kSoftwareCursor:
        kSettings.bSoftwareCursor = true;
        break;
    case kNoSonyPad:
        kSettings.bNoSonyPad = true;
        break;
    default:
        return;
    }
    kSettings.uiSet |= uiKey;
}

void Merge(DisplaySettings& dst, const DisplaySettings& cli)
{
    if (cli.uiSet & kBorderless)
    {
        dst.bBorderless = cli.bBorderless;
    }
    if (cli.uiSet & kAdapter)
    {
        dst.uiAdapter = cli.uiAdapter;
    }
    if (cli.uiSet & kAdapterName)
    {
        strcpy(dst.acAdapterName, cli.acAdapterName);
    }
    if (cli.uiSet & kRefresh)
    {
        dst.uiRefresh = cli.uiRefresh;
    }
    if (cli.uiSet & kVsync)
    {
        dst.uiVsync = cli.uiVsync;
    }
    if (cli.uiSet & kFps)
    {
        dst.uiFps = cli.uiFps;
    }
    if (cli.uiSet & kNoWaitQuery)
    {
        dst.bNoWaitQuery = cli.bNoWaitQuery;
    }
    if (cli.uiSet & kNoDpi)
    {
        dst.bNoDpi = cli.bNoDpi;
    }
    if (cli.uiSet & kSoftwareCursor)
    {
        dst.bSoftwareCursor = cli.bSoftwareCursor;
    }
    if (cli.uiSet & kNoSonyPad)
    {
        dst.bNoSonyPad = cli.bNoSonyPad;
    }
    if (cli.uiSet & kPadLayout)
    {
        dst.uiPadLayout = cli.uiPadLayout;
    }
    if (cli.uiSet & kPadLookSpeed)
    {
        dst.uiPadLookSpeed = cli.uiPadLookSpeed;
    }
    if (cli.uiSet & kHudScale)
    {
        dst.uiHudScale = cli.uiHudScale;
    }
    if (cli.uiSet & kFov)
    {
        dst.uiFov = cli.uiFov;
    }
    if (cli.uiSet & kCrispFonts)
    {
        dst.bCrispFonts = cli.bCrispFonts;
    }
    if (cli.uiSet & kRenderer)
    {
        dst.uiRenderer = cli.uiRenderer;
    }
    if (cli.uiSet & kDisplayIni)
    {
        strcpy(dst.acDisplayIni, cli.acDisplayIni);
    }
    dst.uiSet |= cli.uiSet;
    dst.uiCliMask |= cli.uiSet;
}

void LoadIni(DisplaySettings& kSettings, const char* pcPath)
{
    char acFull[MAX_PATH];
    if (!FullPath(pcPath, acFull) || GetFileAttributesA(acFull) == INVALID_FILE_ATTRIBUTES)
    {
        return;
    }
    static const unsigned int auiKeys[] = {kAdapter, kAdapterName, kBorderless, kRefresh,
                                           kVsync,   kFps,         kNoWaitQuery, kNoDpi,
                                           kSoftwareCursor, kPadLayout, kPadLookSpeed, kHudScale, kFov, kCrispFonts,
                                           kRenderer, kNoSonyPad};
    for (unsigned int i = 0; i < sizeof(auiKeys) / sizeof(auiKeys[0]); ++i)
    {
        char acValue[256];
        GetPrivateProfileStringA("Display", KeyName(auiKeys[i]), "\x7f", acValue, sizeof(acValue), acFull);
        acValue[sizeof(acValue) - 1] = '\0';
        if (strcmp(acValue, "\x7f") == 0)
        {
            continue;
        }
        SetFromText(kSettings, auiKeys[i], acValue, "display.ini");
    }
}

bool SaveIni(const DisplaySettings& kSettings, const char* pcPath, bool bGoodFrame)
{
    if (!bGoodFrame)
    {
        return false;
    }
    char acFull[MAX_PATH];
    char acTemp[MAX_PATH + 8];
    if (!FullPath(pcPath, acFull) || strlen(acFull) + 4 >= sizeof(acTemp))
    {
        return false;
    }
    sprintf(acTemp, "%s.tmp", acFull);
    SetFileAttributesA(acTemp, FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(acTemp);
    CopyFileA(acFull, acTemp, FALSE);
    SetFileAttributesA(acTemp, FILE_ATTRIBUTE_NORMAL);
    bool bOk = true;
    char acValue[32];
    if (ShouldWrite(kSettings, kAdapter))
    {
        sprintf(acValue, "%u", kSettings.uiAdapter);
        bOk = PutKey("Adapter", acValue, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kAdapterName))
    {
        bOk = PutKey("AdapterName", kSettings.acAdapterName, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kBorderless))
    {
        bOk = PutKey("Borderless", kSettings.bBorderless ? "1" : "0", acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kRefresh))
    {
        sprintf(acValue, "%u", kSettings.uiRefresh);
        bOk = PutKey("Refresh", acValue, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kVsync))
    {
        sprintf(acValue, "%u", kSettings.uiVsync);
        bOk = PutKey("Vsync", acValue, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kFps))
    {
        sprintf(acValue, "%u", kSettings.uiFps);
        bOk = PutKey("Fps", acValue, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kNoWaitQuery))
    {
        bOk = PutKey("NoWaitQuery", kSettings.bNoWaitQuery ? "1" : "0", acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kNoDpi))
    {
        bOk = PutKey("NoDpi", kSettings.bNoDpi ? "1" : "0", acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kSoftwareCursor))
    {
        bOk = PutKey("SoftwareCursor", kSettings.bSoftwareCursor ? "1" : "0", acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kNoSonyPad))
    {
        bOk = PutKey("NoSonyPad", kSettings.bNoSonyPad ? "1" : "0", acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kPadLayout))
    {
        bOk = PutKey("PadLayout", kSettings.uiPadLayout ? "legacy" : "modern", acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kPadLookSpeed))
    {
        sprintf(acValue, "%u", kSettings.uiPadLookSpeed);
        bOk = PutKey("PadLookSpeed", acValue, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kHudScale))
    {
        sprintf(acValue, "%u", kSettings.uiHudScale);
        bOk = PutKey("HudScale", acValue, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kFov))
    {
        sprintf(acValue, "%u", kSettings.uiFov);
        bOk = PutKey("Fov", acValue, acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kCrispFonts))
    {
        bOk = PutKey("CrispFonts", kSettings.bCrispFonts ? "1" : "0", acTemp) && bOk;
    }
    if (ShouldWrite(kSettings, kRenderer))
    {
        bOk = PutKey("Renderer", kSettings.uiRenderer == DisplayLogic::kRendererD3D9 ? "d3d9" : "vulkan", acTemp) && bOk;
    }
    WritePrivateProfileStringA(0, 0, 0, acTemp);
    if (bOk)
    {
        bOk = MoveFileExA(acTemp, acFull, MOVEFILE_REPLACE_EXISTING) != 0;
    }
    if (!bOk)
    {
        DeleteFileA(acTemp);
    }
    return bOk;
}

bool DisplayIniPath(char* pcPath, unsigned int uiSize)
{
    if (uiSize < MAX_PATH)
    {
        return false;
    }
    if (gCliDisplay.acDisplayIni[0] != '\0')
    {
        return FullPath(gCliDisplay.acDisplayIni, pcPath);
    }
    DWORD dwLen = GetModuleFileNameA(0, pcPath, MAX_PATH);
    if (dwLen > 0 && dwLen < MAX_PATH)
    {
        char* pcSlash = strrchr(pcPath, '\\');
        if (pcSlash != 0 && strlen(pcPath) + 11 < MAX_PATH)
        {
            strcpy(pcSlash + 1, "display.ini");
            return true;
        }
    }
    return false;
}

bool SaveAdapter(unsigned int uiIndex, const char* pcName)
{
    char acPath[MAX_PATH];
    if (!DisplayIniPath(acPath, sizeof(acPath)))
    {
        return false;
    }
    DisplaySettings kSave;
    SetDefaults(kSave);
    char acValue[16];
    sprintf(acValue, "%u", uiIndex);
    if (!SetFromText(kSave, kAdapter, acValue, "lua"))
    {
        return false;
    }
    if (pcName == 0 || !SetFromText(kSave, kAdapterName, pcName, "lua"))
    {
        SetFromText(kSave, kAdapterName, "", "lua");
    }

    char acDiskAdapter[16];
    char acDiskName[sizeof(kSave.acAdapterName) + 1];
    GetPrivateProfileStringA("Display", "Adapter", "\x7f", acDiskAdapter, sizeof(acDiskAdapter),
                             acPath);
    GetPrivateProfileStringA("Display", "AdapterName", "\x7f", acDiskName, sizeof(acDiskName),
                             acPath);
    const bool bSame =
        strcmp(acDiskAdapter, acValue) == 0 && strcmp(acDiskName, kSave.acAdapterName) == 0;
    if (!bSame && !SaveIni(kSave, acPath, true))
    {
        return false;
    }

    gIniDisplay.uiAdapter = kSave.uiAdapter;
    strcpy(gIniDisplay.acAdapterName, kSave.acAdapterName);
    gIniDisplay.uiSet |= kAdapter | kAdapterName;
    return true;
}

// OpenSWBF2: the controller layout chosen in Options -> Game, written on its
// own like the adapter.
bool SavePadLayout(unsigned int uiLayout)
{
    char acPath[MAX_PATH];
    if (!DisplayIniPath(acPath, sizeof(acPath)))
    {
        return false;
    }
    DisplaySettings kSave;
    SetDefaults(kSave);
    if (!SetFromText(kSave, kPadLayout, uiLayout ? "legacy" : "modern", "lua") ||
        !SaveIni(kSave, acPath, true))
    {
        return false;
    }
    gIniDisplay.uiPadLayout = kSave.uiPadLayout;
    gIniDisplay.uiSet |= kPadLayout;
    gDisplay.uiPadLayout = kSave.uiPadLayout;
    gDisplay.uiSet |= kPadLayout;
    return true;
}

// OpenSWBF2: one numeric option from the options screens, written on its own
// like the layout.
static bool SaveUIntKey(unsigned int uiKey, unsigned int uiValue, unsigned int DisplaySettings::*puiField)
{
    char acPath[MAX_PATH];
    if (!DisplayIniPath(acPath, sizeof(acPath)))
    {
        return false;
    }
    DisplaySettings kSave;
    SetDefaults(kSave);
    char acValue[16];
    sprintf(acValue, "%u", uiValue);
    if (!SetFromText(kSave, uiKey, acValue, "lua") || !SaveIni(kSave, acPath, true))
    {
        return false;
    }
    gIniDisplay.*puiField = kSave.*puiField;
    gIniDisplay.uiSet |= uiKey;
    gDisplay.*puiField = kSave.*puiField;
    gDisplay.uiSet |= uiKey;
    return true;
}

// The controller sensitivity level from Options -> Game.
bool SavePadLookSpeed(unsigned int uiLevel)
{
    return SaveUIntKey(kPadLookSpeed, uiLevel, &DisplaySettings::uiPadLookSpeed);
}

// The HUD scale and field of view from Options -> Video.
bool SaveHudScale(unsigned int uiPercent)
{
    return SaveUIntKey(kHudScale, uiPercent, &DisplaySettings::uiHudScale);
}

bool SaveFov(unsigned int uiDegrees)
{
    return SaveUIntKey(kFov, uiDegrees, &DisplaySettings::uiFov);
}

// The renderer from Options -> Video, written on its own like the layout. Only
// the ini copy changes: the running renderer was picked at startup and stays.
bool SaveRenderer(unsigned int uiRenderer)
{
    char acPath[MAX_PATH];
    if (!DisplayIniPath(acPath, sizeof(acPath)))
    {
        return false;
    }
    DisplaySettings kSave;
    SetDefaults(kSave);
    if (!SetFromText(kSave, kRenderer, uiRenderer == DisplayLogic::kRendererD3D9 ? "d3d9" : "vulkan", "lua") ||
        !SaveIni(kSave, acPath, true))
    {
        return false;
    }
    gIniDisplay.uiRenderer = kSave.uiRenderer;
    gIniDisplay.uiSet |= kRenderer;
    return true;
}

static PendingAdapter sPendingAdapter;

void SetPendingAdapter(unsigned int uiIndex, const char* pcName)
{
    sPendingAdapter.uiIndex = uiIndex;
    sPendingAdapter.acName[0] = '\0';
    if (pcName != 0)
    {
        strncpy(sPendingAdapter.acName, pcName, sizeof(sPendingAdapter.acName) - 1);
        sPendingAdapter.acName[sizeof(sPendingAdapter.acName) - 1] = '\0';
    }
    sPendingAdapter.bSet = true;
}

void ClearPendingAdapter()
{
    sPendingAdapter.bSet = false;
    sPendingAdapter.uiIndex = 0;
    sPendingAdapter.acName[0] = '\0';
}

const PendingAdapter& GetPendingAdapter()
{
    return sPendingAdapter;
}

bool ValidateMenu(const MenuFields& kFields)
{
    return kFields.uiMode <= kModeMax && kFields.uiRefresh <= kRefreshMax &&
           kFields.uiVsync <= kVsyncMax && kFields.uiFps <= kFpsMax;
}

static unsigned int MenuDiff(const DisplaySettings& kMark, const DisplaySettings& kNow)
{
    unsigned int uiMask = 0;
    if (kMark.bBorderless != kNow.bBorderless)
    {
        uiMask |= kBorderless;
    }
    if (kMark.uiRefresh != kNow.uiRefresh)
    {
        uiMask |= kRefresh;
    }
    if ((kMark.uiSet & kVsync) != (kNow.uiSet & kVsync) || kMark.uiVsync != kNow.uiVsync)
    {
        uiMask |= kVsync;
    }
    if (kMark.uiFps != kNow.uiFps)
    {
        uiMask |= kFps;
    }
    if (kMark.bNoWaitQuery != kNow.bNoWaitQuery)
    {
        uiMask |= kNoWaitQuery;
    }
    return uiMask;
}

SaveResult SaveMenu(const DisplaySettings& kMark, const DisplaySettings& kNow, const char* pcPath, bool bGoodFrame, unsigned int& uiWritten)
{
    uiWritten = 0;
    const unsigned int uiMask = MenuDiff(kMark, kNow);
    if (uiMask != 0 && !bGoodFrame)
    {
        return kSaveDeferred;
    }
    if (uiMask == 0)
    {
        return kSaveOk;
    }
    DisplaySettings kSave;
    SetDefaults(kSave);
    char acValue[16];
    bool bOk = true;
    if (uiMask & kBorderless)
    {
        bOk = SetFromText(kSave, kBorderless, kNow.bBorderless ? "1" : "0", "menu") && bOk;
    }
    if (uiMask & kRefresh)
    {
        sprintf(acValue, "%u", kNow.uiRefresh);
        bOk = SetFromText(kSave, kRefresh, acValue, "menu") && bOk;
    }
    if (uiMask & kVsync)
    {
        sprintf(acValue, "%u", kNow.uiVsync);
        bOk = SetFromText(kSave, kVsync, acValue, "menu") && bOk;
    }
    if (uiMask & kFps)
    {
        sprintf(acValue, "%u", kNow.uiFps);
        bOk = SetFromText(kSave, kFps, acValue, "menu") && bOk;
    }
    if (uiMask & kNoWaitQuery)
    {
        bOk = SetFromText(kSave, kNoWaitQuery, kNow.bNoWaitQuery ? "1" : "0", "menu") && bOk;
    }
    if (!bOk || !SaveIni(kSave, pcPath, bGoodFrame))
    {
        return kSaveFailed;
    }
    if (uiMask & kBorderless)
    {
        gIniDisplay.bBorderless = kSave.bBorderless;
    }
    if (uiMask & kRefresh)
    {
        gIniDisplay.uiRefresh = kSave.uiRefresh;
    }
    if (uiMask & kVsync)
    {
        gIniDisplay.uiVsync = kSave.uiVsync;
    }
    if (uiMask & kFps)
    {
        gIniDisplay.uiFps = kSave.uiFps;
    }
    if (uiMask & kNoWaitQuery)
    {
        gIniDisplay.bNoWaitQuery = kSave.bNoWaitQuery;
    }
    gIniDisplay.uiSet |= uiMask;
    uiWritten = uiMask;
    return kSaveOk;
}

void Init(bool bCliWindowed)
{
    SetDefaults(gIniDisplay);
    char acPath[MAX_PATH];
    if (DisplayIniPath(acPath, sizeof(acPath)))
    {
        LoadIni(gIniDisplay, acPath);
    }
    gDisplay = gIniDisplay;
    Merge(gDisplay, gCliDisplay);
    // OpenSWBF2: borderless unless display.ini or the command line chose, or -win asked for a
    // window. Picking Fullscreen or Windowed in the options then saves Borderless=0.
    if (!((gIniDisplay.uiSet | gCliDisplay.uiSet) & kBorderless))
    {
        gDisplay.bBorderless = !bCliWindowed;
    }
}

}
