#include "RedEngineFL/Application/pc/pcFrameLimiter.h"

#include <windows.h>

namespace FrameLimiter
{

namespace
{

typedef HANDLE(WINAPI* CreateWaitableTimerExFn)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);

const DWORD kHighResolutionFlag = 0x2;

bool sInitDone = false;
bool sHeadless = false;
int sKind = kTimerNone;
HANDLE sTimer = 0;
bool sPeriodBegun = false;
__int64 sFrequency = 0;

__int64 Frequency()
{
    if (sFrequency == 0)
    {
        LARGE_INTEGER li;
        li.QuadPart = 0;
        QueryPerformanceFrequency(&li);
        sFrequency = li.QuadPart > 0 ? li.QuadPart : 1;
    }
    return sFrequency;
}

__int64 RealNow(void*)
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

void RealWait(void*, __int64 iTicks)
{
    if (iTicks <= 0)
    {
        return;
    }
    const __int64 iFreq = Frequency();
    const __int64 iMs = iTicks * 1000 / iFreq;
    if (sKind == kTimerHires && sTimer != 0)
    {
        LARGE_INTEGER due;
        due.QuadPart = -(iTicks * 10000000 / iFreq);
        if (due.QuadPart == 0)
        {
            due.QuadPart = -1;
        }
        if (SetWaitableTimer(sTimer, &due, 0, 0, 0, FALSE))
        {
            WaitForSingleObject(sTimer, (DWORD)iMs + 50);
        }
    }
    else if (iMs >= 1)
    {
        Sleep((DWORD)iMs);
    }
}

}

__int64 MinTicksForFps(__int64 iFrequency, unsigned int uiFps)
{
    if (uiFps == 0)
    {
        return 1;
    }
    const __int64 iQuot = iFrequency / (__int64)uiFps;
    return iQuot < 1 ? 1 : iQuot;
}

__int64 Pace(const FrameOps& ops, const FramePlan& plan, __int64 iOld, __int64& iNowOut)
{
    const __int64 iNeed = plan.iMinTicks < 1 ? 1 : plan.iMinTicks;
    for (;;)
    {
        const __int64 iNow = ops.Now(ops.pCtx);
        const __int64 iDelta = iNow - iOld;
        if (iDelta < 0)
        {
            iNowOut = iNow;
            return iNeed;
        }
        if (iDelta >= iNeed)
        {
            iNowOut = iNow;
            return iDelta;
        }
        const __int64 iRemain = iNeed - iDelta;
        if (iRemain <= plan.iSlackTicks)
        {
            iNowOut = iNow;
            return iDelta;
        }
        if (iRemain > plan.iMarginTicks)
        {
            ops.WaitTicks(ops.pCtx, iRemain - plan.iMarginTicks);
        }
    }
}

int Init(bool bNoRender)
{
    if (sInitDone)
    {
        return sKind;
    }
    sInitDone = true;
    sKind = kTimerNone;
    sHeadless = bNoRender;
    if (bNoRender)
    {
        return sKind;
    }
    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    CreateWaitableTimerExFn pCreate =
        hKernel ? (CreateWaitableTimerExFn)GetProcAddress(hKernel, "CreateWaitableTimerExW") : 0;
    if (pCreate)
    {
        sTimer = pCreate(0, 0, kHighResolutionFlag, TIMER_ALL_ACCESS);
    }
    if (sTimer != 0)
    {
        sKind = kTimerHires;
    }
    else if (timeBeginPeriod(1) == TIMERR_NOERROR)
    {
        sPeriodBegun = true;
        sKind = kTimerPeriod;
    }
    return sKind;
}

void Shutdown()
{
    if (!sInitDone)
    {
        return;
    }
    if (sTimer != 0)
    {
        CloseHandle(sTimer);
        sTimer = 0;
    }
    EndPeriod();
    sKind = kTimerNone;
    sHeadless = false;
    sInitDone = false;
}

void EndPeriod()
{
    if (sPeriodBegun)
    {
        timeEndPeriod(1);
        sPeriodBegun = false;
    }
}

int Kind()
{
    return sKind;
}

const char* KindName(int iKind)
{
    switch (iKind)
    {
    case kTimerPeriod:
        return "period";
    case kTimerHires:
        return "hires";
    default:
        return "none";
    }
}

FramePlan PlanFor(int iKind, bool bHeadless, __int64 iFrequency, unsigned int uiFps)
{
    FramePlan plan;
    plan.iMinTicks = MinTicksForFps(iFrequency, uiFps);
    plan.iSlackTicks = 0;
    if (bHeadless)
    {
        plan.iMarginTicks = 0;
        plan.iSlackTicks = iFrequency / 1000;
    }
    else if (iKind == kTimerHires)
    {
        plan.iMarginTicks = iFrequency / 1000;
    }
    else if (iKind == kTimerPeriod)
    {
        plan.iMarginTicks = iFrequency * 2 / 1000;
    }
    else
    {
        plan.iMarginTicks = iFrequency * 20 / 1000;
    }
    return plan;
}

void BindReal(FrameOps& ops, FramePlan& plan, unsigned int uiFps)
{
    ops.Now = RealNow;
    ops.WaitTicks = RealWait;
    ops.pCtx = 0;
    plan = PlanFor(sKind, sHeadless, Frequency(), uiFps);
}

}
