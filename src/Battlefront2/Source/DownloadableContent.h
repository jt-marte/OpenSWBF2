#pragma once

#include <stddef.h>

class LuaScript;

namespace DownloadableContent
{

struct DownloadableMissionType
{
    unsigned int mMapName;
    unsigned int mMissionName;
    char mDir[260];
    unsigned int mLevelMemory;
};
 
 
 

void SetCurrentMission(unsigned int uiHash);

void AddMissionCommon(LuaScript* pScript, char const* pcDir);
void AddMissions(LuaScript* pScript);
void SetCurrentMap(unsigned int uiHash);
bool IsDownloadableContent();
char const* GetContentDirectory();
bool IsMissionDownloaded(unsigned int uiHash);
bool ReadDataFile(char const* pcFileName);

// OpenSWBF2: retail kept 500 and silently dropped the rest; big mod collections pass that.
const int kMaxMissions = 4096;

// OpenSWBF2: the unofficial 1.3 patch's own addon. Its addme caps every other mod at 500
// missions, and OpenSWBF2 has the same mod support built in, so it is skipped.
bool IsSkippedAddOn(char const* pcFolder);

extern DownloadableMissionType mMissions[kMaxMissions];

}
