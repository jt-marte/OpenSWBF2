#include "third_party/doctest/doctest.h"

#include <string>

#include "testutil.h"

TEST_CASE("weapon cannon fire compiles with fp strict" * doctest::test_suite("regression"))
{
    std::string sText;
    REQUIRE(ReadRepoText("BattlefrontII.vcxproj", sText));
    const std::string sItem = "<ClCompile Include=\"src\\Battlefront2\\Source\\WeaponCannon.cpp\">\n"
                              "      <FloatingPointModel>Strict</FloatingPointModel>\n"
                              "    </ClCompile>";
    const size_t uiItem = sText.find(sItem);
    CHECK(uiItem != std::string::npos);
    CHECK(sText.find(sItem, uiItem + 1) == std::string::npos);
    CHECK(sText.find("<ClCompile Include=\"src\\Battlefront2\\Source\\WeaponCannon.cpp\" />") == std::string::npos);
}
