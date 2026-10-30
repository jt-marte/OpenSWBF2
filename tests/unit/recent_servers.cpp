#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string.h>

#include "PebbleFL/Common/PblHashTable.h"
#include "Battlefront2/Source/RecentServers.h"

TEST_CASE("recent servers: one address however it was typed" * doctest::test_suite("regression"))
{
    char acOut[RecentServers::kAddrMax];
    REQUIRE(RecentServers::Normalize("  1.2.3.4:3658 ", acOut, sizeof(acOut)));
    CHECK(strcmp(acOut, "1.2.3.4") == 0);
    REQUIRE(RecentServers::Normalize("Play.Example.COM:3700", acOut, sizeof(acOut)));
    CHECK(strcmp(acOut, "play.example.com:3700") == 0);
    REQUIRE(RecentServers::Normalize("10.0.0.5:36580", acOut, sizeof(acOut)));
    CHECK(strcmp(acOut, "10.0.0.5:36580") == 0); // not the default port, just ends like it
    CHECK(!RecentServers::Normalize("", acOut, sizeof(acOut)));
    CHECK(!RecentServers::Normalize("   ", acOut, sizeof(acOut)));
    CHECK(!RecentServers::Normalize(":3658", acOut, sizeof(acOut)));
    CHECK(!RecentServers::Normalize("1.2.3.4\tx", acOut, sizeof(acOut)));
    CHECK(!RecentServers::Normalize("a b", acOut, sizeof(acOut)));
    CHECK(!RecentServers::Normalize(0, acOut, sizeof(acOut)));
}

TEST_CASE("recent servers: newest first with no duplicates and eight at most" * doctest::test_suite("regression"))
{
    RecentServers::List list;
    RecentServers::Clear(list);
    CHECK(RecentServers::Add(list, "1.1.1.1", "First"));
    CHECK(RecentServers::Add(list, "2.2.2.2", "Second"));
    CHECK(RecentServers::Add(list, "1.1.1.1:3658", "")); // same server: moves up, keeps its name
    REQUIRE(list.miCount == 2);
    CHECK(strcmp(list.maEntries[0].acAddr, "1.1.1.1") == 0);
    CHECK(strcmp(list.maEntries[0].acName, "First") == 0);
    CHECK(strcmp(list.maEntries[1].acAddr, "2.2.2.2") == 0);
    CHECK(RecentServers::Add(list, "2.2.2.2", "Renamed"));
    CHECK(strcmp(list.maEntries[0].acName, "Renamed") == 0);
    CHECK(!RecentServers::Add(list, " ", "Nothing"));
    CHECK(list.miCount == 2);

    char acAddr[16];
    for (int i = 0; i < 10; ++i)
    {
        sprintf(acAddr, "10.0.0.%d", i);
        CHECK(RecentServers::Add(list, acAddr, "x"));
    }
    CHECK(list.miCount == RecentServers::kMaxServers);
    CHECK(strcmp(list.maEntries[0].acAddr, "10.0.0.9") == 0);
    CHECK(strcmp(list.maEntries[7].acAddr, "10.0.0.2") == 0);
}

TEST_CASE("recent servers: names can't break the file's lines" * doctest::test_suite("regression"))
{
    RecentServers::List list;
    RecentServers::Clear(list);
    CHECK(RecentServers::Add(list, "1.2.3.4", "  My\tServer\r\nName  "));
    CHECK(strcmp(list.maEntries[0].acName, "My Server Name") == 0);
    char acLong[200];
    memset(acLong, 'n', sizeof(acLong) - 1);
    acLong[sizeof(acLong) - 1] = 0;
    CHECK(RecentServers::Add(list, "5.6.7.8", acLong));
    CHECK(strlen(list.maEntries[0].acName) == RecentServers::kNameMax - 1);
}

TEST_CASE("recent servers: the file round-trips and skips junk" * doctest::test_suite("regression"))
{
    RecentServers::List list;
    RecentServers::Clear(list);
    RecentServers::Add(list, "2.2.2.2", "Old");
    RecentServers::Add(list, "1.1.1.1:3700", "New");
    char acText[RecentServers::kTextMax];
    const int iLen = RecentServers::Serialize(list, acText, sizeof(acText));
    CHECK(iLen == (int)strlen("1.1.1.1:3700\tNew\r\n2.2.2.2\tOld\r\n"));
    CHECK(strcmp(acText, "1.1.1.1:3700\tNew\r\n2.2.2.2\tOld\r\n") == 0);

    RecentServers::List back;
    RecentServers::Parse(acText, back);
    REQUIRE(back.miCount == 2);
    CHECK(strcmp(back.maEntries[0].acAddr, "1.1.1.1:3700") == 0);
    CHECK(strcmp(back.maEntries[1].acName, "Old") == 0);

    RecentServers::Parse("\n\n   \nbad addr\tx\n3.3.3.3\n3.3.3.3:3658\tdup\n4.4.4.4\tFour", back);
    REQUIRE(back.miCount == 2);
    CHECK(strcmp(back.maEntries[0].acAddr, "3.3.3.3") == 0);
    CHECK(strcmp(back.maEntries[0].acName, "") == 0);
    CHECK(strcmp(back.maEntries[1].acName, "Four") == 0);

    RecentServers::Parse(0, back);
    CHECK(back.miCount == 0);
    CHECK(RecentServers::Serialize(list, acText, 8) == -1);
}

TEST_CASE("hash pins: Modern and Legacy and Direct Connect text" * doctest::test_suite("unit"))
{
    // RedLocalizeDB::Find answers these hashes with OpenSWBF2's own text.
    CHECK((unsigned int)PblHash("ifs.mp.join.modern") == 0x834AC275);
    CHECK((unsigned int)PblHash("ifs.mp.join.legacy") == 0x7F0595E1);
    CHECK((unsigned int)PblHash("ifs.mp.tabs.direct") == 0x3AB6DBF7);
    CHECK((unsigned int)PblHash("ifs.mp.direct.address") == 0x2EE8282B);
    CHECK((unsigned int)PblHash("ifs.mp.direct.password") == 0x6FBB2C30);
    CHECK((unsigned int)PblHash("ifs.mp.direct.join") == 0x1CF8E6C1);
    CHECK((unsigned int)PblHash("ifs.mp.direct.recent") == 0xBC7DD8B6);
    CHECK((unsigned int)PblHash("ifs.mp.direct.none") == 0x6A6BC3B3);
    CHECK((unsigned int)PblHash("ifs.mp.direct.needpass") == 0x7E6A8238);
}
