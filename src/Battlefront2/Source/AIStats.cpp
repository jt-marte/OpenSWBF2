#include <windows.h>

#include "AIStats.h"

#include "AIDifficulty.h"
#include "AIUtil.h"
#include "Character.h"
#include "CommandLine.h"
#include "Controllable.h"
#include "GameLoop.h"
#include "GameObject.h"
#include "RedEngineFL/Util/RedLog.h"

namespace AIStats
{
namespace
{
struct Counters
{
    // Seconds of bot thinking spent aimed at a human or a bot. "Contested" seconds are the ones
    // where the bot had both a human and a bot enemy it could pick, split for vehicles/turrets.
    float fTargetHuman;
    float fTargetBot;
    float fContested;
    float fContestedHuman;
    float fVehContested;
    float fVehContestedHuman;
    int iKills[2][2]; // [killer is human][victim is human]
    double dFrameMs;
    double dFrameMsMax;
    int iFrames;
};

Counters sCounters;
LARGE_INTEGER sFrameStart;
float sNextReport = 30.0f;
bool sReported;

double TicksToMs(LONGLONG llTicks)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (double)llTicks * 1000.0 / (double)freq.QuadPart;
}
}

bool HasHuman(const GameObject* pObject)
{
    if (!pObject)
    {
        return false;
    }
    const PblList<Controllable>& list = pObject->GetControllableList();
    for (PblList<Controllable>::Itor it = list.Begin(); it; ++it)
    {
        const Controllable* pRider = it;
        if (pRider->mPlayerId >= 0)
        {
            return true;
        }
    }
    return false;
}

void OnThink(const Controllable* pMe, const GameObject* pTarget, bool bHumanSeen, bool bBotSeen, float fDelta)
{
    if (!gAIStatsLog || !pMe || fDelta <= 0.0f)
    {
        return;
    }
    if (fDelta > 5.0f)
    {
        fDelta = 5.0f;
    }
    const bool bHumanTarget = HasHuman(pTarget);
    if (pTarget)
    {
        (bHumanTarget ? sCounters.fTargetHuman : sCounters.fTargetBot) += fDelta;
    }
    if (bHumanSeen && bBotSeen)
    {
        const bool bVehicle = AIUtil::GetEntityType(pMe->GetEntity()) != AIUtil::ENTITY_SOLDIER;
        sCounters.fContested += fDelta;
        if (bVehicle)
        {
            sCounters.fVehContested += fDelta;
        }
        if (pTarget && bHumanTarget)
        {
            sCounters.fContestedHuman += fDelta;
            if (bVehicle)
            {
                sCounters.fVehContestedHuman += fDelta;
            }
        }
    }
}

void OnKill(const Character* pKiller, const Character* pVictim)
{
    if (!gAIStatsLog || !pKiller || !pVictim)
    {
        return;
    }
    ++sCounters.iKills[pKiller->mPlayerId >= 0 ? 1 : 0][pVictim->mPlayerId >= 0 ? 1 : 0];
}

void BeginFrame()
{
    if (gAIStatsLog)
    {
        QueryPerformanceCounter(&sFrameStart);
    }
}

void EndFrame()
{
    if (!gAIStatsLog)
    {
        return;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const double dMs = TicksToMs(now.QuadPart - sFrameStart.QuadPart);
    sCounters.dFrameMs += dMs;
    if (dMs > sCounters.dFrameMsMax)
    {
        sCounters.dFrameMsMax = dMs;
    }
    ++sCounters.iFrames;
    if (GameLoop::GetMissionTime() >= sNextReport)
    {
        sNextReport = GameLoop::GetMissionTime() + 30.0f;
        Report();
    }
}

void Report()
{
    if (!gAIStatsLog)
    {
        return;
    }
    const Counters& c = sCounters;
    const float fTargeted = c.fTargetHuman + c.fTargetBot;
    LOG(LogSim, Inform,
        "OPENSWBF2 AI STATS t=%.0f level=%d target_human=%.0f%% (%.0fs of %.0fs) contested_human=%.0f%% "
        "(%.0fs) veh_contested_human=%.0f%% (%.0fs) kills bot>bot=%d bot>human=%d human>bot=%d "
        "ai_ms=%.3f max=%.2f",
        GameLoop::GetMissionTime(), AIDifficulty::GetProfileDifficulty(),
        fTargeted > 0.0f ? 100.0f * c.fTargetHuman / fTargeted : 0.0f, c.fTargetHuman, fTargeted,
        c.fContested > 0.0f ? 100.0f * c.fContestedHuman / c.fContested : 0.0f, c.fContested,
        c.fVehContested > 0.0f ? 100.0f * c.fVehContestedHuman / c.fVehContested : 0.0f, c.fVehContested,
        c.iKills[0][0], c.iKills[0][1], c.iKills[1][0], c.iFrames ? c.dFrameMs / c.iFrames : 0.0,
        c.dFrameMsMax);
}
}
