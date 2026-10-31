#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "PebbleFL/Common/PblHashTable.h"
#include "Battlefront2/Source/NetWire.h"
#include "Battlefront2/Source/NetLobby.h"
#include "Battlefront2/Source/NetAdminText.h"
#include "Battlefront2/Source/MissionPlayListOps.h"

// OpenSWBF2's server manager, game side: the lobby, rcon sessions that are safe to expose, the
// rotation edits and reply text a launcher drives, and the wiring into the engine.

namespace
{

// A NetSocketPeerRecord: an 8-byte Galaxy ID, then a sockaddr_in (family, port in network order,
// address).
void PeerRecord(unsigned char* pRecord, unsigned long long uGalaxyId, unsigned int uIP, unsigned int uPort)
{
    memset(pRecord, 0, 24);
    memcpy(pRecord, &uGalaxyId, 8);
    pRecord[8] = 2; // AF_INET
    pRecord[10] = (unsigned char)(uPort >> 8);
    pRecord[11] = (unsigned char)uPort;
    memcpy(pRecord + 12, &uIP, 4);
}

std::string ReadScript(const char* pcPath)
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

bool Has(const std::string& sText, const char* pcNeedle)
{
    return sText.find(pcNeedle) != std::string::npos;
}

}

TEST_CASE("rcon sessions are told apart by address on LAN" * doctest::test_suite("unit"))
{
    unsigned char aA[24];
    unsigned char aB[24];
    unsigned char aC[24];
    PeerRecord(aA, 0, 0x0100007f, 50000); // 127.0.0.1:50000
    PeerRecord(aB, 0, 0x0100007f, 50001); // same IP, another port
    PeerRecord(aC, 0, 0x0201a8c0, 50000); // 192.168.1.2:50000

    // retail compared only the Galaxy ID, 0 for every LAN peer: one shared session
    CHECK(NetWire::RconPeerKey(aA, true) != NetWire::RconPeerKey(aB, true));
    CHECK(NetWire::RconPeerKey(aA, true) != NetWire::RconPeerKey(aC, true));
    CHECK(NetWire::RconPeerKey(aA, true) == NetWire::RconPeerKey(aA, true));
    CHECK((unsigned int)NetWire::RconPeerKey(aA, true) == 0x0100007fu);
    CHECK((unsigned int)(NetWire::RconPeerKey(aA, true) >> 32) == 50000u);

    // the login limit counts per IP, so changing port doesn't buy more tries, and is never off
    CHECK(NetWire::RconLoginKey(aA, true) == NetWire::RconLoginKey(aB, true));
    CHECK(NetWire::RconLoginKey(aA, true) != NetWire::RconLoginKey(aC, true));
    unsigned char aZero[24];
    PeerRecord(aZero, 0, 0, 0);
    CHECK(NetWire::RconLoginKey(aZero, true) != 0u);

    // online, the Galaxy ID is the peer
    unsigned char aOnline[24];
    PeerRecord(aOnline, 0x1122334455667788ull, 0x0100007f, 1);
    CHECK(NetWire::RconPeerKey(aOnline, false) == 0x1122334455667788ull);
    CHECK(NetWire::RconLoginKey(aOnline, false) != 0u);
}

TEST_CASE("a heartbeat flood can't push a logged-in admin out" * doctest::test_suite("unit"))
{
    float afLast[3] = { 0.0f, 0.0f, 0.0f };
    bool abIn[3] = { false, false, false };
    CHECK(NetWire::PickRconSlot(afLast, abIn, 3, 100.0f) == 0); // free slots first

    float afBusy[3] = { 95.0f, 90.0f, 99.0f };
    CHECK(NetWire::PickRconSlot(afBusy, abIn, 3, 100.0f) == 1); // the oldest that isn't logged in
    bool abTwoIn[3] = { false, true, true };
    CHECK(NetWire::PickRconSlot(afBusy, abTwoIn, 3, 100.0f) == 0);
    bool abAllIn[3] = { true, true, true };
    CHECK(NetWire::PickRconSlot(afBusy, abAllIn, 3, 100.0f) == -1);

    // a session silent for over a minute, or dated in the future (clock reset), is free again
    float afStale[3] = { 95.0f, 30.0f, 99.0f };
    CHECK(NetWire::PickRconSlot(afStale, abAllIn, 3, 100.0f) == 1);
    float afFuture[3] = { 95.0f, 99.0f, 500.0f };
    CHECK(NetWire::PickRconSlot(afFuture, abAllIn, 3, 100.0f) == 2);
}

TEST_CASE("the lobby rides on the pre-game time's top value" * doctest::test_suite("unit"))
{
    CHECK(NetLobby::IsLobby(true, NetLobby::kLobbyPreGameTime));
    CHECK(!NetLobby::IsLobby(false, NetLobby::kLobbyPreGameTime));
    CHECK(!NetLobby::IsLobby(true, 45));
    CHECK(NetLobby::kLobbyPreGameTime == 1023); // the 10-bit field's top, sent as is
    CHECK(NetLobby::ClampPreGameTime(5000) == NetLobby::kMaxPreGameTime);
    CHECK(NetLobby::ClampPreGameTime(-3) == 0);
    CHECK(NetLobby::ClampPreGameTime(45) == 45);
    CHECK(NetLobby::kMaxPreGameTime < NetLobby::kLobbyPreGameTime);
}

TEST_CASE("rotation edits keep the next map next" * doctest::test_suite("unit"))
{
    using namespace MissionPlayListOps;
    // entries A B C D, next is C (posn 2)
    CHECK(MovedIndex(2, 0, 3) == 1); // A to the end: C moves up
    CHECK(MovedIndex(0, 0, 3) == 3);
    CHECK(MovedIndex(1, 3, 0) == 2); // D to the front: B moves down
    CHECK(MovedIndex(3, 0, 1) == 3); // untouched
    CHECK(PositionAfterMove(2, 4, 0, 3) == 1);
    CHECK(PositionAfterMove(2, 4, 2, 0) == 0); // moving the next entry itself
    CHECK(PositionAfterMove(4, 4, 0, 3) == 4); // a wrap stays a wrap
    CHECK(PositionAfterRemove(2, 0) == 1);
    CHECK(PositionAfterRemove(2, 2) == 2); // removing the next makes its follower next
    CHECK(PositionAfterRemove(2, 3) == 2);
    CHECK(NextIndex(2, 4) == 2);
    CHECK(NextIndex(4, 4) == 0);
    CHECK(NextIndex(0, 0) == -1);

    // a full 256-entry rotation round trip
    int iPosn = 200;
    iPosn = PositionAfterMove(iPosn, 256, 255, 0);
    CHECK(iPosn == 201);
    iPosn = PositionAfterMove(iPosn, 256, 0, 255);
    CHECK(iPosn == 200);
}

TEST_CASE("rcon reply text stays on one line" * doctest::test_suite("unit"))
{
    char acName[16];
    NetAdminText::CleanName("Bad\nName\tHere", acName, sizeof(acName));
    CHECK(std::string(acName) == "Bad?Name?Here");
    NetAdminText::CleanName("A very long player name indeed", acName, sizeof(acName));
    CHECK(strlen(acName) == 15);
    NetAdminText::CleanName(0, acName, sizeof(acName));
    CHECK(acName[0] == 0);

    CHECK(NetAdminText::IsMissionName("geo1c_con"));
    CHECK(NetAdminText::IsMissionName("AB1k_con"));
    CHECK(!NetAdminText::IsMissionName(""));
    CHECK(!NetAdminText::IsMissionName("geo1c_con /endgame"));
    CHECK(!NetAdminText::IsMissionName("..\\geo1c_con"));
    CHECK(!NetAdminText::IsMissionName(std::string(64, 'a').c_str()));
}

TEST_CASE("the server manager is wired into the game" * doctest::test_suite("regression"))
{
    const std::string sAdmin = ReadScript("../src/Battlefront2/Source/NetAdmin.cpp");
    const std::string sNet = ReadScript("../src/Battlefront2/Source/NetGame.cpp");
    const std::string sChar = ReadScript("../src/Battlefront2/Source/Character.cpp");
    const std::string sCmd = ReadScript("../src/Battlefront2/Source/CommandLine.cpp");
    const std::string sMain = ReadScript("../src/Battlefront2/Source/pcMain.cpp");
    const std::string sShell = ReadScript("../src/Battlefront2/Source/ShellLoop.cpp");
    const std::string sText = ReadScript("../src/RedEngineFL/Util/RedLocalizeDB.cpp");

    // rcon sessions: keyed by address, a login per sender IP, nothing left logged in after a line
    CHECK(Has(sAdmin, "const unsigned __int64 uPeer = NetWire::RconPeerKey(pPeer, NetComm::IsLAN());"));
    CHECK(Has(sAdmin, "NetWire::PickRconSlot(afLastRecv, abLoggedIn, 3, fNow)"));
    CHECK(Has(sAdmin, "NetCommP::FindJoinSource((const int*)&sAdminPeers[i]);"));
    CHECK(Has(sAdmin, "sLoginKey = NetWire::RconLoginKey(NetCommP::GetAddr(iDest), NetComm::IsLAN());"));
    CHECK(Has(sAdmin, "adminDests[iAdmin].admin = remoteLoggedIn;\n            remoteLoggedIn = false;"));
    CHECK(Has(sAdmin, "bConsole || (iPlayer >= 0 ? iPlayer < 64 && admins[iPlayer] : remoteLoggedIn)"));
    CHECK(Has(sAdmin, "i < (int)(sizeof(adminCommands) / sizeof(adminCommands[0]))"));
    CHECK(!Has(sAdmin, "i < 94"));

    // the new commands and their reply tags
    const char* apcCommands[] = { "{ AC(93), \"start\" }", "{ AC(94), \"lobby\" }", "{ AC(95), \"state\" }",
                                  "{ AC(96), \"playerinfo\" }", "{ AC(97), \"maplist\" }", "{ AC(98), \"clearmaps\" }",
                                  "{ AC(99), \"appendmap\" }", "{ AC(100), \"movemap\" }", "{ AC(101), \"removemap\" }" };
    for (const char* pcCommand : apcCommands)
    {
        CAPTURE(pcCommand);
        CHECK(Has(sAdmin, pcCommand));
    }
    CHECK(Has(sAdmin, "PutMsg(\"#state\");"));
    CHECK(Has(sAdmin, "\"state=%s hold=%d players=%d/%d/%d time=%d tickets=%d/%d scores=%d/%d map=%s next=%s\""));

    // the lobby: held from the start, never runs out, no human spawns, the jump button can't skip it
    CHECK(Has(sNet, "if (sbLobbyHold && IsDedicated())"));
    CHECK(Has(sNet, "netPreGameTime = NetLobby::kLobbyPreGameTime;"));
    CHECK(Has(sNet, "if (siLobbyStart < 0)\n        {\n            return;\n        }"));
    CHECK(Has(sNet, "netPreGameTimeMax = NetLobby::ClampPreGameTime(iSeconds);"));
    CHECK(Has(sNet, "IsJumpButtonDown() && !NetGame::IsLobby()"));
    CHECK(Has(sNet, "\"OPENSWBF2 NET mission start host=%d dedicated=%d tps=%d game=%d map=%s\""));
    CHECK(Has(sChar, "if (mPlayerId >= 0 && NetGame::IsLobby())\n    {\n        return true;\n    }"));
    CHECK(Has(sCmd, "_stricmp(pcArg + 1, \"lobby\") == 0"));

    // the mission list for the launcher
    CHECK(Has(sCmd, "_stricmp(pcArg + 1, \"listmissions\") == 0"));
    CHECK(Has(sMain, "HasSwitchWord(lpCmdLine, \"listmissions\")"));
    CHECK(Has(sMain, "pcCrash::Exit(gExitCode);"));
    CHECK(Has(sShell, "LuaHelper::GetGlobal(\"missionlist_DumpJSON\");"));
    CHECK(Has(sShell, "MoveFileExA(acTemp, gListMissionsPath, MOVEFILE_REPLACE_EXISTING)"));

    // the waiting text
    CHECK((unsigned int)PblHash("openswbf2.lobby.waiting") == 0xba3e6ee8);
    CHECK(Has(sText, "{ 0xba3e6ee8, L\"Waiting for the host to start the match\" },"));
}
