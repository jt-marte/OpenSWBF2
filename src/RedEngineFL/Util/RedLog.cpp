#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "RedEngineFL/Util/RedLog.h"

extern void(__cdecl* gPblTraceSink)(char const*);

namespace RedLog
{

Observer gObserver = 0;
int gFrame = 0;

namespace
{

struct Site
{
    char const* pcFile;
    int iLine;
    unsigned int uCount;
};

CRITICAL_SECTION sLock;
volatile LONG sLockState = 0;
int sVerbosity[CategoryCount] = { Inform, Inform, Inform, Inform, Inform, Inform, Inform, Inform, Inform };
char sDeferred[256];
LARGE_INTEGER sBase;
LARGE_INTEGER sFreq;
bool sBaseSet = false;
Site sSites[64];
unsigned int sSiteCount = 0;
unsigned int sFailures = 0;
bool sTableFullLogged = false;

char const* const sCategoryNames[CategoryCount] = { "LogNet", "LogLua", "LogLoad", "LogMemory", "LogSim", "LogRender", "LogAudio", "LogCore", "LogTemp" };
char const* const sSeverityNames[] = { "Verbose", "Inform", "Warning", "Error", "Bug", "Fatal" };

void Lock()
{
    if (sLockState != 2)
    {
        if (InterlockedCompareExchange(&sLockState, 1, 0) == 0)
        {
            InitializeCriticalSection(&sLock);
            sLockState = 2;
        }
        else
        {
            while (sLockState != 2)
            {
                Sleep(0);
            }
        }
    }
    EnterCriticalSection(&sLock);
}

void Unlock()
{
    LeaveCriticalSection(&sLock);
}

void __cdecl TraceSink(char const* pcText)
{
    Write(LogCore, Verbose, "PblTrace.cpp", 0, "%s", pcText);
}

bool IsSep(char c)
{
    return c == '\\' || c == '/';
}

}

char const* CategoryName(int iCategory)
{
    return (iCategory >= 0 && iCategory < CategoryCount) ? sCategoryNames[iCategory] : "?";
}

char const* SeverityName(int iSeverity)
{
    return (iSeverity >= Verbose && iSeverity <= Fatal) ? sSeverityNames[iSeverity] : "?";
}

char const* TrimPath(char const* pcPath)
{
    char const* pcResult = pcPath;
    for (char const* pc = pcPath; *pc; ++pc)
    {
        if (pc[0] == 's' && pc[1] == 'r' && pc[2] == 'c' && IsSep(pc[3]) && (pc == pcPath || IsSep(pc[-1])))
        {
            pcResult = pc + 4;
        }
    }
    return pcResult;
}

static HANDLE sStdout;

void SetStdout(StdoutMode eMode)
{
    sStdout = 0;
    if (eMode == StdoutOff)
    {
        return;
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((!hOut || hOut == INVALID_HANDLE_VALUE) && eMode >= StdoutAttach)
    {
        // AttachConsole is XP+, so look it up rather than raise the headers' target version.
        typedef BOOL(WINAPI * AttachConsoleFn)(DWORD);
        AttachConsoleFn pfnAttach = (AttachConsoleFn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "AttachConsole");
        if ((pfnAttach && pfnAttach((DWORD)-1)) || (eMode == StdoutConsole && AllocConsole()))
        {
            hOut = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        }
    }
    if (hOut && hOut != INVALID_HANDLE_VALUE)
    {
        sStdout = hOut;
    }
}

void __cdecl Write(Category eCategory, Severity eSeverity, char const* pcFile, int iLine,
                   char const* pcFormat, ...)
{
    if ((int)eCategory < 0 || (int)eCategory >= CategoryCount || (int)eSeverity < sVerbosity[eCategory])
    {
        return;
    }

    Lock();

    if (!sBaseSet)
    {
        QueryPerformanceFrequency(&sFreq);
        QueryPerformanceCounter(&sBase);
        sBaseSet = true;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    unsigned __int64 u64Ms = (unsigned __int64)(now.QuadPart - sBase.QuadPart) * 1000 / (unsigned __int64)sFreq.QuadPart;

    char acLine[1024];
    int iPrefix = _snprintf(acLine, sizeof(acLine) - 2, "[t=%u.%03u] [f=%d] %s %s %s:%d: ",
                            (unsigned int)(u64Ms / 1000), (unsigned int)(u64Ms % 1000), gFrame,
                            CategoryName(eCategory), SeverityName(eSeverity), TrimPath(pcFile), iLine);
    if (iPrefix < 0)
    {
        iPrefix = (int)sizeof(acLine) - 2;
    }
    acLine[iPrefix] = 0;

    va_list args;
    va_start(args, pcFormat);
    int iBody = _vsnprintf(acLine + iPrefix, sizeof(acLine) - 2 - iPrefix, pcFormat, args);
    va_end(args);
    if (iBody < 0 || iPrefix + iBody > (int)sizeof(acLine) - 2)
    {
        iBody = (int)sizeof(acLine) - 2 - iPrefix;
    }
    acLine[iPrefix + iBody] = 0;

    for (char* pc = acLine + iPrefix; *pc; ++pc)
    {
        if (*pc == '\r' || *pc == '\n')
        {
            *pc = ' ';
        }
    }

    acLine[iPrefix + iBody] = '\n';
    acLine[iPrefix + iBody + 1] = 0;

    if (gObserver)
    {
        gObserver((int)eSeverity, (int)eCategory, acLine);
    }

    RedWarning::SetLogData((RedWarning::RED_ERROR_SEVERITY)eSeverity);
    RedWarning::LogMessage("%s", acLine);

    if (sStdout)
    {
        DWORD dwWritten = 0;
        WriteFile(sStdout, acLine, (DWORD)(iPrefix + iBody + 1), &dwWritten, 0);
    }

    Unlock();
}

void NextFrame()
{
    ++gFrame;
}

void ParseCmds(char const* pcArgs)
{
    if (!pcArgs)
    {
        return;
    }

    char acCopy[512];
    strncpy(acCopy, pcArgs, sizeof(acCopy) - 1);
    acCopy[sizeof(acCopy) - 1] = 0;

    char* pcPiece = acCopy;
    while (pcPiece)
    {
        char* pcComma = strchr(pcPiece, ',');
        if (pcComma)
        {
            *pcComma = 0;
        }

        char acPiece[128];
        strncpy(acPiece, pcPiece, sizeof(acPiece) - 1);
        acPiece[sizeof(acPiece) - 1] = 0;

        char* pcCat = strtok(pcPiece, " \t");
        char* pcSev = pcCat ? strtok(0, " \t") : 0;
        char* pcExtra = pcSev ? strtok(0, " \t") : 0;

        int iCat = -1;
        int iSev = -1;
        bool bAll = false;
        if (pcCat && pcSev && !pcExtra)
        {
            if (_stricmp(pcCat, "all") == 0)
            {
                bAll = true;
            }
            for (int i = 0; i < CategoryCount; ++i)
            {
                if (_stricmp(pcCat, sCategoryNames[i]) == 0)
                {
                    iCat = i;
                }
            }
            for (int i = Verbose; i <= Fatal; ++i)
            {
                if (_stricmp(pcSev, sSeverityNames[i]) == 0)
                {
                    iSev = i;
                }
            }
        }

        if ((bAll || iCat >= 0) && iSev >= 0)
        {
            for (int i = 0; i < CategoryCount; ++i)
            {
                if (bAll || i == iCat)
                {
                    sVerbosity[i] = iSev;
                }
            }
        }
        else
        {
            char* pcStart = acPiece;
            while (*pcStart == ' ' || *pcStart == '\t')
            {
                ++pcStart;
            }
            if (*pcStart)
            {
                size_t uLen = strlen(sDeferred);
                if (uLen && uLen + 2 < sizeof(sDeferred))
                {
                    sDeferred[uLen++] = ',';
                    sDeferred[uLen++] = ' ';
                    sDeferred[uLen] = 0;
                }
                strncat(sDeferred, pcStart, sizeof(sDeferred) - 1 - strlen(sDeferred));
            }
        }

        pcPiece = pcComma ? pcComma + 1 : 0;
    }
}

int MinVerbosity()
{
    int iMin = Fatal;
    for (int i = 0; i < CategoryCount; ++i)
    {
        if (sVerbosity[i] < iMin)
        {
            iMin = sVerbosity[i];
        }
    }
    return iMin;
}

void Apply(char const* pcBuildId)
{
    RedWarning::SetSeverityToDestination((unsigned int)RED_LOG_DESTINATION_FILE,
                                         (RedWarning::RED_ERROR_SEVERITY)MinVerbosity());

    SYSTEMTIME st;
    GetLocalTime(&st);
    LOG(LogCore, Inform, "log start %04d-%02d-%02d %02d:%02d:%02d build %s", st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, pcBuildId ? pcBuildId : "?");

    if (sDeferred[0])
    {
        LOG(LogCore, Warning, "-logcmds ignored: %s", sDeferred);
    }

    gPblTraceSink = TraceSink;
}

bool EnsureFailed(char const* pcExpr, char const* pcFile, int iLine)
{
    Lock();
    ++sFailures;

    Site* pSite = 0;
    for (unsigned int i = 0; i < sSiteCount; ++i)
    {
        if (sSites[i].iLine == iLine && strcmp(sSites[i].pcFile, pcFile) == 0)
        {
            pSite = &sSites[i];
            break;
        }
    }

    bool bLogSite = false;
    bool bLogFull = false;
    if (pSite)
    {
        ++pSite->uCount;
    }
    else if (sSiteCount < 64)
    {
        pSite = &sSites[sSiteCount++];
        pSite->pcFile = pcFile;
        pSite->iLine = iLine;
        pSite->uCount = 1;
        bLogSite = true;
    }
    else if (!sTableFullLogged)
    {
        sTableFullLogged = true;
        bLogFull = true;
    }

    if (bLogSite)
    {
        Write(LogCore, Error, pcFile, iLine, "ensure failed: %s", pcExpr);
    }
    if (bLogFull)
    {
        Write(LogCore, Error, pcFile, iLine, "ensure table full");
    }

    Unlock();
    return false;
}

unsigned int EnsureFailureCount()
{
    return sFailures;
}

unsigned int EnsureSiteCount()
{
    return sSiteCount;
}

void LogEnsureSummary()
{
    if (sFailures)
    {
        Write(LogCore, Inform, "RedLog.cpp", 0, "ensure summary: %u failures at %u sites", sFailures, sSiteCount);
    }
}

void ResetForTest()
{
    Lock();
    memset(sSites, 0, sizeof(sSites));
    sSiteCount = 0;
    sFailures = 0;
    sTableFullLogged = false;
    for (int i = 0; i < CategoryCount; ++i)
    {
        sVerbosity[i] = Inform;
    }
    sDeferred[0] = 0;
    gFrame = 0;
    Unlock();
}

}
