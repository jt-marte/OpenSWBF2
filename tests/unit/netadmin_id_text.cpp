#include "third_party/doctest/doctest.h"

#include <string.h>
#include <string>

#include "testutil.h"
#include "Battlefront2/Source/NetAdminIdText.h"

namespace
{

void CheckIdText(unsigned __int64 uAddr, const char* pcExpected)
{
    char acBuf[32];
    memset(acBuf, 0x5A, sizeof(acBuf));
    const int iLen = FormatAdminIdText(acBuf, uAddr);
    CAPTURE(pcExpected);
    CHECK(iLen == (int)strlen(pcExpected));
    CHECK(memcmp(acBuf, pcExpected, strlen(pcExpected) + 1) == 0);
}

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

TEST_CASE("admin id text keeps the retail low-dword format" * doctest::test_suite("regression"))
{
    CheckIdText(0ull, "0ll");
    CheckIdText(0x000000017F000001ull, "2130706433ll");
    CheckIdText(0xFFFFFFFF12345678ull, "305419896ll");
    CheckIdText(0xFFFFFFFFFFFFFFFFull, "4294967295ll");
}

TEST_CASE("admin id text sites use the pinned format" * doctest::test_suite("regression"))
{
    std::string sAdmin;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\NetAdmin.cpp", sAdmin));
    CHECK(CountText(sAdmin, "FormatAdminIdText(") == 3);
    CHECK(sAdmin.find("FormatAdminIdText(adminPlayerIdText, *NetCommP::GetAddr(i));") != std::string::npos);
    CHECK(sAdmin.find("FormatAdminIdText(adminBanIdText, *NetCommP::GetAddr(iTarget));") != std::string::npos);
    CHECK(sAdmin.find("FormatAdminIdText(adminBanKeyIdText, *NetCommP::GetAddr(iPlayer));") != std::string::npos);
    CHECK(sAdmin.find("\"%ull\"") == std::string::npos);

    std::string sHeader;
    REQUIRE(ReadRepoText("src\\Battlefront2\\Source\\NetAdminIdText.h", sHeader));
    CHECK(sHeader.find("\"%ull\", (unsigned int)") != std::string::npos);
}
