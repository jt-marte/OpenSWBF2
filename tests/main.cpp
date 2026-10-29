#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_NO_WINDOWS_SEH
#include "third_party/doctest/doctest.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testutil.h"
#include "RedEngineFL/Util/RedWarning.h"
#include "RedEngineFL/Util/RedLog.h"

char gCurrentCase[256];
unsigned int gFuzzSeed = 1;
unsigned int gFuzzIter = 0;
unsigned int gFuzzCount = 0;
bool gFuzzDump = false;
char gFuzzOutPath[260];

static volatile LONG sDeadlineMs;
static char sLabel[256];
static DWORD sTimeoutMs = 10000;

void WatchdogArm(const char* pcLabel)
{
    strncpy(sLabel, pcLabel, sizeof(sLabel) - 1);
    InterlockedExchange(&sDeadlineMs, (LONG)(GetTickCount() + sTimeoutMs));
}

static DWORD WINAPI WatchdogProc(LPVOID)
{
    for (;;)
    {
        Sleep(50);
        if ((LONG)(GetTickCount() - (DWORD)sDeadlineMs) > 0)
        {
            printf("[TIMEOUT] %s %s\n", gCurrentCase, sLabel);
            fflush(stdout);
            TerminateProcess(GetCurrentProcess(), 124);
        }
    }
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* pInfo)
{
    printf("[CRASH] %s code=0x%08X\n", gCurrentCase, (unsigned int)pInfo->ExceptionRecord->ExceptionCode);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), pInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

extern void __cdecl PblTraceP(const char* Format, ...);
int FloatCorpusGenerate(const char* pcOutDir);

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--debug-output-child") == 0)
        {
            RedWarning::Init();
            RedLog::ParseCmds("all Verbose, LogNet Warning");
            RedLog::Apply("test");
            RedLog::Write(RedLog::LogCore, RedLog::Inform, "x.cpp", 1, "debug-output-marker %d", 7);
            RedLog::Write(RedLog::LogNet, RedLog::Inform, "x.cpp", 2, "net-filtered-marker");
            PblTraceP("pbltrace-marker\n");
            RedWarning::LogMessage("legacy-marker\n");
            return 0;
        }
        if (strncmp(argv[i], "--gen-corpus=", 13) == 0)
        {
            return FloatCorpusGenerate(argv[i] + 13);
        }
    }

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    SetUnhandledExceptionFilter(CrashFilter);

    bool bListing = false;
    for (int i = 1; i < argc; ++i)
    {
        const char* pcArg = argv[i];
        if (strncmp(pcArg, "-tc=", 4) == 0 || strncmp(pcArg, "--test-case=", 12) == 0)
        {
            strncpy(gCurrentCase, strchr(pcArg, '=') + 1, sizeof(gCurrentCase) - 1);
        }
        else if (strcmp(pcArg, "--timeout") == 0 && i + 1 < argc)
        {
            double dMs = atof(argv[i + 1]) * 1000.0;
            sTimeoutMs = dMs < 1000.0 ? 1000 : (DWORD)dMs;
        }
        else if (strncmp(pcArg, "--fuzz-seed=", 12) == 0)
        {
            gFuzzSeed = (unsigned int)strtoul(pcArg + 12, 0, 0);
        }
        else if (strncmp(pcArg, "--fuzz-iter=", 12) == 0)
        {
            gFuzzIter = (unsigned int)strtoul(pcArg + 12, 0, 0);
        }
        else if (strncmp(pcArg, "--fuzz-count=", 13) == 0)
        {
            gFuzzCount = (unsigned int)strtoul(pcArg + 13, 0, 0);
        }
        else if (strcmp(pcArg, "--fuzz-dump") == 0)
        {
            gFuzzDump = true;
        }
        else if (strncmp(pcArg, "--fuzz-out=", 11) == 0)
        {
            strncpy(gFuzzOutPath, pcArg + 11, sizeof(gFuzzOutPath) - 1);
        }
        else if (strstr(pcArg, "list-test") || strcmp(pcArg, "-ltc") == 0 || strcmp(pcArg, "-lts") == 0
                 || strcmp(pcArg, "-c") == 0 || strcmp(pcArg, "--count") == 0)
        {
            bListing = true;
        }
    }

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    if (!bListing)
    {
        RedWarning::Init();
        RedLog::gObserver = TestLogObserver;
        printf("[RUN] %s\n", gCurrentCase);
        fflush(stdout);
        WatchdogArm("");
        CreateThread(0, 0, WatchdogProc, 0, 0, 0);
    }
    int iRc = context.run();
    if (!bListing && TestStrictnessReport(gCurrentCase) != 0)
    {
        return 1;
    }
    return iRc;
}
