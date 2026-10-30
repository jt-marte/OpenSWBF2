#pragma once

class Character;
class Controllable;
class GameObject;

// OpenSWBF2 -aistats: counters for judging how the AI picks targets and how fights go, logged
// as one "OPENSWBF2 AI STATS" line every 30 s of mission time and once at the end. Host only:
// clients run no AI. Every call is a no-op without the switch.
namespace AIStats
{
// One high-level think of a bot: who it is aiming at and which kinds of enemy it could pick.
void OnThink(const Controllable* pMe, const GameObject* pTarget, bool bHumanSeen, bool bBotSeen, float fDelta);
void OnKill(const Character* pKiller, const Character* pVictim);
void BeginFrame();
void EndFrame();
void Report();
bool HasHuman(const GameObject* pObject);
}
