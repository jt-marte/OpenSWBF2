#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string>

#include "Battlefront2/Source/NetHeroAi.h"

// OpenSWBF2: AI heroes in multiplayer. A host gives the hero to its humans first, as retail did;
// a bot takes it once nobody has for 30 seconds, or when every human declined or the team has
// none. Retail never gave a bot the hero online.

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

bool Has(const std::string& sText, const char* pcNeedle)
{
    return sText.find(pcNeedle) != std::string::npos;
}

} // namespace

TEST_CASE("a bot takes the hero after the wait or once no human can" * doctest::test_suite("unit"))
{
    CHECK(NetHeroAi::kAiHeroWaitSeconds == 30.0f);
    CHECK_FALSE(NetHeroAi::AiHeroDue(false, 0.0f));
    CHECK_FALSE(NetHeroAi::AiHeroDue(false, 29.9f));
    CHECK(NetHeroAi::AiHeroDue(false, 30.0f));
    CHECK(NetHeroAi::AiHeroDue(true, 0.0f)); // every human declined, or there are none
}

TEST_CASE("humans keep first refusal and a bot is only a stand-in" * doctest::test_suite("unit"))
{
    // Not due: the humans, as in retail; a bot only when there is no human at all
    CHECK(NetHeroAi::PickBucket(false, 3, 5) == 1);
    CHECK(NetHeroAi::PickBucket(false, 0, 5) == 0);
    // Due: a bot, if the team has one to spare...
    CHECK(NetHeroAi::PickBucket(true, 3, 5) == 0);
    // ...or the humans again, never nobody while someone could take it
    CHECK(NetHeroAi::PickBucket(true, 3, 0) == 1);
    CHECK(NetHeroAi::PickBucket(true, 0, 0) == -1);
    CHECK(NetHeroAi::PickBucket(false, 0, 0) == -1);
}

TEST_CASE("the host hands an unclaimed hero to a bot" * doctest::test_suite("regression"))
{
    const std::string sNet = ReadSource("../src/Battlefront2/Source/NetGame.cpp");
    const std::string sAdmin = ReadSource("../src/Battlefront2/Source/NetAdmin.cpp");

    // The wait is measured per team in turns, and cleared whenever the hero is taken or the
    // turn passes
    // Every human said no: at once; nobody on the team yet (players still loading): after the wait
    CHECK(Has(sNet, "bAiDue = NetHeroAi::AiHeroDue(bAllDeclined && bAnyHuman, fWaited);"));
    CHECK(Has(sNet, "else if (bAllDeclined && !bAiWaiting)"));
    CHECK(Has(sNet, "const int iBucket = NetHeroAi::PickBucket(bAiDue, aiCount[1], aiCount[0]);"));
    CHECK(Has(sNet, "else if (IsEnabled() && !bAiDue)\n                        {\n                            continue;"));
    CHECK(Has(sNet, "static void PassHeroTurn(int iTeam)\n{\n    snHeroOpenTurn[iTeam] = 0;"));
    CHECK(Has(sNet, "    netHeroSpawnedTurn[iTeam] = 0;\n    snHeroOpenTurn[iTeam] = 0;\n}"));
    CHECK(Has(sNet, "OPENSWBF2 NET hero ai team=%d char=%d declined=%d"));

    // Online, a bot in an empty player slot is no human: it never declines, which kept the hero
    // from its team for good once the team's humans had said no
    CHECK(Has(sNet, "pCharacter->GetTeamNumber() == iTeam && (!IsEnabled() || IsPlaying(i, false))"));

    // The chosen bot comes back as the hero, not as a class its team is short of, and a player
    // taking over its slot gives the hero turn back
    const std::string sTeam = ReadSource("../src/Battlefront2/Source/Team.cpp");
    CHECK(Has(sTeam, "if (netHeroAIID[mTeamNumber] >= 0 && pCharacter == Character::GetPtr(netHeroAIID[mTeamNumber]))"));
    const std::string sChar = ReadSource("../src/Battlefront2/Source/Character.cpp");
    CHECK(Has(sChar, "{\n    NetGame::ReleaseAiHero(this);"));

    // The bot vanishes and respawns as the hero on a multiplayer host too
    CHECK(Has(sNet, "if ((IsEnabled() && !AiHeroesOn()) || netHeroNextPlayerID[team] != -1 || netHeroAIID[team] == -1)"));
    CHECK(Has(sNet, "else if ((!NetGame::IsEnabled() || AiHeroesOn()) && netHeroAIID[team] == player)"));

    // Rcon and ServerSettings.cfg: /aiheroes [0|1] for this run, never saved to display.ini
    CHECK(Has(sAdmin, "{ AC(102), \"aiheroes\" }"));
    CHECK(Has(sAdmin, "NetGame::SetAiHeroes(atoi(acToken) != 0);"));
    CHECK(Has(sAdmin, "\"AI heroes   : %s\""));

    // /hrrespawnvalue [seconds|-1] for ServerSettings.cfg and rcon (retail had it on the command
    // line only); -1 is never, and it sets the rule with the time
    CHECK(Has(sAdmin, "{ AC(103), \"hrrespawnvalue\" }"));
    CHECK(Has(sAdmin, "netHeroRuleRespawn = iSeconds < 0 ? 1 : 2;"));
    CHECK(Has(sAdmin, "PutAdminNumber(netHeroRuleRespawn == 1 ? -1 : netHeroRuleRespawnVal);"));
}
