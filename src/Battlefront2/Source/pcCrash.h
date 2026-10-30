#pragma once

struct _EXCEPTION_POINTERS;

namespace pcCrash
{
    void Install();
    void Reinstall();
    long Filter(struct _EXCEPTION_POINTERS* pInfo);
    void __cdecl Exit(int iCode);
    void SetLogPath(char const* pcPath);
    char const* GetLogPath();
    void SetCrashTest(char const* pcKind);
    void RunCrashTest();
    void RunMissionCrashTest();
    void FatalInit(char const* pcWhat, unsigned int uiSize);
    bool SimulateAllocFailure(char const* pcKind);
    void ReserveLowAddressSpace();
    bool IsLargeAddressAware();
    void __cdecl Check(char const* pcExpr, char const* pcFile, int iLine);
    void Tick();
    void SetHangTimeout(int iSeconds);
    void StartWatchdog();
    char const* GetBuildId();
    char const* GetCrashTest();
    void FatalReset(long hr, unsigned int uiFailures, unsigned int uiElapsedMs);
}
