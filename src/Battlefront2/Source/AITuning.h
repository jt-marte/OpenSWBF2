#pragma once

// OpenSWBF2 AI tuning with no engine dependencies, so the unit tests can call it.
namespace AITuning
{
// Profile difficulty levels. Retail has Easy, Normal and Elite (the PC menus show only Normal
// and Elite); OpenSWBF2 adds Overhaul, which turns on the reworked AI.
const int kLevelEasy = 1;
const int kLevelNormal = 2;
const int kLevelElite = 3;
const int kLevelOverhaul = 4;

// Skill on the 1..20 scale for both teams on Overhaul. Retail Elite is 17 for the enemy and 11
// for the player's AI allies.
const int kOverhaulSkill = 18;

bool IsValidLevel(int iLevel);
// iLevel when it is a valid level, otherwise iFallback.
int ValidLevelOr(int iLevel, int iFallback);
// The level as stock clients understand it (1..3): Overhaul is sent as Elite.
int WireLevel(int iLevel);
}
