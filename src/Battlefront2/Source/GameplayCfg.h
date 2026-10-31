#pragma once

// OpenSWBF2: gameplay options that stand in for the HD Remaster's extras. They live in
// display.ini's [Gameplay] section (Options -> General writes them) and a switch overrides one
// for a single run: -aiheroes 0|1, -awardweapons 0|1, -awardeffects 0|1. No engine
// dependencies, so the unit tests can call it.
namespace GameplayCfg
{
enum Option
{
    kAiHeroes,     // Instant Action: a bot takes the hero when no human on its team does
    kAwardWeapons, // single player: award weapons unlock (stock behaviour)
    kAwardEffects, // the War Hero and Guardian glow shows (stock behaviour)
    kNumOptions
};

struct Settings
{
    bool abValue[kNumOptions];
    bool abCli[kNumOptions]; // set by a switch, so the ini leaves it alone this run
};

// The option a display.ini key or a switch (without its dash) names, case-insensitive; -1 if none.
int Find(const char* pcName);
const char* KeyName(int iOption);
bool Default(int iOption);
void SetDefaults(Settings& kSettings);
// Reads [Gameplay]; missing or bad values keep what kSettings had, and switch-set ones stay.
void LoadIni(Settings& kSettings, const char* pcPath);
bool SaveIni(int iOption, bool bValue, const char* pcPath);
// True when pcSwitch names an option and pcValue is 0 or 1.
bool ParseSwitch(Settings& kSettings, const char* pcSwitch, const char* pcValue);

// The running game's options, read from display.ini on first use.
bool Get(int iOption);
// Applies at once and saves to display.ini.
bool Set(int iOption, bool bValue);
// Award weapons are only turned off in single player: online the host and the clients would
// disagree about who holds which weapon.
bool AwardWeaponsOn(bool bNetGame);
// Whether a soldier's offense or defense buff shows its effect. A timer above 300 is the War
// Hero or Guardian award (UpdateBuffTimers holds it at 340); with award effects off that still
// buffs, just without the glow.
bool ShowBuffEffect(float fBuffTimer);
// For the command line, before anything reads an option.
bool ParseCommandLineSwitch(const char* pcSwitch, const char* pcValue);
}
