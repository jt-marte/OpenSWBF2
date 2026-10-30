#include "third_party/doctest/doctest.h"

#include <string.h>
#include <string>

#include "testutil.h"
#include "Battlefront2/Source/SideNames.h"

TEST_CASE("side name prefix is bounded" * doctest::test_suite("regression"))
{
    REQUIRE(SideNamePrefix(-1) == SideNamePrefix(6));
    REQUIRE(SideNamePrefix(6) == SideNamePrefix(7));
    REQUIRE(SideNamePrefix(7) == SideNamePrefix(8));
    CHECK(SideNamePrefix(6)[0] == '\0');
    CHECK(strcmp(SideNamePrefix(0), "Neu") == 0);
    CHECK(strcmp(SideNamePrefix(1), "All") == 0);
    CHECK(strcmp(SideNamePrefix(2), "Imp") == 0);
    CHECK(strcmp(SideNamePrefix(3), "Rep") == 0);
    CHECK(strcmp(SideNamePrefix(4), "CIS") == 0);
    CHECK(strcmp(SideNamePrefix(5), "Loc") == 0);
}

TEST_CASE("final score uses the bounded side name" * doctest::test_suite("regression"))
{
    std::string sText;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\GameLoop.cpp", sText));
    CHECK(sText.find("SideNamePrefix(pTeam->mSide)") != std::string::npos);
    CHECK(sText.find("sSideNamePrefixes[") == std::string::npos);
}
