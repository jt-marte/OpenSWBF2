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

TEST_CASE("hash pin ifs.difficulty.overhaul" * doctest::test_suite("unit"))
{
    // RedLocalizeDB::Find answers this hash with "Overhaul"; the retail keys sit beside it.
    CHECK((unsigned int)PblHash("ifs.difficulty.overhaul") == 0xEA8E8A80);
    CHECK((unsigned int)PblHash("ifs.difficulty.medium") == 0x6B6CFA1B);
    CHECK((unsigned int)PblHash("ifs.difficulty.hard") == 0xB544ABF9);
}
