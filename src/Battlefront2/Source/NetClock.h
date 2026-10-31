#pragma once

// OpenSWBF2: the client clock's numbers at any turn rate. Retail counted them in turns tuned for
// 20 turns a second; at 120 the same counts are six times shorter, so the clock steered too
// weakly to keep up (a locked client moved 1 turn per 1.5 s, 5.6 ms a second) and a frame over
// 25 ms dropped turns for good, and the client's lead crept up to 40 turns on a quiet LAN. Each
// value here is retail's at 20 turns a second and scales with the rate. No engine dependencies,
// so the unit tests can call it.
namespace NetClock
{
// Turns a client may be off and still count as settled: retail's 3, 2, 1, 0 as it stays settled
// (150, 100, 50, 0 ms at 20), and never under 25 ms, where whole turns are too fine to hit.
inline int StableWindow(int iStableCount, int iTurnsPerSecond)
{
    const int iBase = iStableCount < 5 ? 3 : iStableCount < 10 ? 2 : iStableCount < 15 ? 1 : 0;
    const int iWindow = iBase * iTurnsPerSecond / 20;
    const int iFloor = iTurnsPerSecond / 40;
    return iWindow > iFloor ? iWindow : iFloor;
}

// Turns a settled clock moves per adjustment: retail's 1 at 20 (50 ms).
inline int LockedStep(int iTurnsPerSecond)
{
    const int iStep = iTurnsPerSecond / 20;
    return iStep > 1 ? iStep : 1;
}

// Turns one frame may submit or simulate: retail's 3 (client) and 4 (host) at 20, so a frame of
// 150 or 200 ms is caught up; beyond that the clock is reset anyway.
inline int MaxSubmitTurns(int iTurnsPerSecond)
{
    const int iTurns = iTurnsPerSecond * 3 / 20;
    return iTurns > 3 ? iTurns : 3;
}

inline int MaxSimTurns(int iTurnsPerSecond)
{
    const int iTurns = iTurnsPerSecond / 5;
    return iTurns > 4 ? iTurns : 4;
}

// How far ahead of the host a joining client starts: retail's 4 turns at 20 (200 ms).
inline int JoinLead(int iTurnsPerSecond)
{
    const int iTurns = iTurnsPerSecond / 5;
    return iTurns > 4 ? iTurns : 4;
}

// The host sends a client's lag in one byte (lag + 128); past it the value wrapped and steered
// the client the wrong way.
inline unsigned int LagByte(int iLag)
{
    return (unsigned int)((iLag < -128 ? -128 : iLag > 127 ? 127 : iLag) + 128);
}
}
