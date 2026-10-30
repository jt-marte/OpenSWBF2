#pragma once
#include <stdio.h>
#include <stdlib.h>

extern char gCurrentCase[256];
void WatchdogArm(const char* pcLabel);

extern unsigned int gFuzzSeed;
extern unsigned int gFuzzIter;
extern unsigned int gFuzzCount;
extern bool gFuzzDump;
extern char gFuzzOutPath[260];

const char* FindGameDataRoot();

void SkipCase(const char* pcReason);

void HarnessError(const char* pcReason);

#define SKIP_IF(cond, reason) \
    do \
    { \
        if (cond) \
        { \
            SkipCase(reason); \
        } \
    } while (0)

#define REQUIRE_GAMEDATA() SKIP_IF(FindGameDataRoot() == 0, "GameData not found")

#include <string>
#include <vector>

namespace RedWarning
{
extern void(__cdecl* g_fnLogCallback)(char const*, ...);
}

class LogCapture
{
public:
    LogCapture();
    ~LogCapture();
    bool Contains(const char* pcText) const;
    int Count(const char* pcText) const;

private:
    static void __cdecl Sink(char const* pcLine, ...);
    static std::vector<std::string> sLines;
    void(__cdecl* mpOldCallback)(char const*, ...);
    int meOldSeverity;
};

class ExpectLog
{
public:
    explicit ExpectLog(const char* pcSubstring);
    ~ExpectLog();

private:
    ExpectLog(const ExpectLog&);
    ExpectLog& operator=(const ExpectLog&);
    std::string mText;
};

class ExpectEnsure
{
public:
    explicit ExpectEnsure(const char* pcName);

private:
    ExpectLog mLog;
};

void __cdecl TestLogObserver(int iSeverity, int iCategory, char const* pcLine);
int TestStrictnessReport(const char* pcCase);

bool ReadRepoText(const char* pcRelPath, std::string& sOut);
