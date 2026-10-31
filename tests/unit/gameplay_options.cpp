#include "third_party/doctest/doctest.h"

#include <windows.h>
#include <stdio.h>
#include <string>

#include "PebbleFL/Common/PblHashTable.h"
#include "Battlefront2/Source/GameplayCfg.h"

// OpenSWBF2's gameplay options (Options -> General, display.ini [Gameplay]) that stand in for
// the HD Remaster's extras: AI heroes, award weapons and award effects.

namespace
{

std::string TempIni(const char* pcName, const char* pcText)
{
    char acTemp[MAX_PATH];
    GetTempPathA(MAX_PATH, acTemp);
    const std::string sPath = std::string(acTemp) + pcName;
    FILE* pFile = fopen(sPath.c_str(), "wb");
    REQUIRE(pFile);
    fputs(pcText, pFile);
    fclose(pFile);
    return sPath;
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

int Count(const std::string& sText, const char* pcNeedle)
{
    int iCount = 0;
    for (size_t uiAt = sText.find(pcNeedle); uiAt != std::string::npos; uiAt = sText.find(pcNeedle, uiAt + 1))
    {
        ++iCount;
    }
    return iCount;
}

}

TEST_CASE("gameplay options read display.ini's Gameplay section" * doctest::test_suite("unit"))
{
    GameplayCfg::Settings kSettings;
    GameplayCfg::SetDefaults(kSettings);
    CHECK(kSettings.abValue[GameplayCfg::kAiHeroes]);
    CHECK(kSettings.abValue[GameplayCfg::kAwardWeapons]);
    CHECK(kSettings.abValue[GameplayCfg::kAwardEffects]);

    const std::string sPath = TempIni("owb2_gameplay_read.ini",
                                      "[Display]\r\nFps=60\r\n[Gameplay]\r\nAiHeroes=0\r\nAwardWeapons=off\r\n"
                                      "AwardEffects=sometimes\r\n");
    GameplayCfg::LoadIni(kSettings, sPath.c_str());
    CHECK(!kSettings.abValue[GameplayCfg::kAiHeroes]);
    CHECK(!kSettings.abValue[GameplayCfg::kAwardWeapons]);
    CHECK(kSettings.abValue[GameplayCfg::kAwardEffects]); // a bad value keeps the default

    // no ini, or no section, changes nothing
    GameplayCfg::SetDefaults(kSettings);
    GameplayCfg::LoadIni(kSettings, (sPath + ".missing").c_str());
    CHECK(kSettings.abValue[GameplayCfg::kAiHeroes]);
    DeleteFileA(sPath.c_str());
}

TEST_CASE("a gameplay switch wins over display.ini for that run" * doctest::test_suite("unit"))
{
    CHECK(GameplayCfg::Find("aiheroes") == GameplayCfg::kAiHeroes);
    CHECK(GameplayCfg::Find("AwardWeapons") == GameplayCfg::kAwardWeapons);
    CHECK(GameplayCfg::Find("AWARDEFFECTS") == GameplayCfg::kAwardEffects);
    CHECK(GameplayCfg::Find("heroes") == -1);
    CHECK(GameplayCfg::Find(0) == -1);

    GameplayCfg::Settings kSettings;
    GameplayCfg::SetDefaults(kSettings);
    CHECK(GameplayCfg::ParseSwitch(kSettings, "aiheroes", "0"));
    CHECK(!GameplayCfg::ParseSwitch(kSettings, "awardweapons", "2"));
    CHECK(!GameplayCfg::ParseSwitch(kSettings, "awardweapons", "off")); // switches take 0 or 1
    CHECK(!GameplayCfg::ParseSwitch(kSettings, "awardweapons", 0));
    CHECK(!GameplayCfg::ParseSwitch(kSettings, "bots", "1"));
    CHECK(!kSettings.abValue[GameplayCfg::kAiHeroes]);
    CHECK(kSettings.abValue[GameplayCfg::kAwardWeapons]);

    const std::string sPath = TempIni("owb2_gameplay_cli.ini", "[Gameplay]\r\nAiHeroes=1\r\nAwardWeapons=0\r\n");
    GameplayCfg::LoadIni(kSettings, sPath.c_str());
    CHECK(!kSettings.abValue[GameplayCfg::kAiHeroes]);    // the switch stays
    CHECK(!kSettings.abValue[GameplayCfg::kAwardWeapons]); // the ini still counts for the rest
    DeleteFileA(sPath.c_str());
}

TEST_CASE("saving a gameplay option keeps the rest of display.ini" * doctest::test_suite("unit"))
{
    const std::string sPath = TempIni("owb2_gameplay_save.ini", "[Display]\r\nFps=60\r\n");
    REQUIRE(GameplayCfg::SaveIni(GameplayCfg::kAwardEffects, false, sPath.c_str()));
    REQUIRE(GameplayCfg::SaveIni(GameplayCfg::kAiHeroes, true, sPath.c_str()));
    CHECK(!GameplayCfg::SaveIni(-1, true, sPath.c_str()));

    char acValue[16];
    GetPrivateProfileStringA("Display", "Fps", "", acValue, sizeof(acValue), sPath.c_str());
    CHECK(std::string(acValue) == "60");
    GetPrivateProfileStringA("Gameplay", "AwardEffects", "", acValue, sizeof(acValue), sPath.c_str());
    CHECK(std::string(acValue) == "0");

    GameplayCfg::Settings kSettings;
    GameplayCfg::SetDefaults(kSettings);
    GameplayCfg::LoadIni(kSettings, sPath.c_str());
    CHECK(!kSettings.abValue[GameplayCfg::kAwardEffects]);
    CHECK(kSettings.abValue[GameplayCfg::kAiHeroes]);
    DeleteFileA(sPath.c_str());
}

TEST_CASE("award effects off hides only the award buffs' glow" * doctest::test_suite("unit"))
{
    REQUIRE(GameplayCfg::ParseCommandLineSwitch("awardeffects", "0"));
    CHECK(!GameplayCfg::ShowBuffEffect(340.0f)); // War Hero / Guardian, held at 340
    CHECK(GameplayCfg::ShowBuffEffect(25.0f));   // a unit's own buff (Rally, Fury...) still shows
    CHECK(!GameplayCfg::ShowBuffEffect(0.0f));
    REQUIRE(GameplayCfg::ParseCommandLineSwitch("awardeffects", "1"));
    CHECK(GameplayCfg::ShowBuffEffect(340.0f));

    // award weapons only turn off in single player
    REQUIRE(GameplayCfg::ParseCommandLineSwitch("awardweapons", "0"));
    CHECK(!GameplayCfg::AwardWeaponsOn(false));
    CHECK(GameplayCfg::AwardWeaponsOn(true));
    REQUIRE(GameplayCfg::ParseCommandLineSwitch("awardweapons", "1"));
    CHECK(GameplayCfg::AwardWeaponsOn(false));
}

TEST_CASE("gameplay options are wired into the game" * doctest::test_suite("regression"))
{
    const std::string sNet = ReadScript("../src/Battlefront2/Source/NetGame.cpp");
    const std::string sChar = ReadScript("../src/Battlefront2/Source/Character.cpp");
    const std::string sCombat = ReadScript("../src/Battlefront2/Source/CombatUtil.cpp");
    const std::string sSoldier = ReadScript("../src/Battlefront2/Source/EntitySoldier.cpp");
    const std::string sDroideka = ReadScript("../src/Battlefront2/Source/EntityDroideka.cpp");
    const std::string sSpawn = ReadScript("../src/Battlefront2/Platform/Pc/pcSpawnDisplay.cpp");
    const std::string sSound = ReadScript("../src/SoundFL/source/sndsoundstream.cpp");
    const std::string sCmd = ReadScript("../src/Battlefront2/Source/CommandLine.cpp");
    const std::string sPc = ReadScript("../src/Battlefront2/Source/LuaCallbacks_PC.cpp");
    const std::string sText = ReadScript("../src/RedEngineFL/Util/RedLocalizeDB.cpp");
    const std::string sOpts = ReadScript("../data/scripts/common/ifs_opt_general.lua");
    const std::string sSmoke = ReadScript("smoke.ps1");

    // AI heroes: a team whose humans all declined (or that has none) hands the hero to a bot,
    // outside the campaign and Galactic Conquest (multiplayer hosts too: mp_ai_heroes.cpp)
    CHECK(Has(sNet, "if (bAllDeclined && !AiHeroesOn())"));
    CHECK(Has(sNet, "PassHeroTurn(iTeam); // no bot to take it either"));
    CHECK(Has(sNet, "if (Lua_Callbacks::gCurRules == Lua_Callbacks::eGAMERULES_CAMPAIGN ||"));
    CHECK(Has(sNet, "return GameplayCfg::Get(GameplayCfg::kAiHeroes);"));
    // a bot hero that never respawned declined as player -1, writing before netPlayers
    CHECK(Has(sNet, "    if (iPlayer >= 0)\n    {\n        netPlayers[iPlayer].mDeclinedHero = true;"));

    // award weapons: every place that asks whether a weapon is unlocked
    CHECK(Has(sChar, "MedalsMgr* pMedals = GameplayCfg::AwardWeaponsOn(NetGame::IsEnabled()) ? mMedalsMgr : 0;"));
    CHECK(Has(sCombat, "GameplayCfg::AwardWeaponsOn(NetGame::IsEnabled()) ? pMe->mCharacter->mMedalsMgr : 0;"));
    CHECK(Has(sSoldier, "if (mCharacter && GameplayCfg::AwardWeaponsOn(NetGame::IsEnabled()))"));
    CHECK(Has(sSpawn, "if (mOwner && GameplayCfg::AwardWeaponsOn(NetGame::IsEnabled()))"));

    // award effects: offense and defense on soldiers and droidekas
    CHECK(Count(sSoldier, "GameplayCfg::ShowBuffEffect(mBuff") == 2);
    CHECK(Count(sDroideka, "GameplayCfg::ShowBuffEffect(mBuff") == 2);

    // a music list naming a segment its stream lacks (Tatooine) picks again
    CHECK(Has(sSound, "for (int iTry = 0; !pSegment && iTry < 8; ++iTry)"));

    // switches, Lua, the Options page and its strings
    CHECK(Has(sCmd, "if (GameplayCfg::Find(pcArg + 1) >= 0)"));
    CHECK(Has(sPc, "{ \"ScriptCB_GetGameplayOption\", ScriptCB_GetGameplayOption },"));
    CHECK(Has(sPc, "{ \"ScriptCB_SetGameplayOption\", ScriptCB_SetGameplayOption },"));
    CHECK(Has(sOpts, "aiheroes = \"AiHeroes\","));
    CHECK(Has(sOpts, "ScriptCB_SetGameplayOption(ifs_opt_general_gameplay[buttongroup.tag], btnNum == 2)"));
    CHECK(Has(sOpts, "{ tag = \"awardeffects\", title = \"ifs.gameopt.awardeffects\","));
    CHECK((unsigned int)PblHash("ifs.gameopt.aiheroes") == 0xd3766076);
    CHECK((unsigned int)PblHash("ifs.gameopt.awardweapons") == 0x1d8946ea);
    CHECK((unsigned int)PblHash("ifs.gameopt.awardeffects") == 0x7ff56831);
    CHECK(Has(sText, "{ 0xd3766076, L\"AI heroes\" },"));
    CHECK(Has(sText, "{ 0x1d8946ea, L\"Award weapons\" },"));
    CHECK(Has(sText, "{ 0x7ff56831, L\"Award effects\" },"));

    // the harness keeps stock hero rules, so its digests don't move
    CHECK(Has(sSmoke, "-nofocuspause -aiheroes 0 -log"));
}
