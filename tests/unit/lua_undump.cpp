#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "testutil.h"
#include "LuaFL/include/lua.h"
#include "LuaFL/include/lauxlib.h"

namespace
{

// Offset of the size_t width byte in a chunk header: "\033Lua", version, endianness, int.
const size_t kSizeTByte = 7;
// Signature, nine format bytes and the float test number.
const size_t kHeaderLen = 4 + 10 + sizeof(lua_Number);

// Exercises nested protos, upvalues, string and number constants, a loop and
// table constructors. Not stripped, so the debug strings go through LoadSize too.
const char kScript[] =
    "local iBase = 40\n"
    "local function Add(a, b) return a + b + iBase end\n"
    "local tNames = { \"alpha\", \"beta\", \"gamma\" }\n"
    "gUndumpSum = Add(1, 1)\n"
    "gUndumpName = tNames[2] .. \"_\" .. \"x64\"\n"
    "gUndumpCount = 0\n"
    "for i = 1, 10 do gUndumpCount = gUndumpCount + i end\n"
    "return \"done\", 3.5\n";

// BODY of scr_ "ifs_opt_mp_listtags" in stock data\_lvl_pc\common.lvl (Steam depot 6061,
// sha1 bd52f8b6cee60b9b0507620185f33b260089379c, scr_ at offset 0x347c8). Retail
// luac output: 4-byte size_t, float numbers, stripped. It builds one global table.
const unsigned char kRetailListTags[] =
{
    0x1B, 0x4C, 0x75, 0x61, 0x50, 0x01, 0x04, 0x04, 0x04, 0x06, 0x08, 0x09,
    0x09, 0x04, 0x3B, 0xAF, 0xEF, 0x4B, 0x08, 0x00, 0x00, 0x00, 0x3D, 0x28,
    0x6E, 0x6F, 0x6E, 0x65, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x04, 0x14, 0x00, 0x00, 0x00, 0x69,
    0x66, 0x73, 0x5F, 0x6F, 0x70, 0x74, 0x5F, 0x6D, 0x70, 0x5F, 0x6C, 0x69,
    0x73, 0x74, 0x74, 0x61, 0x67, 0x73, 0x00, 0x04, 0x07, 0x00, 0x00, 0x00,
    0x70, 0x72, 0x6F, 0x6D, 0x70, 0x74, 0x00, 0x04, 0x08, 0x00, 0x00, 0x00,
    0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x73, 0x00, 0x04, 0x05, 0x00, 0x00,
    0x00, 0x69, 0x63, 0x6F, 0x6E, 0x00, 0x04, 0x0C, 0x00, 0x00, 0x00, 0x76,
    0x6F, 0x69, 0x63, 0x65, 0x72, 0x65, 0x63, 0x6F, 0x72, 0x64, 0x00, 0x04,
    0x0E, 0x00, 0x00, 0x00, 0x76, 0x6F, 0x69, 0x63, 0x65, 0x70, 0x6C, 0x61,
    0x79, 0x62, 0x61, 0x63, 0x6B, 0x00, 0x04, 0x0C, 0x00, 0x00, 0x00, 0x76,
    0x6F, 0x69, 0x63, 0x65, 0x65, 0x6E, 0x61, 0x62, 0x6C, 0x65, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x03, 0x00, 0x41,
    0x00, 0x00, 0x01, 0x81, 0x00, 0x00, 0x02, 0xC1, 0x00, 0x00, 0x03, 0x01,
    0x01, 0x00, 0x04, 0x41, 0x01, 0x00, 0x05, 0x81, 0x01, 0x00, 0x06, 0x5F,
    0x01, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x1B, 0x80, 0x00, 0x00, 0x00,
};

const char* const kRetailTags[] = { "prompt", "players", "icon", "voicerecord", "voiceplayback", "voiceenable" };

int AppendChunk(lua_State*, const void* pData, size_t uSize, void* pUser)
{
    static_cast<std::string*>(pUser)->append(static_cast<const char*>(pData), uSize);
    return 0;
}

// Dumps the function on top of L with the host's lua_dump.
std::string DumpTop(lua_State* L)
{
    std::string sChunk;
    CHECK(lua_dump(L, AppendChunk, &sChunk) == 1);
    return sChunk;
}

int Load(lua_State* L, const std::string& sChunk)
{
    return luaL_loadbuffer(L, sChunk.data(), sChunk.size(), "=undump");
}

std::string Compile(const char* pcSource)
{
    lua_State* L = lua_open();
    REQUIRE(L);
    REQUIRE(luaL_loadbuffer(L, pcSource, strlen(pcSource), "=undump") == 0);
    std::string sChunk = DumpTop(L);
    lua_close(L);
    return sChunk;
}

std::string ErrorText(lua_State* L)
{
    const char* pcError = lua_tostring(L, -1);
    return pcError ? pcError : "";
}

// Copies a little-endian chunk field by field, writing every size_t at a new width.
struct Rewriter
{
    const std::string* pIn;
    size_t uPos;
    int iFromWidth;
    int iToWidth;
    bool bOk;
    std::string sOut;
};

const char* Take(Rewriter& r, size_t uBytes)
{
    if (!r.bOk || r.pIn->size() - r.uPos < uBytes)
    {
        r.bOk = false;
        return 0;
    }
    const char* pcData = r.pIn->data() + r.uPos;
    r.uPos += uBytes;
    return pcData;
}

void CopyBytes(Rewriter& r, size_t uBytes)
{
    const char* pcData = Take(r, uBytes);
    if (pcData)
    {
        r.sOut.append(pcData, uBytes);
    }
}

int CopyInt(Rewriter& r)
{
    int iValue = 0;
    const char* pcData = Take(r, sizeof(iValue));
    if (pcData)
    {
        memcpy(&iValue, pcData, sizeof(iValue));
        r.sOut.append(pcData, sizeof(iValue));
    }
    if (iValue < 0)
    {
        r.bOk = false;
    }
    return r.bOk ? iValue : 0;
}

void CopyString(Rewriter& r)
{
    unsigned long long uSize = 0;
    const char* pcData = Take(r, r.iFromWidth);
    if (!pcData)
    {
        return;
    }
    memcpy(&uSize, pcData, r.iFromWidth);
    r.sOut.append(reinterpret_cast<const char*>(&uSize), r.iToWidth);
    CopyBytes(r, static_cast<size_t>(uSize));
}

void CopyFunction(Rewriter& r)
{
    CopyString(r);
    CopyInt(r);
    CopyBytes(r, 4);
    CopyBytes(r, CopyInt(r) * sizeof(int));
    int iCount = CopyInt(r);
    for (int i = 0; i < iCount && r.bOk; ++i)
    {
        CopyString(r);
        CopyInt(r);
        CopyInt(r);
    }
    iCount = CopyInt(r);
    for (int i = 0; i < iCount && r.bOk; ++i)
    {
        CopyString(r);
    }
    iCount = CopyInt(r);
    for (int i = 0; i < iCount && r.bOk; ++i)
    {
        const char* pcType = Take(r, 1);
        if (!pcType)
        {
            break;
        }
        r.sOut += *pcType;
        if (*pcType == LUA_TNUMBER)
        {
            CopyBytes(r, sizeof(lua_Number));
        }
        else if (*pcType == LUA_TSTRING)
        {
            CopyString(r);
        }
        else if (*pcType != LUA_TNIL)
        {
            r.bOk = false;
        }
    }
    iCount = CopyInt(r);
    for (int i = 0; i < iCount && r.bOk; ++i)
    {
        CopyFunction(r);
    }
    CopyBytes(r, CopyInt(r) * sizeof(int));
}

std::string RewriteSizeWidth(const std::string& sIn, int iToWidth)
{
    Rewriter r;
    r.pIn = &sIn;
    r.uPos = 0;
    r.iFromWidth = sIn.size() > kSizeTByte ? (unsigned char)sIn[kSizeTByte] : 0;
    r.iToWidth = iToWidth;
    r.bOk = sIn.size() >= kHeaderLen && sIn.compare(0, 4, "\033Lua") == 0 && sIn[5] == 1 &&
            (r.iFromWidth == 4 || r.iFromWidth == 8);
    CopyBytes(r, kSizeTByte);
    r.sOut += (char)iToWidth;
    Take(r, 1);
    CopyBytes(r, kHeaderLen - kSizeTByte - 1);
    CopyFunction(r);
    CHECK(r.bOk);
    // Retail BODY chunks end with one NUL past the chunk; keep any trailer as is.
    r.sOut.append(sIn, r.uPos, std::string::npos);
    return r.sOut;
}

double GlobalNumber(lua_State* L, const char* pcName)
{
    lua_getglobal(L, pcName);
    double dValue = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : -1.0;
    lua_pop(L, 1);
    return dValue;
}

std::string GlobalString(lua_State* L, const char* pcName)
{
    lua_getglobal(L, pcName);
    const char* pcValue = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    std::string sValue = pcValue;
    lua_pop(L, 1);
    return sValue;
}

// Loads and runs a chunk of kScript, then checks its globals and return values.
void RunScriptChunk(const std::string& sChunk)
{
    lua_State* L = lua_open();
    REQUIRE(L);
    int iLoad = Load(L, sChunk);
    std::string sError = iLoad ? ErrorText(L) : std::string();
    INFO(sError);
    REQUIRE(iLoad == 0);
    REQUIRE(lua_pcall(L, 0, LUA_MULTRET, 0) == 0);
    REQUIRE(lua_gettop(L) == 2);
    CHECK(std::string(lua_tostring(L, 1)) == "done");
    CHECK(lua_tonumber(L, 2) == 3.5f);
    CHECK(GlobalNumber(L, "gUndumpSum") == 42.0);
    CHECK(GlobalString(L, "gUndumpName") == "beta_x64");
    CHECK(GlobalNumber(L, "gUndumpCount") == 55.0);
    lua_close(L);
}

void CheckRetailTags(lua_State* L)
{
    lua_getglobal(L, "ifs_opt_mp_listtags");
    REQUIRE(lua_istable(L, -1));
    for (int i = 0; i < 6; ++i)
    {
        lua_rawgeti(L, -1, i + 1);
        const char* pcTag = lua_tostring(L, -1);
        CHECK(std::string(pcTag ? pcTag : "") == kRetailTags[i]);
        lua_pop(L, 1);
    }
    lua_rawgeti(L, -1, 7);
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 2);
}

// Expects lua_load to fail without crashing; returns the error message.
std::string LoadError(const std::string& sChunk)
{
    lua_State* L = lua_open();
    REQUIRE(L);
    CHECK(Load(L, sChunk) != 0);
    std::string sError = ErrorText(L);
    lua_close(L);
    return sError;
}

}

TEST_CASE("lua undump runs a native dump" * doctest::test_suite("unit"))
{
    std::string sChunk = Compile(kScript);
    REQUIRE(sChunk.size() > kHeaderLen);
    // lua_dump writes the 32-bit tools' 4-byte size_t on every build
    CHECK((unsigned char)sChunk[kSizeTByte] == 4);
    RunScriptChunk(sChunk);
}

TEST_CASE("lua undump runs a dump rewritten with 8-byte sizes" * doctest::test_suite("unit"))
{
    std::string sNative = Compile(kScript);
    std::string sWide = RewriteSizeWidth(sNative, 8);
    REQUIRE(sWide.size() > kHeaderLen);
    CHECK(sWide[kSizeTByte] == 8);
    CHECK(sWide.size() > sNative.size());
    RunScriptChunk(sWide);
}

TEST_CASE("lua undump runs a dump rewritten with 4-byte sizes" * doctest::test_suite("unit"))
{
    std::string sNative = Compile(kScript);
    std::string sNarrow = RewriteSizeWidth(sNative, 4);
    REQUIRE(sNarrow.size() > kHeaderLen);
    CHECK(sNarrow[kSizeTByte] == 4);
    CHECK(sNarrow == sNative);
    RunScriptChunk(sNarrow);
}

TEST_CASE("lua undump gives the same protos at either width" * doctest::test_suite("unit"))
{
    std::string sNative = Compile(kScript);
    lua_State* L = lua_open();
    REQUIRE(L);
    REQUIRE(Load(L, RewriteSizeWidth(sNative, 4)) == 0);
    std::string sFromNarrow = DumpTop(L);
    REQUIRE(Load(L, RewriteSizeWidth(sNative, 8)) == 0);
    std::string sFromWide = DumpTop(L);
    lua_close(L);
    CHECK(sFromNarrow == sNative);
    CHECK(sFromWide == sNative);
}

TEST_CASE("lua undump rejects bad size_t widths" * doctest::test_suite("unit"))
{
    std::string sChunk = Compile(kScript);
    const int aiWidths[] = { 0, 2, 16 };
    for (int i = 0; i < 3; ++i)
    {
        std::string sBad = sChunk;
        sBad[kSizeTByte] = (char)aiWidths[i];
        std::string sError = LoadError(sBad);
        INFO("width " << aiWidths[i] << ": " << sError);
        CHECK(sError.find("size of size_t is 4 or 8") != std::string::npos);
    }
}

TEST_CASE("lua undump rejects an 8-byte size beyond 32 bits" * doctest::test_suite("unit"))
{
    std::string sWide = RewriteSizeWidth(Compile(kScript), 8);
    REQUIRE(sWide.size() > kHeaderLen + 8);
    // The main function's source string length comes first after the header.
    const unsigned long long uHuge = 0x100000009ull;
    memcpy(&sWide[kHeaderLen], &uHuge, sizeof(uHuge));
    std::string sError = LoadError(sWide);
    INFO(sError);
    if (sizeof(size_t) == 4)
    {
        CHECK(sError.find("size too large in undump") != std::string::npos);
    }
    else
    {
        CHECK(sError.find("block too big") != std::string::npos);
    }
}

TEST_CASE("lua undump rejects a truncated chunk at either width" * doctest::test_suite("unit"))
{
    std::string sNative = Compile(kScript);
    const int aiWidths[] = { 4, 8 };
    for (int i = 0; i < 2; ++i)
    {
        std::string sChunk = RewriteSizeWidth(sNative, aiWidths[i]);
        std::string sError = LoadError(sChunk.substr(0, sChunk.size() / 2));
        INFO("width " << aiWidths[i] << ": " << sError);
        CHECK(sError.find("unexpected end of file") != std::string::npos);
    }
}

TEST_CASE("lua undump runs a retail script at either width" * doctest::test_suite("unit"))
{
    std::string sRetail(reinterpret_cast<const char*>(kRetailListTags), sizeof(kRetailListTags));
    REQUIRE(sRetail[kSizeTByte] == 4);
    std::string asChunks[2] = { sRetail, RewriteSizeWidth(sRetail, 8) };
    for (int i = 0; i < 2; ++i)
    {
        lua_State* L = lua_open();
        REQUIRE(L);
        int iLoad = Load(L, asChunks[i]);
        std::string sError = iLoad ? ErrorText(L) : std::string();
        INFO("chunk " << i << ": " << sError);
        REQUIRE(iLoad == 0);
        REQUIRE(lua_pcall(L, 0, 0, 0) == 0);
        CheckRetailTags(L);
        lua_close(L);
    }
}

namespace
{

bool ReadFile(const std::string& sPath, std::string& sData)
{
    FILE* pFile = fopen(sPath.c_str(), "rb");
    if (!pFile)
    {
        return false;
    }
    fseek(pFile, 0, SEEK_END);
    long lSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    sData.resize(lSize > 0 ? (size_t)lSize : 0);
    size_t uRead = sData.empty() ? 0 : fread(&sData[0], 1, sData.size(), pFile);
    fclose(pFile);
    return uRead == sData.size();
}

unsigned int Word(const std::string& sData, size_t uAt)
{
    unsigned int uValue = 0;
    memcpy(&uValue, sData.data() + uAt, sizeof(uValue));
    return uValue;
}

// Finds every scr_ chunk whose NAME and BODY children parse and whose BODY is Lua bytecode.
void FindScripts(const std::string& sLvl, std::vector<std::string>& asNames, std::vector<std::string>& asBodies)
{
    for (size_t uAt = sLvl.find("scr_"); uAt != std::string::npos; uAt = sLvl.find("scr_", uAt + 4))
    {
        if (sLvl.size() - uAt < 8 || Word(sLvl, uAt + 4) > sLvl.size() - uAt - 8)
        {
            continue;
        }
        size_t uEnd = uAt + 8 + Word(sLvl, uAt + 4);
        std::string sName;
        std::string sBody;
        bool bOk = true;
        for (size_t uPos = uAt + 8; uPos + 8 <= uEnd;)
        {
            unsigned int uSize = Word(sLvl, uPos + 4);
            if (uSize > uEnd - uPos - 8)
            {
                bOk = false;
                break;
            }
            if (sLvl.compare(uPos, 4, "NAME") == 0)
            {
                sName = sLvl.substr(uPos + 8, uSize).c_str();
            }
            else if (sLvl.compare(uPos, 4, "BODY") == 0)
            {
                sBody = sLvl.substr(uPos + 8, uSize);
            }
            uPos = (uPos + 8 + uSize + 3) & ~(size_t)3;
        }
        if (bOk && !sName.empty() && sBody.compare(0, 4, "\033Lua") == 0)
        {
            asNames.push_back(sName);
            asBodies.push_back(sBody);
        }
    }
}

}

TEST_CASE("lua undump loads every stock script at either width" * doctest::test_suite("unit"))
{
    REQUIRE_GAMEDATA();
    const char* const apcLvls[] = { "shell.lvl", "common.lvl", "ingame.lvl", "mission.lvl" };
    int iTotal = 0;
    for (int f = 0; f < 4; ++f)
    {
        std::string sPath = std::string(FindGameDataRoot()) + "\\data\\_lvl_pc\\" + apcLvls[f];
        std::string sLvl;
        REQUIRE(ReadFile(sPath, sLvl));
        std::vector<std::string> asNames;
        std::vector<std::string> asBodies;
        FindScripts(sLvl, asNames, asBodies);
        printf("%s: %u scripts\n", apcLvls[f], (unsigned int)asBodies.size());
        CHECK(!asBodies.empty());
        lua_State* L = lua_open();
        REQUIRE(L);
        for (size_t i = 0; i < asBodies.size(); ++i)
        {
            const std::string& sBody = asBodies[i];
            CHECK(sBody[kSizeTByte] == 4);
            int iLoad = Load(L, sBody);
            std::string sError = iLoad ? ErrorText(L) : std::string();
            INFO(apcLvls[f] << " " << asNames[i] << ": " << sError);
            REQUIRE(iLoad == 0);
            std::string sFromNarrow = DumpTop(L);
            REQUIRE(Load(L, RewriteSizeWidth(sBody, 8)) == 0);
            std::string sFromWide = DumpTop(L);
            CHECK(sFromNarrow == sFromWide);
            lua_settop(L, 0);
            ++iTotal;
        }
        lua_close(L);
    }
    printf("%d stock scripts loaded\n", iTotal);
}
