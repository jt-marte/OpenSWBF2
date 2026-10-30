#include "AITuning.h"

namespace AITuning
{

bool IsValidLevel(int iLevel)
{
    return iLevel >= kLevelEasy && iLevel <= kLevelOverhaul;
}

int ValidLevelOr(int iLevel, int iFallback)
{
    return IsValidLevel(iLevel) ? iLevel : iFallback;
}

int WireLevel(int iLevel)
{
    if (iLevel < kLevelEasy)
    {
        return kLevelEasy;
    }
    return iLevel > kLevelElite ? kLevelElite : iLevel;
}

float OverhaulPostWeight(float fImportance, float fDistance, int iTeammatesThere, int iTeamSize)
{
    if (fImportance <= 0.0f)
    {
        return 0.0f;
    }
    if (fDistance < 0.0f)
    {
        fDistance = 0.0f;
    }
    float fWeight = fImportance * 150.0f / (150.0f + fDistance);
    fWeight /= 1.0f + 0.5f * (float)iTeammatesThere;
    if (iTeamSize > 0 && 2 * iTeammatesThere >= iTeamSize)
    {
        fWeight *= 0.25f;
    }
    return fWeight;
}

bool ShouldRetreat(float fMyHealth, float fEnemyHealth)
{
    return fMyHealth < kRetreatHealth && fEnemyHealth > fMyHealth;
}

unsigned int PatternSeed(unsigned int uBotSeed, unsigned int uTargetId, float fMissionTime)
{
    unsigned int uHash = 2166136261u; // FNV-1a over the three words
    const unsigned int auWords[3] = { uBotSeed, uTargetId, (unsigned int)(fMissionTime / 15.0f) };
    for (int i = 0; i < 3; ++i)
    {
        for (int b = 0; b < 4; ++b)
        {
            uHash ^= (auWords[i] >> (8 * b)) & 0xFFu;
            uHash *= 16777619u;
        }
    }
    return uHash;
}

int OverhaulThreatScore(const ThreatInputs& in)
{
    // Status as retail: 100 + 18 per step, so a seen enemy outranks an unseen one.
    int iScore = 18 * in.iStatus + 100;

    // Distance, six times steeper than retail's 100 - key / 600 so that near enemies matter: an
    // enemy soldier at 20 m keeps 96, at 100 m 78, at 250 m 45.
    int iVisual = 100 - in.iVisualKey / 100;
    if (iVisual < 0)
    {
        iVisual = 0;
    }

    // An enemy facing me counts fully, one looking away 60%.
    float fFace = (in.fFacing - 0.6f) / 0.4f;
    if (fFace < 0.0f)
    {
        fFace = 0.0f;
    }
    else if (fFace > 1.0f)
    {
        fFace = 1.0f;
    }
    iVisual = (int)((float)iVisual * (0.6f + 0.4f * fFace));

    if (in.bRecentAttacker)
    {
        iVisual += 35;
    }
    if (in.fHealthFraction < 0.35f)
    {
        iVisual += 10;
    }
    if (in.bFlagCarrier && iVisual < 100)
    {
        iVisual = 100;
    }
    iScore += iVisual;

    // Fade as retail: 10 a second since last seen, twice that for a forced sighting.
    if (in.iStatus != 0)
    {
        iScore -= (int)(in.fSinceSeen * 10.0f);
    }
    if (in.iStatus == 2)
    {
        iScore -= (int)(in.fSinceSeen * 10.0f);
    }
    if (iScore < 0)
    {
        return 0;
    }
    return iScore > 254 ? 254 : iScore;
}

}
