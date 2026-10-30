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

// Overhaul threat score. Retail halves every bot enemy's score and lets a human facing the bot
// beat any bot at any distance; Overhaul scores every enemy, human or bot, by the same rules.
struct ThreatInputs
{
    int iStatus;           // AI::ThreatStatus (0 unknown, 1 unseen, 2 forced seen, 3 seen)
    int iVisualKey;        // VisionManager::GetVisualPriority: type weight times distance
    float fFacing;         // the enemy's forward axis dotted with the direction to me
    bool bRecentAttacker;  // the enemy damaged me in the last few seconds
    float fHealthFraction; // the enemy's health, 0..1
    bool bFlagCarrier;
    float fSinceSeen;      // seconds since the enemy was last seen
};
// 0..254 (255 stays the retail score of a scripted, locked target).
int OverhaulThreatScore(const ThreatInputs& in);
// Seconds an attacker counts as recent for the score above.
const float kRecentAttackerSeconds = 4.0f;

// Overhaul aim: retail shrinks the miss "bubble" to 0.3 against bots and leaves it whole against
// humans, so bots snipe each other and spray at players. Overhaul uses one scale for every target.
const float kOverhaulBubbleScale = 0.6f;
}
