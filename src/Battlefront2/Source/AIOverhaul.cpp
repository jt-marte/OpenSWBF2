#include "AIOverhaul.h"

#include "AITuning.h"
#include "Character.h"
#include "Controllable.h"
#include "GameLoop.h"
#include "GameObject.h"

namespace AIOverhaul
{
namespace
{
// Indexed by the victim's slot in Character::sCharacters (at most 103 in a 64-player game).
const int kMaxCharacters = 256;

struct Attacker
{
    const Character* pCharacter;
    float fTime;
};

Attacker sLastAttacker[kMaxCharacters];

int SlotOf(const Character* pCharacter)
{
    if (!pCharacter || !Character::sCharacters)
    {
        return -1;
    }
    const int iSlot = (int)(pCharacter - Character::sCharacters);
    return iSlot >= 0 && iSlot < kMaxCharacters ? iSlot : -1;
}
}

void NoteAttacker(const Controllable* pVictim, const Character* pAttacker)
{
    const int iSlot = pVictim ? SlotOf(pVictim->mCharacter) : -1;
    if (iSlot < 0 || !pAttacker)
    {
        return;
    }
    sLastAttacker[iSlot].pCharacter = pAttacker;
    sLastAttacker[iSlot].fTime = GameLoop::GetMissionTime();
}

bool IsRecentAttacker(const Controllable* pMe, const GameObject* pObject)
{
    const int iSlot = pMe ? SlotOf(pMe->mCharacter) : -1;
    if (iSlot < 0 || !pObject || !sLastAttacker[iSlot].pCharacter)
    {
        return false;
    }
    // A new mission restarts the clock, which also retires the last mission's entries.
    const float fAge = GameLoop::GetMissionTime() - sLastAttacker[iSlot].fTime;
    if (fAge < 0.0f || fAge > AITuning::kRecentAttackerSeconds)
    {
        return false;
    }
    const PblList<Controllable>& list = pObject->GetControllableList();
    for (PblList<Controllable>::Itor it = list.Begin(); it; ++it)
    {
        const Controllable* pRider = it;
        if (pRider->mCharacter == sLastAttacker[iSlot].pCharacter)
        {
            return true;
        }
    }
    return false;
}
}
