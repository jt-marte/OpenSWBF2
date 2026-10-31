#pragma once

#include <stddef.h>

struct SpawnCameraFloatPair
{
    float mFirstValue;
    float mSecondValue;
};
 

void ParseCommandLine(char const* pcAppName, char const* pcCmdLine);

void ParseCommandLine(int iArgc, char** const apcArgv);

extern char gGamespyAutoName[32];
extern char gGamespyAutoConnectAddr[64];
extern char gGamespyAutoPassStr[128];
extern char gGamespyAutoEmail[64];
extern char gGamespyAutoPPass[32];
extern unsigned int gGamespyAutoPass;
extern bool gBootLagger;
extern bool gShowLoadSpam;
extern bool gLogFinalScore;
extern bool gExitOnGameOver;
extern int gDigestTrace;
extern bool gFactoryLog;
extern bool gNoFocusPause;
extern bool gAllowUploads;
extern bool gNetStatsLog;
extern bool gNetLoopback;
extern bool gAIStatsLog;
extern bool gAITestHuman;
// -nettestplayer: a client spawns by itself and plays a fixed pattern of moves (NetGame.cpp)
extern bool gNetTestPlayer;
extern bool gLogStdout;
extern bool gLogConsole;
extern bool gDedicatedShorthand;
// -listmissions <file>: where ShellLoop writes the mission list before quitting; "" when not asked
extern char gListMissionsPath[260];
// The process exit code a normal quit (WM_CLOSE) returns; -listmissions sets 2 when it fails.
extern int gExitCode;
extern bool gSeedSet;
extern unsigned int gSeed;
extern int gBootLaggerPing;

namespace PlayerPrefs { extern int gDefaultDifficulty; }
namespace PlayerPrefs { extern int gDefaultTeamNum; }
