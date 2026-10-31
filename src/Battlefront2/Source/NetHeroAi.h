#pragma once

// OpenSWBF2: AI heroes in multiplayer. The hero goes to the team's humans first, as in retail;
// once it has waited kAiHeroWaitSeconds without one taking it, or none of them can (every one
// declined, or the team has none), one of the team's bots takes it, as in Instant Action. No
// engine dependencies, so the unit tests can call it.
namespace NetHeroAi
{
const float kAiHeroWaitSeconds = 30.0f;

// Whether a bot should take the hero now, after fWaitedSeconds with nobody taking it.
inline bool AiHeroDue(bool bAllDeclined, float fWaitedSeconds)
{
    return bAllDeclined || fWaitedSeconds >= kAiHeroWaitSeconds;
}

// Who gets the hero from the candidates found: 1 for a human, 0 for a bot, -1 for nobody. A bot
// only once one is due; until then (or with no bot to take it) the humans, as in retail.
inline int PickBucket(bool bAiDue, int iHumans, int iBots)
{
    if (bAiDue && iBots > 0)
    {
        return 0;
    }
    if (iHumans > 0)
    {
        return 1;
    }
    return iBots > 0 ? 0 : -1;
}
}
