#include "third_party/doctest/doctest.h"

#include <string.h>
#include <string>

#include "testutil.h"
#include "PebbleFL/Common/PblUnicode.h"

namespace
{

const unsigned short kCanary = 0xCDCD;

void FillCanary(unsigned short* pusBlock, unsigned int uiCount)
{
    for (unsigned int i = 0; i < uiCount; ++i)
    {
        pusBlock[i] = kCanary;
    }
}

bool CanaryIntact(const unsigned short* pusBlock, unsigned int uiFrom, unsigned int uiTo)
{
    for (unsigned int i = uiFrom; i < uiTo; ++i)
    {
        if (pusBlock[i] != kCanary)
        {
            return false;
        }
    }
    return true;
}

struct CallSitePin
{
    const char* pcFile;
    const char* pcOld;
    const char* pcNew;
};

const CallSitePin kPins[] = {
    { "src\\Battlefront2\\Platform\\Pc\\pcFLJoystick.cpp", "ustrlcat(ausLine, chatCursor, 128)", "ustrlcat(ausLine, chatCursor, 127)" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSaveFile.cpp", "PblUnicode::ustrncpy(mFileName, pusFileName, sizeof(mFileName) / sizeof(mFileName[0]));", "PblUnicode::ustrlcpy(mFileName, pusFileName, sizeof(mFileName) / sizeof(mFileName[0]));" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSaveFile.cpp", "PblUnicode::ustrncnv(pusDate, acDate, 128);", "PblUnicode::ustrncnv(pusDate, acDate, 64);" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSpawnDisplay.cpp", "ustrlcat(text, RedLocalizeDB::Find(0xD3CD201E, false), 256)", "ustrlcat(text, RedLocalizeDB::Find(0xD3CD201E, false), 255)" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSpawnDisplay.cpp", "ustrlcat(text, countText, 256)", "ustrlcat(text, countText, 255)" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSpawnDisplay.cpp", "ustrlcat(text, pWeapon->mLabel, 256)", "ustrlcat(text, pWeapon->mLabel, 255)" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSpawnDisplay.cpp", "ustrncnv(name, pWeapon->mFilename, 128)", "ustrncnv(name, pWeapon->mFilename, 127)" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSpawnDisplay.cpp", "ustrlcat(text, name, 256)", "ustrlcat(text, name, 255)" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSpawnDisplay.cpp", "ustrlcat(text, sNewlineStr16, 256)", "ustrlcat(text, sNewlineStr16, 255)" },
    { "src\\Battlefront2\\Source\\BootDisplay.cpp", "NetGame::GetPlayerName(iBootPlayer), 64);", "NetGame::GetPlayerName(iBootPlayer), 63);" },
    { "src\\Battlefront2\\Source\\BootDisplay.cpp", "PblUnicode::usnprintf(text, 256,", "PblUnicode::usnprintf(text, 255," },
    { "src\\Battlefront2\\Source\\Character.cpp", "PblUnicode::ustrncpy(mRegularName, pName, 64);", "PblUnicode::ustrlcpy(mRegularName, pName, 64);" },
    { "src\\Battlefront2\\Source\\Character.cpp", "PblUnicode::ustrncpy(mName, pName, 64);", "PblUnicode::ustrlcpy(mName, pName, 64);" },
    { "src\\Battlefront2\\Source\\Character.cpp", "PblUnicode::ustrncpy(mRegularName, name, 64);", "PblUnicode::ustrlcpy(mRegularName, name, 64);" },
    { "src\\Battlefront2\\Source\\Character.cpp", "ustrncnv(name, NetGame::GetPlayerName(iPlayerId), 64)", "ustrncnv(name, NetGame::GetPlayerName(iPlayerId), 63)" },
    { "src\\Battlefront2\\Source\\FLInputManager.cpp", "ustrlcat(awLine, pClass->mLabel, 256)", "ustrlcat(awLine, pClass->mLabel, 255)" },
    { "src\\Battlefront2\\Source\\GameLoop.cpp", "ustrncnv(acTeam1, pTeam1, sizeof(acTeam1))", "ustrncnv(acTeam1, pTeam1, sizeof(acTeam1) - 1)" },
    { "src\\Battlefront2\\Source\\GameLoop.cpp", "ustrncnv(acTeam2, pTeam2, sizeof(acTeam2))", "ustrncnv(acTeam2, pTeam2, sizeof(acTeam2) - 1)" },
    { "src\\Battlefront2\\Source\\GameLoop.cpp", "ustrlcat(acName, acCount, 128)", "ustrlcat(acName, acCount, 127)" },
    { "src\\Battlefront2\\Source\\HeroMessageDisplay.cpp", "NetGame::GetPlayerName(netHeroPlayerID[iTeam]), 64);", "NetGame::GetPlayerName(netHeroPlayerID[iTeam]), 63);" },
    { "src\\Battlefront2\\Source\\HeroMessageDisplay.cpp", "PblUnicode::ustrncpy(playerName, pHero->GetRegularName(), 64);", "PblUnicode::ustrlcpy(playerName, pHero->GetRegularName(), 64);" },
    { "src\\Battlefront2\\Source\\HeroMessageDisplay.cpp", "PblUnicode::usnprintf(message, 256,", "PblUnicode::usnprintf(message, 255," },
    { "src\\Battlefront2\\Source\\HUDElementMap.cpp", "usnprintf(buffer, sizeof(buffer) - 1, \"%d\"", "usnprintf(buffer, sizeof(buffer) / sizeof(buffer[0]) - 2, \"%d\"" },
    { "src\\Battlefront2\\Source\\HUDGameEvents.cpp", "ustrncnv(acFilename, pWeaponClass->mFilename, 256)", "ustrncnv(acFilename, pWeaponClass->mFilename, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks.cpp", "(const unsigned short*)luaL_checklstring(LuaHelper::mState, i, 0), 256);", "(const unsigned short*)luaL_checklstring(LuaHelper::mState, i, 0), 255);" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "GetLocalizedString(usAction, sizeof(usAction),", "GetLocalizedString(usAction, sizeof(usAction) / sizeof(usAction[0])," },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "GetLocalizedString(usButton, sizeof(usButton),", "GetLocalizedString(usButton, sizeof(usButton) / sizeof(usButton[0])," },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "GetLocalizedString(usKey, sizeof(usKey),", "GetLocalizedString(usKey, sizeof(usKey) / sizeof(usKey[0])," },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrlcat(usAction, usColon, 256)", "ustrlcat(usAction, usColon, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrlcpy(usKey, usHash, 512)", "ustrlcpy(usKey, usHash, 256)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrlcat(usKey, usNum, 512)", "ustrlcat(usKey, usNum, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrlcat(usKey, usButton, 512)", "ustrlcat(usKey, usButton, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrlcpy(usKey, usButton, 512)", "ustrlcpy(usKey, usButton, 256)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrlcat(usKeys, usComma, 256)", "ustrlcat(usKeys, usComma, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrlcat(usKeys, usKey, 256)", "ustrlcat(usKeys, usKey, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrncnv(usBuffer, acBuffer, 512)", "ustrncnv(usBuffer, acBuffer, 256)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", "ustrncnv(usBuffer, stats.username, 256)", "ustrncnv(usBuffer, stats.username, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "usnprintf(ausName, 64, RedLocalizeDB::Find(0x3802F15B, false), 1)", "usnprintf(ausName, 63, RedLocalizeDB::Find(0x3802F15B, false), 1)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "ustrncnv(acName, (unsigned short const*)pcName, sizeof(acName))", "ustrncnv(acName, (unsigned short const*)pcName, sizeof(acName) - 1)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "ustrncnv(acName, pusName, sizeof(acName))", "ustrncnv(acName, pusName, sizeof(acName) - 1)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "ustrlcpy(sMetagameFilename, pusName, sizeof(sMetagameFilename))", "ustrlcpy(sMetagameFilename, pusName, sizeof(sMetagameFilename) / sizeof(sMetagameFilename[0]))" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "ustrlcpy(sCampaignFilename, pusName, sizeof(sCampaignFilename))", "ustrlcpy(sCampaignFilename, pusName, sizeof(sCampaignFilename) / sizeof(sCampaignFilename[0]))" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "PblUnicode::ustrncpy(Lua_Callbacks::sTempReturnString16, pusBase, 256);", "PblUnicode::ustrlcpy(Lua_Callbacks::sTempReturnString16, pusBase, 256);" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "usnprintf(Lua_Callbacks::sTempReturnString16, 512, pusBase, uiTry)", "usnprintf(Lua_Callbacks::sTempReturnString16, 255, pusBase, uiTry)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "PblUnicode::ustrncpy(pusDest, pusSource, 128);", "PblUnicode::ustrlcpy(pusDest, pusSource, 128);" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_LoadSave.cpp", "ustrlcat(pusDest, ausStamp, 128)", "ustrlcat(pusDest, ausStamp, 127)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Net.cpp", "ustrncnv(acName, pusName, 32)", "ustrncnv(acName, pusName, 31)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Net.cpp", "ustrncnv(name, gSkipToNTGUIProfile, 64)", "ustrncnv(name, gSkipToNTGUIProfile, 63)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Net.cpp", "ustrlcat(ausText, ausArg, 256)", "ustrlcat(ausText, ausArg, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Net.cpp", "ustrlcat(ausText, RedLocalizeDB::Find(PblHash(pcModeName), false), 256)", "ustrlcat(ausText, RedLocalizeDB::Find(PblHash(pcModeName), false), 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Net.cpp", "PblUnicode::usnprintf(ausText, 256,", "PblUnicode::usnprintf(ausText, 255," },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Net.cpp", "ustrncnv(ausArg, pcVersion, 256)", "ustrncnv(ausArg, pcVersion, 255)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Stats.cpp", "ustrlcat(usText, usTime, 64)", "ustrlcat(usText, usTime, 63)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Stats.cpp", "ustrlcat(usText, usBuf, 64)", "ustrlcat(usText, usBuf, 63)" },
    { "src\\Battlefront2\\Source\\LuaCallbacks_Stats.cpp", "ustrlcat(usTitle, usBuf, 100)", "ustrlcat(usTitle, usBuf, 99)" },
    { "src\\Battlefront2\\Source\\MedalsMgr.cpp", "ustrncnv(wideKey, key, 64)", "ustrncnv(wideKey, key, 63)" },
    { "src\\Battlefront2\\Source\\NetAdmin.cpp", "ustrncnv(acBuffer + uLength, pusMessage, 128 - uLength)", "ustrncnv(acBuffer + uLength, pusMessage, 127 - uLength)" },
    { "src\\Battlefront2\\Source\\NetAdmin.cpp", "ustrncnv(acTeam1, pusTeam1, 32)", "ustrncnv(acTeam1, pusTeam1, 31)" },
    { "src\\Battlefront2\\Source\\NetAdmin.cpp", "ustrncnv(acTeam2, pusTeam2, 32)", "ustrncnv(acTeam2, pusTeam2, 31)" },
    { "src\\Battlefront2\\Source\\NetAdmin.cpp", "ustrncnv(ausMsg, pcMsg, 128)", "ustrncnv(ausMsg, pcMsg, 127)" },
    { "src\\Battlefront2\\Source\\NetAdmin.cpp", "ustrncnv(acTeam, pTeam->mName, 128)", "ustrncnv(acTeam, pTeam->mName, 127)" },
    { "src\\Battlefront2\\Source\\NetAdmin.cpp", "ustrncnv(acLine, pusLine, 512)", "ustrncnv(acLine, pusLine, 511)" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "NetGame::GetPlayerName(iPlayer), 64);", "NetGame::GetPlayerName(iPlayer), 63);" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "ustrncnv(playerName, pszName, 64)", "ustrncnv(playerName, pszName, 63)" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "PblUnicode::ustrncpy(message->text, text, 128);", "PblUnicode::ustrlcpy(message->text, text, 128);" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "ustrlcat(text, admin, 128)", "ustrlcat(text, admin, 127)" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "ustrlcat(text, sender->GetName(), 128)", "ustrlcat(text, sender->GetName(), 127)" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "ustrlcat(text, separator, 128)", "ustrlcat(text, separator, 127)" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "ustrlcat(text, message.text, 128)", "ustrlcat(text, message.text, 127)" },
    { "src\\Battlefront2\\Source\\NetGame.cpp", "ustrlcat(text, RedLocalizeDB::Find(requestKeys[message.code], false), 128)", "ustrlcat(text, RedLocalizeDB::Find(requestKeys[message.code], false), 127)" },
    { "src\\Battlefront2\\Source\\PlayerProfile.cpp", "PblUnicode::ustrncpy(mName, pusName, 16);", "PblUnicode::ustrlcpy(mName, pusName, 16);" },
    { "src\\Battlefront2\\Source\\PlayerProfile.cpp", "PblUnicode::ustrncpy(mName, RedLocalizeDB::Find(0x62a0afc0, false), 16);", "PblUnicode::ustrlcpy(mName, RedLocalizeDB::Find(0x62a0afc0, false), 16);" },
    { "src\\Battlefront2\\Source\\PopupText.cpp", "usnprintf(page, 16, \"%d/%d\"", "usnprintf(page, 14, \"%d/%d\"" },
    { "src\\Battlefront2\\Source\\SaveFsm.cpp", "PblUnicode::uvsnprintf((unsigned short*)pDest, unsigned int(-1), pusFormat,", "PblUnicode::uvsnprintf((unsigned short*)pDest, uiSize - 1, pusFormat," },
    { "src\\Battlefront2\\Source\\SaveFsm.cpp", "PblUnicode::ustrncnv((unsigned short*)pDest, pcFormat, uiSize);", "PblUnicode::ustrncnv((unsigned short*)pDest, pcFormat, uiSize - 1);" },
    { "src\\Battlefront2\\Source\\SaveFsm.cpp", "wcscpy((wchar_t*)pusDest, (const wchar_t*)pusText);", "PblUnicode::ustrlcpy(pusDest, pusText, uiSize);" },
    { "src\\Battlefront2\\Source\\SaveFsm.cpp", "PblUnicode::ustrncnv(pusDest, pcSrc, uiSize);", "PblUnicode::ustrncnv(pusDest, pcSrc, uiSize - 1);" },
    { "src\\Battlefront2\\Source\\SaveGameManager.cpp", "usnprintf(ausName, 256, pusFilename, iStart)", "usnprintf(ausName, 255, pusFilename, iStart)" },
    { "src\\Battlefront2\\Source\\SpawnManager.cpp", "NetGame::GetPlayerName(iPlayer), 32);", "NetGame::GetPlayerName(iPlayer), 31);" },
    { "src\\Battlefront2\\Source\\Team.cpp", "PblUnicode::ustrncpy(pName, pusName, 64);", "PblUnicode::ustrlcpy(pName, pusName, 64);" },
    { "src\\Battlefront2\\Source\\Team.cpp", "ustrlcat(pName, mFirstNames[uRand % mFirstCount], 64)", "ustrlcat(pName, mFirstNames[uRand % mFirstCount], 60)" },
    { "src\\Battlefront2\\Source\\Team.cpp", "ustrlcat(pName, mLastNames[uRand % mLastCount], 64)", "ustrlcat(pName, mLastNames[uRand % mLastCount], 63)" },
    { "src\\RedEngineFL\\Interface\\RedTextElement.cpp", "uvsnprintf(buffer, 0x3ff, pcFormat, pArgs)", "uvsnprintf(buffer, 0x3fe, pcFormat, pArgs)" },
    { "src\\RedEngineFL\\Interface\\RedTextElement.cpp", "ustrncnv(acMagicA, acMagic, 256)", "ustrncnv(acMagicA, acMagic, 255)" },
    { "src\\RedEngineFL\\Util\\RedLocalizeDB.cpp", "memset(pusBuffer, 0, uiSize);", "memset(pusBuffer, 0, uiSize * sizeof(pusBuffer[0]));" },
    { "src\\Battlefront2\\Source\\LuaCallbacks.cpp", "PblUnicode::ustrncpy(sTempReturnString16,\n                         (const unsigned short*)luaL_checklstring", "PblUnicode::ustrlcpy(sTempReturnString16,\n                         (const unsigned short*)luaL_checklstring(LuaHelper::mState, 1, 0), 256);" },
    { "src\\Battlefront2\\Source\\LuaCallbacks.cpp", "(const unsigned short*)luaL_checklstring(LuaHelper::mState, i, 0), 256);", "(const unsigned short*)luaL_checklstring(LuaHelper::mState, i, 0), 255);" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSaveFile.cpp", "PblUnicode::ustrncpy(pusName, pusFilename, uiLen - 7);", "    if (uiLen < 8)\n    {\n        if (pusName)\n        {\n            pusName[0] = 0;\n        }\n        if (pusDate)\n        {\n            pusDate[0] = 0;\n        }\n        return;\n    }\n" },
    { "src\\Battlefront2\\Platform\\Pc\\pcSaveFile.cpp", "pusName[uiLen - 8] = 0;", "const unsigned int uiNameLen = uiLen - 8 < 127 ? uiLen - 8 : 127;\n        PblUnicode::ustrlcpy(pusName, pusFilename, uiNameLen + 1);" },
};

const char* const kCallNames[] = { "ustrlcpy(", "ustrlcat(", "ustrncpy(", "ustrncnv(", "GetLocalizedString(" };

bool HasBareSizeofCount(const std::string& sLine)
{
    bool bCall = false;
    for (size_t i = 0; i < sizeof(kCallNames) / sizeof(kCallNames[0]); ++i)
    {
        if (sLine.find(kCallNames[i]) != std::string::npos)
        {
            bCall = true;
        }
    }
    if (!bCall)
    {
        return false;
    }
    for (size_t uiAt = sLine.find("sizeof("); uiAt != std::string::npos; uiAt = sLine.find("sizeof(", uiAt + 1))
    {
        if (uiAt >= 2 && sLine.compare(uiAt - 2, 2, "/ ") == 0)
        {
            continue;
        }
        const size_t uiClose = sLine.find(')', uiAt);
        if (uiClose == std::string::npos || uiClose + 1 >= sLine.size())
        {
            continue;
        }
        const char cNext = sLine[uiClose + 1];
        if (cNext == ')' || cNext == ',')
        {
            return true;
        }
    }
    return false;
}

}

TEST_CASE("ustrlcpy and ustrlcat stay inside an element-count bound" * doctest::test_suite("regression"))
{
    unsigned short ausSource[400];
    for (unsigned int i = 0; i < 399; ++i)
    {
        ausSource[i] = (unsigned short)('a' + i % 26);
    }
    ausSource[399] = 0;

    unsigned short ausBlock[256 + 8];

    FillCanary(ausBlock, 256 + 8);
    PblUnicode::ustrlcpy(ausBlock, ausSource, 256);
    CHECK(ausBlock[255] == 0);
    CHECK(PblUnicode::ustrlen(ausBlock) == 255);
    CHECK(CanaryIntact(ausBlock, 256, 256 + 8));

    FillCanary(ausBlock, 256 + 8);
    ausBlock[0] = 'x';
    ausBlock[1] = 0;
    PblUnicode::ustrlcat(ausBlock, ausSource, 255);
    CHECK(ausBlock[255] == 0);
    CHECK(PblUnicode::ustrlen(ausBlock) == 254);
    CHECK(CanaryIntact(ausBlock, 256, 256 + 8));

    FillCanary(ausBlock, 256 + 8);
    PblUnicode::ustrncpy(ausBlock, ausSource, 255);
    CHECK(ausBlock[255] == 0);
    CHECK(PblUnicode::ustrlen(ausBlock) == 254);
    CHECK(CanaryIntact(ausBlock, 256, 256 + 8));

    char acSource[400];
    memset(acSource, 'b', sizeof(acSource) - 1);
    acSource[399] = 0;
    FillCanary(ausBlock, 256 + 8);
    PblUnicode::ustrncnv(ausBlock, acSource, 255);
    CHECK(ausBlock[255] == 0);
    CHECK(PblUnicode::ustrlen(ausBlock) == 254);
    CHECK(CanaryIntact(ausBlock, 256, 256 + 8));

    FillCanary(ausBlock, 256 + 8);
    PblUnicode::usnprintf(ausBlock, 255, ausSource);
    CHECK(CanaryIntact(ausBlock, 256, 256 + 8));

    FillCanary(ausBlock, 256 + 8);
    PblUnicode::usnprintf(ausBlock, 254, acSource);
    CHECK(CanaryIntact(ausBlock, 256, 256 + 8));
}

TEST_CASE("wide string call sites pass element counts" * doctest::test_suite("regression"))
{
    for (size_t i = 0; i < sizeof(kPins) / sizeof(kPins[0]); ++i)
    {
        std::string sText;
        CAPTURE(kPins[i].pcFile);
        CAPTURE(kPins[i].pcNew);
        REQUIRE(ReadRepoText(kPins[i].pcFile, sText));
        CHECK(sText.find(kPins[i].pcNew) != std::string::npos);
        CHECK(sText.find(kPins[i].pcOld) == std::string::npos);
    }

    std::string sIFace;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\LuaCallbacks_IFace.cpp", sIFace));
    CHECK(sIFace.find("usKey, usHash, 512)") == std::string::npos);
    CHECK(sIFace.find("usKey, usNum, 512)") == std::string::npos);
    CHECK(sIFace.find("usKey, usButton, 512)") == std::string::npos);
    CHECK(sIFace.find("GetLocalizedString(usButton, sizeof(usButton),") == std::string::npos);
    CHECK(sIFace.find("GetLocalizedString(usAction, sizeof(usAction),") == std::string::npos);

    std::string sLocalize;
    REQUIRE(ReadRepoText("src\\RedEngineFL\\Util\\RedLocalizeDB.cpp", sLocalize));
    CHECK(sLocalize.find("memset(pusBuffer, 0, uiSize);") == std::string::npos);
    CHECK(sLocalize.find("PblUnicode::ustrncpy(pusBuffer, pusString, uiSize - 1);") != std::string::npos);

    for (size_t i = 0; i < sizeof(kPins) / sizeof(kPins[0]); ++i)
    {
        if (i > 0 && strcmp(kPins[i].pcFile, kPins[i - 1].pcFile) == 0)
        {
            continue;
        }
        std::string sText;
        REQUIRE(ReadRepoText(kPins[i].pcFile, sText));
        size_t uiStart = 0;
        while (uiStart < sText.size())
        {
            size_t uiEnd = sText.find('\n', uiStart);
            if (uiEnd == std::string::npos)
            {
                uiEnd = sText.size();
            }
            const std::string sLine = sText.substr(uiStart, uiEnd - uiStart);
            if (HasBareSizeofCount(sLine))
            {
                FAIL_CHECK(std::string(kPins[i].pcFile) + ": " + sLine);
            }
            uiStart = uiEnd + 1;
        }
    }
}
