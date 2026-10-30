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

}
