#pragma once

namespace FrameLimiter
{

enum TimerKind
{
    kTimerNone = 0,
    kTimerPeriod = 1,
    kTimerHires = 2
};

struct FrameOps
{
    __int64 (*Now)(void* pCtx);
    void (*WaitTicks)(void* pCtx, __int64 iTicks);
    void* pCtx;
};

struct FramePlan
{
    __int64 iMinTicks;
    __int64 iMarginTicks;
    // OpenSWBF2: a frame this close to its minimum ends instead of waiting out the rest
    __int64 iSlackTicks;
};

// OpenSWBF2: the pacing plan for a timer kind. A headless run sleeps whole milliseconds and lets
// the last one go instead of spinning: a server or test client needs no finer pacing, and pcMain
// has set a 1 ms timer period for it.
FramePlan PlanFor(int iKind, bool bHeadless, __int64 iFrequency, unsigned int uiFps);

__int64 MinTicksForFps(__int64 iFrequency, unsigned int uiFps);

__int64 Pace(const FrameOps& ops, const FramePlan& plan, __int64 iOld, __int64& iNowOut);

int Init(bool bNoRender);

void Shutdown();

void EndPeriod();

int Kind();

const char* KindName(int iKind);

void BindReal(FrameOps& ops, FramePlan& plan, unsigned int uiFps);

}
