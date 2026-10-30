#include "third_party/doctest/doctest.h"

#include <limits.h>
#include <string>

#include "testutil.h"
#include "Battlefront2/Source/AIDifficultyLevel.h"

namespace
{

int CountText(const std::string& sText, const char* pcNeedle)
{
    int iCount = 0;
    const std::string sNeedle(pcNeedle);
    for (size_t uiAt = sText.find(sNeedle); uiAt != std::string::npos; uiAt = sText.find(sNeedle, uiAt + 1))
    {
        ++iCount;
    }
    return iCount;
}

}

TEST_CASE("ai difficulty level is clamped to the table range including Overhaul" * doctest::test_suite("regression"))
{
    CHECK(ClampDifficultyLevel(1) == 1);
    CHECK(ClampDifficultyLevel(2) == 2);
    CHECK(ClampDifficultyLevel(3) == 3);
    CHECK(ClampDifficultyLevel(0) == 1);
    CHECK(ClampDifficultyLevel(-1) == 1);
    CHECK(ClampDifficultyLevel(INT_MIN) == 1);
    CHECK(ClampDifficultyLevel(4) == 4); // Overhaul
    CHECK(ClampDifficultyLevel(5) == 4);
    CHECK(ClampDifficultyLevel(100) == 4);
    CHECK(ClampDifficultyLevel(INT_MAX) == 4);
}

TEST_CASE("ai difficulty reads go through the clamp" * doctest::test_suite("regression"))
{
    std::string sDifficulty;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\AIDifficulty.cpp", sDifficulty));
    CHECK(sDifficulty.find("#include \"AIDifficultyLevel.h\"") != std::string::npos);
    CHECK(sDifficulty.find("return ClampDifficultyLevel(mProfileDifficulty);") != std::string::npos);
    CHECK(sDifficulty.find("__declspec(noinline) int AIDifficulty::GetProfileDifficulty()") != std::string::npos);
    CHECK(CountText(sDifficulty, "mProfileDifficulty") == 4);
    CHECK(sDifficulty.find("int mProfileDifficulty;") != std::string::npos);
    CHECK(sDifficulty.find("mProfileDifficulty = PlayerPrefs::mAllUserPrefs[0].mDifficulty;") != std::string::npos);
    CHECK(sDifficulty.find("mProfileDifficulty = PlayerProfile::GetProfile(0)->mGameOptions.mCurDiffLevel;") != std::string::npos);

    std::string sController;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\ControllerManager.cpp", sController));
    CHECK(CountText(sController, "mProfileDifficulty") == 1);
    CHECK(sController.find("AIDifficulty::mProfileDifficulty = iDifficulty;") != std::string::npos);

    std::string sCombat;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\CombatUtil.cpp", sCombat));
    CHECK(CountText(sCombat, "mProfileDifficulty") == 0);

    std::string sDamageable;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\Damageable.cpp", sDamageable));
    CHECK(CountText(sDamageable, "mProfileDifficulty") == 0);
}
