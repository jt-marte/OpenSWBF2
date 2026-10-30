#pragma once

class Character;
class Controllable;
class GameObject;

// OpenSWBF2 Overhaul AI (difficulty level 4) state that the retail classes have no room for.
// Only called when AIDifficulty::IsOverhaul().
namespace AIOverhaul
{
// pAttacker hurt the bot riding pVictim.
void NoteAttacker(const Controllable* pVictim, const Character* pAttacker);
// pObject carries (drives, rides or is) someone who hurt pMe in the last few seconds.
bool IsRecentAttacker(const Controllable* pMe, const GameObject* pObject);
}
