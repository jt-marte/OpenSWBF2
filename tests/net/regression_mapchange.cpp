#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "testutil.h"
#include "nettest.h"

// OpenSWBF2: a dedicated server crashed in NetPktGroup::PopFront a couple of seconds into the map
// after an rcon /restart. The late-state ring had grown to 32 slots but the map-change cleanup
// still freed 8, so the rest kept links into a packet pool the next mission rebuilt.

namespace
{

std::string ReadSource(const char* pcPath)
{
    std::string sOut;
    FILE* pFile = fopen(pcPath, "rb");
    REQUIRE(pFile);
    int c;
    while ((c = fgetc(pFile)) != EOF)
    {
        if (c != '\r')
        {
            sOut += (char)c;
        }
    }
    fclose(pFile);
    return sOut;
}

} // namespace

TEST_CASE("a packet group whose count runs past its list is dropped and not followed" *
          doctest::test_suite("regression"))
{
    ExpectLog expect("OPENSWBF2 NET packet group corrupt");
    NetTestReset();
    LogCapture capture;

    unsigned char acPayload[4] = {1, 2, 3, 4};
    NetPktGroup group;
    group.Reset();
    group.PushBack(NetMakePacket(acPayload, 4));
    group.pktCount = 3; // what a stale group looks like after the pool was rebuilt

    CHECK(!group.PopFront());
    CHECK(group.pktCount == 0);
    CHECK(group.firstPkt == -1);
    CHECK(group.lastPkt == -1);
    CHECK(capture.Contains("OPENSWBF2 NET packet group corrupt"));
}

TEST_CASE("a new packet group starts empty" * doctest::test_suite("regression"))
{
    NetPktGroup group;
    CHECK(group.pktCount == 0);
    CHECK(group.firstPkt == -1);
    CHECK(group.lastPkt == -1);
    CHECK(!group.PopFront());
}

TEST_CASE("a map change clears the whole late-state ring" * doctest::test_suite("regression"))
{
    const std::string sNet = ReadSource("../src/Battlefront2/Source/NetGame.cpp");

    // Both cleanups cover every slot, and nothing hard-codes retail's 8 any more
    CHECK(sNet.find("for (unsigned int i = 0; i < kLateRing; i++)\n    {\n        lateState[i].Free();") !=
          std::string::npos);
    CHECK(sNet.find("for (unsigned int i = 0; i < kLateRing; ++i)\n        {\n            lateState[i].Free();") !=
          std::string::npos);
    CHECK(sNet.find("i < 8; i++)\n    {\n        lateState") == std::string::npos);
    CHECK(sNet.find("i < 8; ++i)\n        {\n            lateState") == std::string::npos);

    // ...and whatever survived is forgotten right after the pool is rebuilt
    const size_t uCreate = sNet.find("NetComm::Create(bMission);");
    const size_t uForget = sNet.find("lateState[i].Reset();");
    REQUIRE(uCreate != std::string::npos);
    REQUIRE(uForget != std::string::npos);
    CHECK(uCreate < uForget);
    CHECK(uForget < sNet.find("NetSetup::Create(bMission);"));
}

TEST_CASE("an admin keeps its session and pushes across a map change" * doctest::test_suite("regression"))
{
    const std::string sAdmin = ReadSource("../src/Battlefront2/Source/NetAdmin.cpp");

    // The clock restarts each mission: times from the last map count as just now, instead of
    // retail's dropping the session
    CHECK(sAdmin.find("static void RebaseAdminTimes(float fNow)") != std::string::npos);
    CHECK(sAdmin.find("const float fNow = NetComm::GetTime();\n    RebaseAdminTimes(fNow);") != std::string::npos);
    CHECK(sAdmin.find("admin.lastRecvTime = 0.0f;") == std::string::npos);

    // /restart replays the running map's own entry
    const std::string sList = ReadSource("../src/Battlefront2/Source/MissionPlayList.cpp");
    CHECK(sList.find("sPlaylistPosn = siCurrentEntry;") != std::string::npos);

    // Score pushes name the teams for maps from the command line or rcon too
    CHECK(sAdmin.find("pusTeam1 = Team::sTeams[1]->GetName();") != std::string::npos);
}

TEST_CASE("nobody keeps a deleted vehicle and bots made from players get the right team" *
          doctest::test_suite("regression"))
{
    // A dedicated server crashed in Character::Update a few seconds after a map change: four bot
    // gunners kept their multi-seat vehicle after it was deleted
    const std::string sCtl = ReadSource("../src/Battlefront2/Source/Controllable.cpp");
    const size_t uDtor = sCtl.find("Controllable::~Controllable()");
    REQUIRE(uDtor != std::string::npos);
    CHECK(sCtl.find("Character::ForgetControllable(this);", uDtor) < sCtl.find("void Controllable::Kill()"));

    const std::string sChar = ReadSource("../src/Battlefront2/Source/Character.cpp");
    CHECK(sChar.find("void Character::ForgetControllable(const Controllable* pControllable)") != std::string::npos);
    // MakeAI picks the team from the index in elements, not Win32's 432-byte stride
    CHECK(sChar.find("const int iIndex = pBase ? (int)(this - pBase)") != std::string::npos);

    // A script mod's skin naming a low-res model the map didn't load keeps the old one (it
    // crashed a dedicated server loading nab2c_con)
    const std::string sSoldier = ReadSource("../src/Battlefront2/Source/EntitySoldier.cpp");
    CHECK(sSoldier.find("if (pLowRes)\n        {\n            mModelLowRes = pLowRes;") != std::string::npos);
    CHECK(sSoldier.find("mModelLowRes ? mModelLowRes->GetRedModel(0) : 0") != std::string::npos);
}
