#pragma once

inline int ClampDifficultyLevel(int iLevel)
{
    if (iLevel < 1)
    {
        return 1;
    }
    // this fork adds Overhaul as level 4 (AITuning::kLevelOverhaul); retail tables such as
    // CombatUtil's kWeights[4][4] already have a fourth row
    if (iLevel > 4)
    {
        return 4;
    }
    return iLevel;
}
