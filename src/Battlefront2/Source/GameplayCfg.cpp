#include "GameplayCfg.h"

#include <windows.h>
#include <string.h>
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"

namespace GameplayCfg
{

static const char kSection[] = "Gameplay";

static const struct
{
    const char* pcKey;
    bool bDefault;
} skOptions[kNumOptions] = {
    { "AiHeroes", true },
    { "AwardWeapons", true },
    { "AwardEffects", true },
};

int Find(const char* pcName)
{
    if (pcName == 0)
    {
        return -1;
    }
    for (int i = 0; i < kNumOptions; ++i)
    {
        if (_stricmp(pcName, skOptions[i].pcKey) == 0)
        {
            return i;
        }
    }
    return -1;
}

const char* KeyName(int iOption)
{
    return iOption >= 0 && iOption < kNumOptions ? skOptions[iOption].pcKey : "";
}

bool Default(int iOption)
{
    return iOption >= 0 && iOption < kNumOptions && skOptions[iOption].bDefault;
}

void SetDefaults(Settings& kSettings)
{
    for (int i = 0; i < kNumOptions; ++i)
    {
        kSettings.abValue[i] = skOptions[i].bDefault;
        kSettings.abCli[i] = false;
    }
}

void LoadIni(Settings& kSettings, const char* pcPath)
{
    if (pcPath == 0)
    {
        return;
    }
    for (int i = 0; i < kNumOptions; ++i)
    {
        char acValue[16];
        GetPrivateProfileStringA(kSection, skOptions[i].pcKey, "", acValue, sizeof(acValue), pcPath);
        bool bValue;
        if (!kSettings.abCli[i] && DisplayCfg::ParseBool(acValue, bValue))
        {
            kSettings.abValue[i] = bValue;
        }
    }
}

bool SaveIni(int iOption, bool bValue, const char* pcPath)
{
    if (iOption < 0 || iOption >= kNumOptions || pcPath == 0)
    {
        return false;
    }
    return WritePrivateProfileStringA(kSection, skOptions[iOption].pcKey, bValue ? "1" : "0", pcPath) != 0;
}

bool ParseSwitch(Settings& kSettings, const char* pcSwitch, const char* pcValue)
{
    const int iOption = Find(pcSwitch);
    if (iOption < 0 || pcValue == 0 || (strcmp(pcValue, "0") != 0 && strcmp(pcValue, "1") != 0))
    {
        return false;
    }
    kSettings.abValue[iOption] = pcValue[0] == '1';
    kSettings.abCli[iOption] = true;
    return true;
}

static Settings sSettings;
static bool sbDefaults;
static bool sbLoaded;

static Settings& Running()
{
    if (!sbDefaults)
    {
        SetDefaults(sSettings);
        sbDefaults = true;
    }
    return sSettings;
}

bool Get(int iOption)
{
    if (iOption < 0 || iOption >= kNumOptions)
    {
        return false;
    }
    Settings& kSettings = Running();
    if (!sbLoaded)
    {
        sbLoaded = true;
        char acPath[MAX_PATH];
        if (DisplayCfg::DisplayIniPath(acPath, sizeof(acPath)))
        {
            LoadIni(kSettings, acPath);
        }
    }
    return kSettings.abValue[iOption];
}

bool Set(int iOption, bool bValue)
{
    if (iOption < 0 || iOption >= kNumOptions)
    {
        return false;
    }
    Get(iOption); // loads the rest first, so a later Get doesn't read over this
    sSettings.abValue[iOption] = bValue;
    sSettings.abCli[iOption] = false;
    char acPath[MAX_PATH];
    return DisplayCfg::DisplayIniPath(acPath, sizeof(acPath)) && SaveIni(iOption, bValue, acPath);
}

bool AwardWeaponsOn(bool bNetGame)
{
    return bNetGame || Get(kAwardWeapons);
}

bool ShowBuffEffect(float fBuffTimer)
{
    return fBuffTimer > 0.0f && (fBuffTimer <= 300.0f || Get(kAwardEffects));
}

bool ParseCommandLineSwitch(const char* pcSwitch, const char* pcValue)
{
    return ParseSwitch(Running(), pcSwitch, pcValue);
}

}
