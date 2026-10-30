#include "third_party/doctest/doctest.h"

#include "PebbleFL/Common/PblHashTable.h"
#include "Battlefront2/Source/AITuning.h"

TEST_CASE("difficulty levels are easy to overhaul" * doctest::test_suite("regression"))
{
    CHECK(!AITuning::IsValidLevel(0));
    CHECK(AITuning::IsValidLevel(AITuning::kLevelEasy));
    CHECK(AITuning::IsValidLevel(AITuning::kLevelElite));
    CHECK(AITuning::IsValidLevel(AITuning::kLevelOverhaul));
    CHECK(!AITuning::IsValidLevel(5));
    CHECK(!AITuning::IsValidLevel(-1));
    CHECK(AITuning::ValidLevelOr(4, 2) == 4);
    CHECK(AITuning::ValidLevelOr(0, 2) == 2);
    CHECK(AITuning::ValidLevelOr(7, 2) == 2);
}

TEST_CASE("overhaul goes out to stock clients as elite" * doctest::test_suite("regression"))
{
    // The LAN query reply carries the level in 2 bits and stock clients show 1..3.
    CHECK(AITuning::WireLevel(1) == 1);
    CHECK(AITuning::WireLevel(2) == 2);
    CHECK(AITuning::WireLevel(3) == 3);
    CHECK(AITuning::WireLevel(4) == 3);
    CHECK(AITuning::WireLevel(0) == 1);
    CHECK(AITuning::WireLevel(9) == 3);
}

TEST_CASE("overhaul skill is one above retail elite" * doctest::test_suite("regression"))
{
    CHECK(AITuning::kOverhaulSkill == 18);
}

static AITuning::ThreatInputs SeenSoldier(float fMeters)
{
    AITuning::ThreatInputs in;
    in.iStatus = 3; // seen
    in.iVisualKey = (int)((21 - 10) * fMeters * 2.0f); // soldier on soldier, as GetVisualPriority
    in.fFacing = 0.0f;
    in.bRecentAttacker = false;
    in.fHealthFraction = 1.0f;
    in.bFlagCarrier = false;
    in.fSinceSeen = 0.0f;
    return in;
}

TEST_CASE("overhaul threat score prefers near enemies" * doctest::test_suite("regression"))
{
    const int iNear = AITuning::OverhaulThreatScore(SeenSoldier(20.0f));
    const int iFar = AITuning::OverhaulThreatScore(SeenSoldier(300.0f));
    CHECK(iNear > iFar + 10); // beyond the retail switch hysteresis
    CHECK(AITuning::OverhaulThreatScore(SeenSoldier(5000.0f)) == 154); // distance floors at 0
}

TEST_CASE("overhaul threat score has no idea who is human" * doctest::test_suite("regression"))
{
    // Retail: a human facing the bot beats any bot at any distance. Overhaul: facing helps
    // anyone, and not enough to beat a much nearer enemy.
    AITuning::ThreatInputs facingFar = SeenSoldier(250.0f);
    facingFar.fFacing = 1.0f;
    const AITuning::ThreatInputs awayNear = SeenSoldier(20.0f);
    CHECK(AITuning::OverhaulThreatScore(awayNear) > AITuning::OverhaulThreatScore(facingFar));

    AITuning::ThreatInputs facingNear = SeenSoldier(20.0f);
    facingNear.fFacing = 1.0f;
    CHECK(AITuning::OverhaulThreatScore(facingNear) > AITuning::OverhaulThreatScore(awayNear) + 10);
}

TEST_CASE("overhaul threat score turns on whoever just hurt the bot" * doctest::test_suite("regression"))
{
    AITuning::ThreatInputs shooter = SeenSoldier(60.0f);
    shooter.bRecentAttacker = true;
    const AITuning::ThreatInputs nearer = SeenSoldier(20.0f);
    CHECK(AITuning::OverhaulThreatScore(shooter) > AITuning::OverhaulThreatScore(nearer) + 10);

    AITuning::ThreatInputs hurt = SeenSoldier(60.0f);
    hurt.fHealthFraction = 0.2f;
    CHECK(AITuning::OverhaulThreatScore(hurt) > AITuning::OverhaulThreatScore(SeenSoldier(60.0f)));
}

TEST_CASE("overhaul threat score stays below a locked target and fades unseen" * doctest::test_suite("regression"))
{
    AITuning::ThreatInputs best = SeenSoldier(0.0f);
    best.fFacing = 1.0f;
    best.bRecentAttacker = true;
    best.fHealthFraction = 0.1f;
    best.bFlagCarrier = true;
    CHECK(AITuning::OverhaulThreatScore(best) == 254);

    AITuning::ThreatInputs old = SeenSoldier(20.0f);
    old.fSinceSeen = 3.0f;
    CHECK(AITuning::OverhaulThreatScore(old) == AITuning::OverhaulThreatScore(SeenSoldier(20.0f)) - 30);
    old.iStatus = 2; // forced sighting fades twice as fast
    CHECK(AITuning::OverhaulThreatScore(old) == AITuning::OverhaulThreatScore(SeenSoldier(20.0f)) - 18 - 60);
    old.fSinceSeen = 60.0f;
    CHECK(AITuning::OverhaulThreatScore(old) == 0);
}

TEST_CASE("hash pin ifs.difficulty.overhaul" * doctest::test_suite("unit"))
{
    // RedLocalizeDB::Find answers this hash with "Overhaul"; the retail keys sit beside it.
    CHECK((unsigned int)PblHash("ifs.difficulty.overhaul") == 0xEA8E8A80);
    CHECK((unsigned int)PblHash("ifs.difficulty.medium") == 0x6B6CFA1B);
    CHECK((unsigned int)PblHash("ifs.difficulty.hard") == 0xB544ABF9);
}
