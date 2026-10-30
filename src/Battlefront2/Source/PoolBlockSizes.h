#pragma once

#include "PebbleFL/Common/PblRawStorage.h"

// OpenSWBF2: these pools hand out blocks as large as their largest subclass. Retail sized them
// with Win32 sizeof literals (GameLoop.cpp, AIUtil.cpp) and x64 objects are larger. NetState.cpp
// and AIUtil.cpp check every subclass fits.
#define WEAPON_POOL_BLOCK PBL_RAW_SIZE(0x1c0, 592)     // WeaponMelee, WeaponShield
#define ORDNANCE_POOL_BLOCK PBL_RAW_SIZE(384, 560)     // OrdnanceMeleeThrow
#define NAVIGATOR_POOL_BLOCK PBL_RAW_SIZE(0x1e8, 536)  // NavigatorFollowTarget
#define UNITAGENT_POOL_BLOCK PBL_RAW_SIZE(0x358, 1168) // UnitDefendAgent
#define AIGOAL_POOL_BLOCK PBL_RAW_SIZE(0x3c, 96)       // DefendGoal, DestroyGoal, FollowGoal
