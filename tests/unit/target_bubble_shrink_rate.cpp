#include "third_party/doctest/doctest.h"

#include <string>

#include "testutil.h"

TEST_CASE("ai difficulty compiles with fp strict" * doctest::test_suite("regression"))
{
    std::string sText;
    REQUIRE(ReadRepoText("BattlefrontII.vcxproj", sText));
    const std::string sItem = "<ClCompile Include=\"src\\Battlefront2\\Source\\AIDifficulty.cpp\">";
    const size_t uiItem = sText.find(sItem);
    REQUIRE(uiItem != std::string::npos);
    const size_t uiEnd = sText.find("</ClCompile>", uiItem);
    REQUIRE(uiEnd != std::string::npos);
    const std::string sBody = sText.substr(uiItem, uiEnd - uiItem);
    CHECK(sBody.find("<FloatingPointModel>Strict</FloatingPointModel>") != std::string::npos);
    CHECK(sText.find("/fp:") == std::string::npos);

    const std::string sModel = "<FloatingPointModel>";
    const std::string sInclude = "<ClCompile Include=\"";
    for (size_t uiAt = sText.find(sModel); uiAt != std::string::npos; uiAt = sText.find(sModel, uiAt + 1))
    {
        CHECK(sText.compare(uiAt, 47, "<FloatingPointModel>Strict</FloatingPointModel>") == 0);
        const size_t uiOwner = sText.rfind(sInclude, uiAt);
        REQUIRE(uiOwner != std::string::npos);
        const size_t uiName = uiOwner + sInclude.size();
        const size_t uiQuote = sText.find('"', uiName);
        REQUIRE(uiQuote != std::string::npos);
        const std::string sOwner = sText.substr(uiName, uiQuote - uiName);
        CHECK(sText.compare(uiQuote, 2, "\">") == 0);
        CHECK(sText.find("</ClCompile>", uiOwner) > uiAt);
        const bool bAllowed = sOwner == "src\\Battlefront2\\Source\\AIDifficulty.cpp" ||
                              sOwner == "src\\Battlefront2\\Source\\WeaponCannon.cpp";
        CHECK_MESSAGE(bAllowed, sOwner);
    }
}
