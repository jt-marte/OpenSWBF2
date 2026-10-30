#include <windows.h>
#include <dbghelp.h>
#include <intrin.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "pcCrash.h"
#include "GameMemory.h"
#include "LuaHelper.h"
#include "GameState.h"
#include "RedEngineFL/Memory/RedMemory.h"
#include "RedEngineFL/Util/RedLog.h"
#include "RedEngineFL/Application/pc/pcFrameLimiter.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"
#include "RedEngineFL/Graphics/PC/pcRedRenderer.h"
#include "RedEngineFL/input/pc/pcRedInputManager.h"
#include "PlayerProfile.h"

typedef BOOL (WINAPI* MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
typedef BOOL (WINAPI* SetThreadStackGuaranteeFn)(PULONG);

static LONG gFaultGuard;
static volatile DWORD gFaultThreadId;
static HANDLE ghFaultEvent;
static HANDLE ghDoneEvent;
static MINIDUMP_EXCEPTION_INFORMATION gDumpInfo;
static MiniDumpWriteDumpFn gpfnMiniDumpWriteDump;
static wchar_t gDumpDir[MAX_PATH];
static wchar_t gDefaultDumpDir[MAX_PATH];
static wchar_t gDumpPath[MAX_PATH + 64];
static char gLogPath[MAX_PATH];
static char gBuildId[32];
static char gCrashKind[32];
static char gFatalLine[1024];
static LPTOP_LEVEL_EXCEPTION_FILTER gpfnUef;
static bool gbInstalled;
static bool gbReinstalled;
static bool gbCrashTestRan;
static char gCheckText[384];
static char gCheckFile[128];
static int giCheckLine;
static volatile LONG gTicks;
extern bool gIsQuitting;
static unsigned int guiHangTimeout = 60;
static bool gbWatchdog;

static void TerminateNow(int iCode)
{
    HwCursor::ReleaseClip();
    FrameLimiter::EndPeriod();
    TerminateProcess(GetCurrentProcess(), (UINT)iCode);
    _exit(iCode);
}

static void AppendLog(char const* pcText)
{
    if (gLogPath[0] != 0)
    {
        HANDLE hFile = CreateFileA(gLogPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD uiWritten = 0;
            WriteFile(hFile, pcText, (DWORD)lstrlenA(pcText), &uiWritten, 0);
            CloseHandle(hFile);
        }
    }
    OutputDebugStringA(pcText);
}

static void MakeDirs(wchar_t* pwcPath)
{
    for (wchar_t* pwc = pwcPath + 3; *pwc != 0; ++pwc)
    {
        if (*pwc == L'\\')
        {
            *pwc = 0;
            CreateDirectoryW(pwcPath, 0);
            *pwc = L'\\';
        }
    }
    CreateDirectoryW(pwcPath, 0);
}

static void BuildDumpPath()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t const* pwcDir = gDumpDir;
    if (GetFileAttributesW(pwcDir) == INVALID_FILE_ATTRIBUTES)
    {
        pwcDir = gDefaultDumpDir;
    }
    wsprintfW(gDumpPath, L"%s\\BattlefrontII-%04d%02d%02d-%02d%02d%02d-%u.dmp", pwcDir, (int)st.wYear,
        (int)st.wMonth, (int)st.wDay, (int)st.wHour, (int)st.wMinute, (int)st.wSecond,
        (unsigned int)GetCurrentProcessId());
}

static void WriteDump()
{
    gDumpPath[0] = 0;
    if (gpfnMiniDumpWriteDump == 0)
    {
        return;
    }
    BuildDumpPath();
    HANDLE hFile = CreateFileW(gDumpPath, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        gDumpPath[0] = 0;
        return;
    }
    MINIDUMP_TYPE eFull = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithThreadInfo);
    PMINIDUMP_EXCEPTION_INFORMATION pExc = gDumpInfo.ExceptionPointers ? &gDumpInfo : 0;
    BOOL bOk = gpfnMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, eFull, pExc,
        0, 0);
    if (!bOk)
    {
        SetFilePointer(hFile, 0, 0, FILE_BEGIN);
        SetEndOfFile(hFile);
        bOk = gpfnMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal,
            pExc, 0, 0);
    }
    CloseHandle(hFile);
    if (!bOk)
    {
        DeleteFileW(gDumpPath);
        gDumpPath[0] = 0;
    }
}

static DWORD WINAPI DumpThread(LPVOID)
{
    for (;;)
    {
        WaitForSingleObject(ghFaultEvent, INFINITE);
        WriteDump();
        SetEvent(ghDoneEvent);
    }
}

static void WriteEnsureSummary()
{
    unsigned int uiFailures = RedLog::EnsureFailureCount();
    if (uiFailures != 0)
    {
        char acLine[96];
        wsprintfA(acLine, "ensure summary: %u failures at %u sites\r\n", uiFailures, RedLog::EnsureSiteCount());
        AppendLog(acLine);
    }
}

static void WriteFatalLine(EXCEPTION_POINTERS* pInfo)
{
    char acModule[MAX_PATH];
    char acPath[MAX_PATH * 3];
    wchar_t awcModule[MAX_PATH];
    unsigned int uiCode = 0;
    unsigned int uiAddr = 0;
    unsigned int uiOffset = 0;
    HMODULE hModule = 0;
    lstrcpyA(acModule, "unknown");
    lstrcpyA(acPath, "none");
    if (pInfo != 0 && pInfo->ExceptionRecord != 0)
    {
        uiCode = (unsigned int)pInfo->ExceptionRecord->ExceptionCode;
        uiAddr = (unsigned int)(UINT_PTR)pInfo->ExceptionRecord->ExceptionAddress;
    }
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(UINT_PTR)uiAddr, &hModule) && hModule != 0)
    {
        DWORD uiLen = GetModuleFileNameW(hModule, awcModule, MAX_PATH);
        if (uiLen > 0 && uiLen < MAX_PATH)
        {
            wchar_t* pwcBase = awcModule;
            for (wchar_t* pwc = awcModule; *pwc != 0; ++pwc)
            {
                if (*pwc == L'\\' || *pwc == L'/')
                {
                    pwcBase = pwc + 1;
                }
            }
            if (WideCharToMultiByte(CP_ACP, 0, pwcBase, -1, acModule, MAX_PATH, 0, 0) == 0)
            {
                lstrcpyA(acModule, "unknown");
            }
            uiOffset = uiAddr - (unsigned int)(UINT_PTR)hModule;
        }
    }
    if (gDumpPath[0] != 0)
    {
        if (WideCharToMultiByte(CP_ACP, 0, gDumpPath, -1, acPath, sizeof(acPath), 0, 0) == 0)
        {
            lstrcpyA(acPath, "none");
        }
    }
    if (uiCode == 0xE0000004)
    {
        wsprintfA(gFatalLine, "FATAL: check failed: %.200s at %.120s:%d build %s dump %.400s\r\n", gCheckText, gCheckFile,
            giCheckLine, gBuildId, acPath);
    }
    else
    {
        wsprintfA(gFatalLine, "FATAL: exception 0x%08X at 0x%08X (%.64s+0x%X) build %s dump %.400s\r\n", uiCode,
            uiAddr, acModule, uiOffset, gBuildId, acPath);
    }
    AppendLog(gFatalLine);
    WriteEnsureSummary();
}

long pcCrash::Filter(struct _EXCEPTION_POINTERS* pInfo)
{
    if (IsDebuggerPresent())
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (pInfo != 0 && pInfo->ExceptionRecord != 0)
    {
        DWORD uiCode = pInfo->ExceptionRecord->ExceptionCode;
        if (uiCode == 0x406D1388 || uiCode == 0x40010006 || uiCode == 0x4001000A)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
    if (InterlockedExchange(&gFaultGuard, 1) == 1)
    {
        if (gFaultThreadId != GetCurrentThreadId())
        {
            Sleep(60000);
        }
        TerminateNow(3);
    }
    gFaultThreadId = GetCurrentThreadId();
    HwCursor::ReleaseClip();
    gDumpPath[0] = 0;
    if (ghFaultEvent != 0 && ghDoneEvent != 0)
    {
        gDumpInfo.ThreadId = GetCurrentThreadId();
        gDumpInfo.ExceptionPointers = pInfo;
        gDumpInfo.ClientPointers = FALSE;
        SetEvent(ghFaultEvent);
        WaitForSingleObject(ghDoneEvent, 30000);
    }
    WriteFatalLine(pInfo);
    TerminateNow(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

static LONG WINAPI UefStub(EXCEPTION_POINTERS* pInfo)
{
    return pcCrash::Filter(pInfo);
}

static void RaiseCrash(DWORD uiCode)
{
    __try
    {
        RaiseException(uiCode, EXCEPTION_NONCONTINUABLE, 0, 0);
    }
    __except (pcCrash::Filter(GetExceptionInformation()))
    {
        TerminateNow(3);
    }
    TerminateNow(3);
}

static void __cdecl InvalidParamHandler(wchar_t const*, wchar_t const*, wchar_t const*, unsigned int,
    uintptr_t)
{
    RaiseCrash(0xE0000001);
}

static void __cdecl PureCallHandler()
{
    RaiseCrash(0xE0000002);
}

static void __cdecl AbortHandler(int)
{
    RaiseCrash(0xE0000003);
}

static void InstallHandlers()
{
    gpfnUef = UefStub;
    SetUnhandledExceptionFilter(UefStub);
    _set_invalid_parameter_handler(InvalidParamHandler);
    _set_purecall_handler(PureCallHandler);
    signal(SIGABRT, AbortHandler);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void pcCrash::Install()
{
    if (gbInstalled)
    {
        return;
    }
    gbInstalled = true;

    wchar_t awcSys[MAX_PATH];
    UINT uiSysLen = GetSystemDirectoryW(awcSys, MAX_PATH - 16);
    if (uiSysLen > 0 && uiSysLen < MAX_PATH - 16)
    {
        lstrcatW(awcSys, L"\\dbghelp.dll");
        HMODULE hDbgHelp = LoadLibraryW(awcSys);
        if (hDbgHelp != 0)
        {
            gpfnMiniDumpWriteDump = (MiniDumpWriteDumpFn)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
        }
    }

    wchar_t awcLocal[MAX_PATH];
    DWORD uiLocalLen = GetEnvironmentVariableW(L"LOCALAPPDATA", awcLocal, MAX_PATH - 32);
    if (uiLocalLen > 0 && uiLocalLen < MAX_PATH - 32)
    {
        wsprintfW(gDefaultDumpDir, L"%s\\OpenSWBF2\\crashes", awcLocal);
        MakeDirs(gDefaultDumpDir);
    }
    else
    {
        lstrcpyW(gDefaultDumpDir, L".");
    }
    if (gDumpDir[0] == 0)
    {
        lstrcpyW(gDumpDir, gDefaultDumpDir);
    }

    HMODULE hExe = GetModuleHandleW(0);
    IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)hExe;
    IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)((char*)hExe + pDos->e_lfanew);
    wsprintfA(gBuildId, "%08X%X", (unsigned int)pNt->FileHeader.TimeDateStamp,
        (unsigned int)pNt->OptionalHeader.SizeOfImage);

    ghFaultEvent = CreateEventW(0, FALSE, FALSE, 0);
    ghDoneEvent = CreateEventW(0, FALSE, FALSE, 0);
    if (ghFaultEvent != 0 && ghDoneEvent != 0)
    {
        HANDLE hThread = CreateThread(0, 64 * 1024, DumpThread, 0, 0, 0);
        if (hThread != 0)
        {
            CloseHandle(hThread);
        }
    }

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    SetThreadStackGuaranteeFn pfnGuarantee = 0;
    if (hKernel != 0)
    {
        pfnGuarantee = (SetThreadStackGuaranteeFn)GetProcAddress(hKernel, "SetThreadStackGuarantee");
    }
    if (pfnGuarantee != 0)
    {
        ULONG uiStack = 65536;
        pfnGuarantee(&uiStack);
    }

    InstallHandlers();
}

void __cdecl pcCrash::Check(char const* pcExpr, char const* pcFile, int iLine)
{
    lstrcpynA(gCheckText, pcExpr ? pcExpr : "?", sizeof(gCheckText));
    lstrcpynA(gCheckFile, pcFile ? pcFile : "?", sizeof(gCheckFile));
    giCheckLine = iLine;
    RaiseCrash(0xE0000004);
}

void pcCrash::Tick()
{
    InterlockedIncrement(&gTicks);
}

void pcCrash::SetHangTimeout(int iSeconds)
{
    if (iSeconds >= 1 && iSeconds <= 3600)
    {
        guiHangTimeout = (unsigned int)iSeconds;
    }
}

char const* pcCrash::GetBuildId()
{
    return gBuildId;
}

static DWORD WINAPI WatchdogThread(LPVOID)
{
    LONG iLast = gTicks;
    DWORD uiChanged = GetTickCount();
    for (;;)
    {
        Sleep(500);
        LONG iNow = gTicks;
        DWORD uiNow = GetTickCount();
        if (iNow != iLast || gIsQuitting)
        {
            iLast = iNow;
            uiChanged = uiNow;
        }
        else if ((DWORD)(uiNow - uiChanged) >= guiHangTimeout * 1000)
        {
            if (InterlockedExchange(&gFaultGuard, 1) == 1)
            {
                return 0;
            }
            gFaultThreadId = GetCurrentThreadId();
            HwCursor::ReleaseClip();
            gDumpPath[0] = 0;
            gDumpInfo.ExceptionPointers = 0;
            if (ghFaultEvent != 0 && ghDoneEvent != 0)
            {
                SetEvent(ghFaultEvent);
                WaitForSingleObject(ghDoneEvent, 30000);
            }
            char acPath[MAX_PATH * 3];
            lstrcpyA(acPath, "none");
            if (gDumpPath[0] != 0 && WideCharToMultiByte(CP_ACP, 0, gDumpPath, -1, acPath, sizeof(acPath), 0, 0) == 0)
            {
                lstrcpyA(acPath, "none");
            }
            wsprintfA(gFatalLine, "FATAL: hang (no frame update for %u s, last frame %d) build %s dump %.400s\r\n",
                guiHangTimeout, RedLog::gFrame, gBuildId, acPath);
            AppendLog(gFatalLine);
            WriteEnsureSummary();
            TerminateNow(5);
        }
    }
}

void pcCrash::StartWatchdog()
{
    if (gbWatchdog || IsDebuggerPresent())
    {
        return;
    }
    gbWatchdog = true;
    HANDLE hThread = CreateThread(0, 64 * 1024, WatchdogThread, 0, 0, 0);
    if (hThread != 0)
    {
        CloseHandle(hThread);
    }
}

char const* pcCrash::GetCrashTest()
{
    return gCrashKind;
}

void pcCrash::FatalReset(long hr, unsigned int uiFailures, unsigned int uiElapsedMs)
{
    if (InterlockedExchange(&gFaultGuard, 1) == 1)
    {
        TerminateNow(1);
    }
    gFaultThreadId = GetCurrentThreadId();
    HwCursor::ReleaseClip();
    gDumpPath[0] = 0;
    gDumpInfo.ExceptionPointers = 0;
    if (ghFaultEvent != 0 && ghDoneEvent != 0)
    {
        SetEvent(ghFaultEvent);
        WaitForSingleObject(ghDoneEvent, 30000);
    }
    char acPath[MAX_PATH * 3];
    lstrcpyA(acPath, "none");
    if (gDumpPath[0] != 0 && WideCharToMultiByte(CP_ACP, 0, gDumpPath, -1, acPath, sizeof(acPath), 0, 0) == 0)
    {
        lstrcpyA(acPath, "none");
    }
    wsprintfA(gFatalLine, "FATAL: device reset failed hr=0x%08X failures=%u after=%u ms build %s dump %.400s\r\n",
        (unsigned int)hr, uiFailures, uiElapsedMs, gBuildId, acPath);
    AppendLog(gFatalLine);
    WriteEnsureSummary();
    FrameLimiter::Shutdown();
    TerminateNow(1);
}

void pcCrash::Reinstall()
{
    InstallHandlers();
    gbReinstalled = true;
}

void pcCrash::SetLogPath(char const* pcPath)
{
    if (pcPath == 0 || strlen(pcPath) >= MAX_PATH)
    {
        return;
    }
    strcpy(gLogPath, pcPath);
    int iSep = -1;
    for (int i = 0; gLogPath[i] != 0; ++i)
    {
        if (gLogPath[i] == '\\' || gLogPath[i] == '/')
        {
            iSep = i;
        }
    }
    if (iSep < 0)
    {
        lstrcpyW(gDumpDir, L".");
        return;
    }
    if (iSep == 0)
    {
        iSep = 1;
    }
    int iLen = MultiByteToWideChar(CP_ACP, 0, gLogPath, iSep, gDumpDir, MAX_PATH - 1);
    gDumpDir[iLen] = 0;
}

void __cdecl pcCrash::Exit(int iCode)
{
    HwCursor::ReleaseClip();
    FrameLimiter::Shutdown();

    if (iCode == 0)
    {
        LPTOP_LEVEL_EXCEPTION_FILTER pfnPrev = SetUnhandledExceptionFilter(gpfnUef);
        if (pfnPrev != gpfnUef)
        {
            AppendLog("WARNING: unhandled exception filter was replaced\r\n");
        }
    }
    else if (iCode == 1)
    {
        AppendLog("FATAL: initialization failed, exiting with code 1\r\n");
    }
    else if (iCode == 4)
    {
        AppendLog("FATAL: lua panic, exiting with code 4\r\n");
    }
    _exit(iCode);
}

static char const* FindArg(char const* pcCmd, char const* pcName, int* piLen)
{
    int iNameLen = lstrlenA(pcName);
    for (char const* pc = pcCmd; *pc != 0; ++pc)
    {
        bool bStart = (pc == pcCmd) || pc[-1] == ' ' || pc[-1] == '"';
        if (bStart && _strnicmp(pc, pcName, iNameLen) == 0 && (pc[iNameLen] == ' ' || pc[iNameLen] == 0))
        {
            char const* pcVal = pc + iNameLen;
            while (*pcVal == ' ')
            {
                ++pcVal;
            }
            char cEnd = ' ';
            if (*pcVal == '"')
            {
                cEnd = '"';
                ++pcVal;
            }
            int iLen = 0;
            while (pcVal[iLen] != 0 && pcVal[iLen] != cEnd)
            {
                ++iLen;
            }
            *piLen = iLen;
            return pcVal;
        }
    }
    return 0;
}

bool pcCrash::SimulateAllocFailure(char const* pcKind)
{
    int iLen = 0;
    char const* pcVal = FindArg(GetCommandLineA(), "-crashtest", &iLen);
    return pcVal != 0 && iLen == lstrlenA(pcKind) && _strnicmp(pcVal, pcKind, iLen) == 0;
}

void pcCrash::ReserveLowAddressSpace()
{
    int iLen = 0;
    if (FindArg(GetCommandLineA(), "-hogaddrspace", &iLen) == 0)
    {
        return;
    }
#ifdef _WIN64
    // x64: every allocation lands above 4 GB, so a pointer squeezed through 32 bits faults
    const uintptr_t kLimit = 0x100000000ull;
#else
    const uintptr_t kLimit = 0x80000000u;
#endif
    const uintptr_t kMinRegion = 0x100000u;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t uiGran = si.dwAllocationGranularity;
    uintptr_t uiAt = (uintptr_t)si.lpMinimumApplicationAddress;
    int iRetry = 0;
    while (uiAt < kLimit)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void*)uiAt, &mbi, sizeof(mbi)))
        {
            break;
        }
        uintptr_t uiBase = (uintptr_t)mbi.BaseAddress;
        uintptr_t uiEnd = uiBase + mbi.RegionSize;
        if (uiEnd < uiBase || uiEnd > kLimit)
        {
            uiEnd = kLimit;
        }
        if (mbi.State == MEM_FREE)
        {
            uintptr_t uiFrom = (uiBase + uiGran - 1) & ~(uiGran - 1);
            if (uiEnd > uiFrom && (uiEnd - uiFrom >= kMinRegion || uiEnd == kLimit))
            {
                if (!VirtualAlloc((void*)uiFrom, uiEnd - uiFrom, MEM_RESERVE, PAGE_NOACCESS) && iRetry < 16)
                {
                    iRetry++;
                    continue;
                }
            }
        }
        if (uiEnd == kLimit)
        {
            break;
        }
        uiAt = uiEnd;
    }
}

bool pcCrash::IsLargeAddressAware()
{
    char* pBase = (char*)GetModuleHandleA(0);
    IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)pBase;
    IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)(pBase + pDos->e_lfanew);
    return (pNt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
}

void pcCrash::FatalInit(char const* pcWhat, unsigned int uiSize)
{
    if (gLogPath[0] == 0)
    {
        int iLen = 0;
        char const* pcVal = FindArg(GetCommandLineA(), "-log", &iLen);
        if (pcVal != 0 && iLen > 0 && iLen < MAX_PATH)
        {
            memcpy(gLogPath, pcVal, iLen);
            gLogPath[iLen] = 0;
        }
    }
    char acLine[256];
    int iLen = wsprintfA(acLine, "FATAL: out of memory allocating %.64s (%u bytes)", pcWhat, uiSize);
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
    {
        iLen += wsprintfA(acLine + iLen, " load %u%% phys %u MB virt %u MB", (unsigned int)ms.dwMemoryLoad,
            (unsigned int)(ms.ullAvailPhys >> 20), (unsigned int)(ms.ullAvailVirtual >> 20));
    }
    lstrcpyA(acLine + iLen, "\r\n");
    AppendLog(acLine);
    WriteEnsureSummary();
    pcCrash::Exit(1);
}

static int LuaAllocHuge(lua_State* L)
{
    lua_newuserdata(L, 0x04000000);
    return 0;
}

extern bool pcWindowed;

static unsigned int MenuTestVsync(const PlayerProfile::tPCOpt& rPC)
{
    const DisplayCfg::DisplaySettings& rDisplay = DisplayCfg::gDisplay;
    return DisplayLogic::EffectiveVsync((rDisplay.uiSet & DisplayCfg::kVsync) != 0, rDisplay.uiVsync,
                                        rPC.mVSync);
}

enum MenuTest
{
    kMenuRevert,
    kMenuKeep,
    kMenuSaveFail,
    kMenuAdapterAccept,
    kMenuOrder,
    kMenuOrderRev,
    kMenuRevertSave,
    kMenuApplyFail
};

static void MenuConfirmFrame(MenuTest eTest, bool& rbRan)
{
    const bool bOrder = eTest == kMenuOrder || eTest == kMenuOrderRev;
    const bool bArmKeep = eTest != kMenuRevert && !bOrder;
    const bool bKeep = eTest == kMenuKeep || eTest == kMenuSaveFail;
    static unsigned int suiFrame;
    static unsigned int suiArmedAt;
    static unsigned int suiCountedAt;
    static unsigned int suiAfter;
    static unsigned int suiMode;
    static bool sbArmed;
    static bool sbCounted;
    static bool sbSkewed;

    ++suiFrame;
    if (!sbArmed)
    {
        if (suiFrame < 30)
        {
            return;
        }
        PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
        if (!pProfile)
        {
            LOG(LogRender, Warning, "OPENSWBF2 DISPLAY menutest no profile");
            rbRan = true;
            return;
        }
        PlayerProfile::MarkDisplay();
        PlayerProfile::tPCOpt& rPC = pProfile->mPCOptions;
        const unsigned int uiBaseW = rPC.mResolutionX;
        const unsigned int uiBaseH = rPC.mResolutionY;
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest base %ux%u fps=%u waitquery=%d", uiBaseW,
            uiBaseH, DisplayCfg::gDisplay.uiFps, DisplayCfg::gDisplay.bNoWaitQuery ? 0 : 1);
        const DisplayLogic::DisplayMode eBase =
            DisplayLogic::ResolveMode(DisplayCfg::gDisplay.bBorderless, pcWindowed, rPC.mWindowed);
        const unsigned int uiAcceptFps = DisplayCfg::gDisplay.uiFps == 30 ? 60 : 30;
        if (eTest == kMenuRevertSave || eTest == kMenuApplyFail)
        {
            LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest prior vsync=%u fps=%u accept=%u",
                MenuTestVsync(rPC), DisplayCfg::gDisplay.uiFps, uiAcceptFps);
            DisplayCfg::MenuFields kPlain;
            kPlain.uiMode = eBase;
            kPlain.uiRefresh = DisplayCfg::gDisplay.uiRefresh;
            kPlain.uiVsync = MenuTestVsync(rPC);
            kPlain.uiFps = uiAcceptFps;
            kPlain.bWaitQuery = !DisplayCfg::gDisplay.bNoWaitQuery;
            PlayerProfile::ApplyMenuDisplay(kPlain);
        }
        DisplayCfg::MenuFields kFields;
        kFields.uiMode = eBase == DisplayLogic::kModeFullscreen ? DisplayLogic::kModeWindowed
                                                                 : DisplayLogic::kModeFullscreen;
        kFields.uiRefresh = DisplayCfg::gDisplay.uiRefresh;
        kFields.uiVsync = bArmKeep ? 2 : MenuTestVsync(rPC);
        kFields.uiFps = DisplayCfg::gDisplay.uiFps;
        kFields.bWaitQuery = !DisplayCfg::gDisplay.bNoWaitQuery;
        if (eTest == kMenuRevert)
        {
            kFields.uiFps = uiAcceptFps;
            kFields.bWaitQuery = DisplayCfg::gDisplay.bNoWaitQuery;
        }
        else if (eTest == kMenuApplyFail)
        {
            kFields.uiFps = uiAcceptFps + 15;
            kFields.bWaitQuery = DisplayCfg::gDisplay.bNoWaitQuery;
        }
        if (uiBaseW == 1280 && uiBaseH == 720)
        {
            rPC.mResolutionX = 1024;
            rPC.mResolutionY = 768;
        }
        else
        {
            rPC.mResolutionX = 1280;
            rPC.mResolutionY = 720;
        }
        pProfile->UpdateProfCRC();
        suiMode = kFields.uiMode;
        if (eTest == kMenuOrder)
        {
            LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest order=legacy-menu");
            PlayerProfile::ApplyLiveDisplay();
            PlayerProfile::ApplyMenuDisplay(kFields);
        }
        else if (eTest == kMenuOrderRev)
        {
            LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest order=menu-legacy");
            PlayerProfile::ApplyMenuDisplay(kFields);
            PlayerProfile::ApplyLiveDisplay();
        }
        else
        {
            PlayerProfile::ApplyMenuDisplay(kFields);
        }
        if (bArmKeep)
        {
            if (eTest == kMenuAdapterAccept)
            {
                RedRenderer::pcRequestAdapter(0);
            }
            LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest save=%u", PlayerProfile::SaveDisplay());
            PlayerProfile::MarkDisplay();
        }
        sbArmed = true;
        suiArmedAt = suiFrame;
        return;
    }

    unsigned int uiState = 0;
    unsigned int uiLeft = 0;
    unsigned int uiResult = 0;
    unsigned int uiKeepSave = 0;
    PlayerProfile::PollDisplayConfirm(uiState, uiLeft, uiResult, uiKeepSave);
    if (suiFrame - suiArmedAt >= 600)
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY menutest no confirm state=%u", uiState);
        rbRan = true;
        return;
    }
    if (eTest == kMenuApplyFail)
    {
        sbCounted = true;
        sbSkewed = true;
    }
    if (!sbCounted)
    {
        if (uiState != DisplayLogic::kConfirmCounting)
        {
            return;
        }
        sbCounted = true;
        suiCountedAt = suiFrame;
        if (bKeep)
        {
            if (eTest == kMenuSaveFail)
            {
                char acPath[MAX_PATH];
                if (DisplayCfg::DisplayIniPath(acPath, sizeof(acPath)))
                {
                    WritePrivateProfileStringA("Display", "Marker", "1", acPath);
                    SetFileAttributesA(acPath, FILE_ATTRIBUTE_READONLY);
                }
            }
            PlayerProfile::EndDisplayConfirm(true);
            PlayerProfile::SkewDisplayConfirmClock(DisplayLogic::kConfirmMs + 1000);
            sbSkewed = true;
        }
        else if (eTest == kMenuRevert)
        {
            PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
            DisplayCfg::MenuFields kFields;
            kFields.uiMode = suiMode;
            kFields.uiRefresh = DisplayCfg::gDisplay.uiRefresh;
            kFields.uiVsync = (pProfile && MenuTestVsync(pProfile->mPCOptions) == 0) ? 1 : 0;
            kFields.uiFps = DisplayCfg::gDisplay.uiFps;
            kFields.bWaitQuery = !DisplayCfg::gDisplay.bNoWaitQuery;
            LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest plain vsync=%u", kFields.uiVsync);
            PlayerProfile::ApplyMenuDisplay(kFields);
        }
        return;
    }
    if (!sbSkewed)
    {
        if (suiFrame - suiCountedAt >= 10)
        {
            PlayerProfile::SkewDisplayConfirmClock(DisplayLogic::kConfirmMs);
            sbSkewed = true;
        }
        return;
    }
    if (uiState != DisplayLogic::kConfirmNone || ++suiAfter < 30)
    {
        return;
    }
    if (eTest == kMenuRevertSave)
    {
        PlayerProfile::RestoreDisplay();
        PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest restored vsync=%u fps=%u",
            pProfile ? MenuTestVsync(pProfile->mPCOptions) : 0u, DisplayCfg::gDisplay.uiFps);
    }
    if (eTest == kMenuSaveFail)
    {
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menutest save=%u", PlayerProfile::SaveDisplay());
        char acPath[MAX_PATH];
        if (DisplayCfg::DisplayIniPath(acPath, sizeof(acPath)))
        {
            SetFileAttributesA(acPath, FILE_ATTRIBUTE_NORMAL);
        }
    }
    LOG(LogRender, Inform,
        "OPENSWBF2 DISPLAY menutest done result=%u fps=%u waitquery=%d keepsave=%u", uiResult,
        DisplayCfg::gDisplay.uiFps, DisplayCfg::gDisplay.bNoWaitQuery ? 0 : 1, uiKeepSave);
    rbRan = true;
}

static void AdapterCancelFrame(bool& rbRan)
{
    static unsigned int suiFrame;
    if (++suiFrame < 30)
    {
        return;
    }
    PlayerProfile::MarkDisplay();
    RedRenderer::pcRequestAdapter(0);
    PlayerProfile::RestoreDisplay();
    LOG(LogRender, Inform, "OPENSWBF2 DISPLAY adaptertest save=%u", PlayerProfile::SaveDisplay());
    rbRan = true;
}

void pcCrash::RunMissionCrashTest()
{
    static bool sbRan;
    if (sbRan || !gbReinstalled || gCrashKind[0] == 0)
    {
        return;
    }
    if (strcmp(gCrashKind, "heapfail") == 0)
    {
        sbRan = true;
        RedAllocFromHeap(GameMemory::RunTimeHeap, 0x10000000, 0);
    }
    else if (strcmp(gCrashKind, "luaoom") == 0)
    {
        sbRan = true;
        lua_cpcall(LuaHelper::mState, LuaAllocHuge, 0);
    }
    else if (strcmp(gCrashKind, "luapanic") == 0)
    {
        sbRan = true;
        lua_pushstring(LuaHelper::mState, "crashtest");
        lua_error(LuaHelper::mState);
    }
    else if (strcmp(gCrashKind, "hang") == 0)
    {
        sbRan = true;
        for (;;)
        {
            Sleep(10);
        }
    }
    else if (strcmp(gCrashKind, "check") == 0)
    {
        sbRan = true;
        (void)ensure(false);
        check(false);
    }
    else if (strcmp(gCrashKind, "resetlaterinit") == 0)
    {
        sbRan = true;
        GameState::SetState(0x8FF60339);
    }
    else if (strcmp(gCrashKind, "menuconfirm") == 0)
    {
        MenuConfirmFrame(kMenuRevert, sbRan);
    }
    else if (strcmp(gCrashKind, "menukeep") == 0)
    {
        MenuConfirmFrame(kMenuKeep, sbRan);
    }
    else if (strcmp(gCrashKind, "menusavefail") == 0)
    {
        MenuConfirmFrame(kMenuSaveFail, sbRan);
    }
    else if (strcmp(gCrashKind, "adaptercancel") == 0)
    {
        AdapterCancelFrame(sbRan);
    }
    else if (strcmp(gCrashKind, "adapteraccept") == 0)
    {
        MenuConfirmFrame(kMenuAdapterAccept, sbRan);
    }
    else if (strcmp(gCrashKind, "menuorder") == 0)
    {
        MenuConfirmFrame(kMenuOrder, sbRan);
    }
    else if (strcmp(gCrashKind, "menuorderrev") == 0)
    {
        MenuConfirmFrame(kMenuOrderRev, sbRan);
    }
    else if (strcmp(gCrashKind, "menurevertsave") == 0)
    {
        MenuConfirmFrame(kMenuRevertSave, sbRan);
    }
    else if (strcmp(gCrashKind, "menuapplyfail") == 0)
    {
        MenuConfirmFrame(kMenuApplyFail, sbRan);
    }
    else
    {
        sbRan = true;
    }
}

void pcCrash::SetCrashTest(char const* pcKind)
{
    static char const* const apcKinds[] = { "nullderef", "divzero", "stackoverflow", "debugbreak", "abort",
        "invalidparam", "fastfail", "arenaoom", "debugheapoom", "heapfail",
        "luaoom", "luapanic", "hang", "check", "resetrecover", "resetfatal", "resetinitlost",
        "resetflap", "resetlaterinit", "applymode", "applymodesmall", "applyfail", "applypresentfail", "applyadapter",
        "applytargetfail", "gammapass", "gammafallback", "gammadrawfail",
        "gammalost", "menuconfirm", "menukeep", "menusavefail", "adaptercancel",
        "adapteraccept", "shellstretch", "menuorder", "menuorderrev",
        "menurevertsave", "menuapplyfail" };
    if (pcKind == 0)
    {
        return;
    }
    for (int i = 0; i < (int)(sizeof(apcKinds) / sizeof(apcKinds[0])); ++i)
    {
        if (_stricmp(pcKind, apcKinds[i]) == 0)
        {
            strcpy(gCrashKind, apcKinds[i]);
            return;
        }
    }
}

static __declspec(noinline) int Recurse(int iDepth)
{
    volatile char acBuf[4096];
    acBuf[0] = (char)iDepth;
    acBuf[4095] = (char)(iDepth >> 8);
    return Recurse(iDepth + 1) + acBuf[0] + acBuf[4095];
}

void pcCrash::RunCrashTest()
{
    if (gCrashKind[0] == 0 || !gbReinstalled || gbCrashTestRan)
    {
        return;
    }
    gbCrashTestRan = true;
    if (strcmp(gCrashKind, "nullderef") == 0)
    {
        volatile int* piNull = 0;
        *piNull = 1;
    }
    else if (strcmp(gCrashKind, "divzero") == 0)
    {
        volatile int iZero = 0;
        volatile int iResult = 1 / iZero;
        (void)iResult;
    }
    else if (strcmp(gCrashKind, "stackoverflow") == 0)
    {
        Recurse(0);
    }
    else if (strcmp(gCrashKind, "debugbreak") == 0)
    {
        __debugbreak();
    }
    else if (strcmp(gCrashKind, "abort") == 0)
    {
        abort();
    }
    else if (strcmp(gCrashKind, "invalidparam") == 0)
    {
        strcpy_s(0, 1, "x");
    }
    else if (strcmp(gCrashKind, "fastfail") == 0)
    {
        __fastfail(7);
    }
}
