#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string>

#include "LuaFL/include/lua.h"
#include "LuaFL/include/lauxlib.h"
#include "LuaFL/include/lualib.h"

// OpenSWBF2's built-in 1.3-style mod support: the era and mode registry, the numbered
// script-mod slots and the mission cap. The Lua runs here with the base, string, table and math
// libraries, as in game.

namespace
{

bool ReadSource(const char* pcPath, std::string& sOut)
{
    FILE* pFile = fopen(pcPath, "rb");
    if (!pFile)
    {
        return false;
    }
    char acBuf[4096];
    size_t uiRead;
    while ((uiRead = fread(acBuf, 1, sizeof(acBuf), pFile)) > 0)
    {
        sOut.append(acBuf, uiRead);
    }
    fclose(pFile);
    return true;
}

std::string ReadScript(const char* pcPath)
{
    std::string sRaw;
    REQUIRE(ReadSource(pcPath, sRaw));
    std::string sOut;
    for (size_t i = 0; i < sRaw.size(); ++i)
    {
        if (sRaw[i] != '\r')
        {
            sOut += sRaw[i];
        }
    }
    return sOut;
}

bool Has(const std::string& sText, const char* pcNeedle)
{
    return sText.find(pcNeedle) != std::string::npos;
}

// sText from the first pcFrom up to (not including) pcTo
std::string Between(const std::string& sText, const char* pcFrom, const char* pcTo)
{
    const size_t uiBegin = sText.find(pcFrom);
    if (uiBegin == std::string::npos)
    {
        return std::string();
    }
    const size_t uiEnd = sText.find(pcTo, uiBegin);
    if (uiEnd == std::string::npos)
    {
        return std::string();
    }
    return sText.substr(uiBegin, uiEnd - uiBegin);
}

struct Lua
{
    lua_State* L;
    std::string sError;

    Lua() : L(lua_open())
    {
        luaopen_base(L);
        luaopen_string(L);
        luaopen_table(L);
        luaopen_math(L);
        lua_settop(L, 0);
    }
    ~Lua() { lua_close(L); }

    bool Run(const std::string& sCode)
    {
        if (luaL_loadbuffer(L, sCode.c_str(), sCode.size(), "mod_support") != 0 || lua_pcall(L, 0, 0, 0) != 0)
        {
            const char* pcError = lua_tostring(L, -1);
            sError = pcError ? pcError : "unknown Lua error";
            lua_settop(L, 0);
            return false;
        }
        return true;
    }

    std::string Global(const char* pcName)
    {
        lua_getglobal(L, pcName);
        const char* pcValue = lua_isstring(L, -1) ? lua_tostring(L, -1) : 0;
        const std::string sValue = pcValue ? pcValue : "nil";
        lua_pop(L, 1);
        return sValue;
    }
};

// The registry from missionlist.lua: the stock era and mode tables and the OpenSWBF2 code
// after them, with the localize lookups stubbed (only "common.era.cw"/"modename.name.con"
// are "localized").
std::string RegistryCode()
{
    const std::string sList = ReadScript("../data/scripts/shell/missionlist.lua");
    const std::string sBlock = Between(sList, "gMapEras = {", "gAllMapsStr = \"_all_maps_\"");
    REQUIRE(!sBlock.empty());
    return "function ScriptCB_getlocalizestr(k, bNil)\n"
           "  if k == 'common.era.cw' then return 'Clone Wars' end\n"
           "  if k == 'modename.name.con' then return 'Conquest' end\n"
           "end\n"
           "function ScriptCB_tounicode(s) return s end\n" +
           sBlock;
}

} // namespace

TEST_CASE("mod registry finds eras and modes from mission list entries" * doctest::test_suite("unit"))
{
    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(RegistryCode()));
    REQUIRE(lua.Run(
        // a BF1 Conversion Pack style entry (era k, modes the patch introduced) and a stock one
        "local sp = {\n"
        "  { mapluafile = 'bf1%s_%s', era_c = 1, era_k = 1, mode_con_k = 1, mode_bf1_k = 1, mode_uber_k = 1,\n"
        "    mode_assault_g = 1, change = { era_k = { name = 'x' } } },\n"
        "  { mapluafile = 'xyz%s_%s', mode_tdf_c = 1, mode_1flag_c = 1 },\n"
        "}\n"
        "before_eras = table.getn(gMapEras)\n"
        "before_modes = table.getn(gMapModes)\n"
        "missionlist_DiscoverErasAndModes(sp)\n"
        "missionlist_DiscoverErasAndModes(sp)\n"
        "eras = table.getn(gMapEras)\n"
        "modes = table.getn(gMapModes)\n"
        "local k = missionlist_FindEra('k')\n"
        "k_key = k.key; k_show = k.showstr; k_name = missionlist_EraUName(k)\n"
        "c_name = missionlist_EraUName(missionlist_FindEra('c'))\n"
        "local bf1 = missionlist_FindMode('bf1')\n"
        "bf1_key = bf1.key; bf1_subst = bf1.subst; bf1_show = bf1.showstr\n"
        "uber_name = missionlist_ModeUName(missionlist_FindMode('mode_uber'))\n"
        "tdf_name = missionlist_ModeUName(missionlist_FindMode('tdf'))\n"
        "con_name = missionlist_ModeUName(missionlist_FindMode('con'))\n"
        "assault_subst = missionlist_FindMode('assault').subst\n"
        // the mission name the menus build from an entry
        "mission = string.format(sp[1].mapluafile, k.subst, bf1.subst)\n"));

    CHECK(lua.Global("before_eras") == "2");
    CHECK(lua.Global("before_modes") == "8");
    CHECK(lua.Global("eras") == "3");   // c, g, k; scanning twice adds nothing
    CHECK(lua.Global("modes") == "11"); // + bf1, uber, tdf (con, assault, 1flag were there)
    CHECK(lua.Global("k_key") == "era_k");
    CHECK(lua.Global("k_show") == "common.era.k");
    CHECK(lua.Global("k_name") == "KotOR");
    CHECK(lua.Global("c_name") == "Clone Wars");
    CHECK(lua.Global("bf1_key") == "mode_bf1");
    CHECK(lua.Global("bf1_subst") == "bf1");
    CHECK(lua.Global("bf1_show") == "modename.name.bf1");
    CHECK(lua.Global("uber_name") == "Uber");
    CHECK(lua.Global("tdf_name") == "TDF");
    CHECK(lua.Global("con_name") == "Conquest");
    CHECK(lua.Global("assault_subst") == "ass"); // the stock record, not a new one
    CHECK(lua.Global("mission") == "bf1k_bf1");
}

TEST_CASE("mod registry merges what custom_gc scripts add" * doctest::test_suite("unit"))
{
    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(RegistryCode()));
    REQUIRE(lua.Run(
        "table.insert(custom_gEras, { id = 'z', Team1Name = 'my.team1' })\n"
        "table.insert(custom_gModes, { id = 'siege' })\n"
        "table.insert(custom_gModes, { key = 'mode_con' })\n" // already known
        "local eras = custom_GetGMapEras()\n"
        "missionlist_BuildRegistry()\n" // again: nothing doubles
        "n_eras = table.getn(eras)\n"
        "n_modes = table.getn(gMapModes)\n"
        "z_team = missionlist_FindEra('z').Team1Name\n"
        "z_team2 = missionlist_FindEra('z').Team2Name\n"
        "siege = missionlist_ModeUName(missionlist_FindMode('siege'))\n"
        "z_name = missionlist_EraUName(missionlist_FindEra('z'))\n"
        "local buttons = custom_GetEraButtonList()\n"
        "button3 = buttons[3].tag .. '=' .. buttons[3].string\n"
        "local gc = custom_GetGCButtonList()\n"
        "gc_first = gc[1].tag; gc_last = gc[table.getn(gc)].tag\n"
        "pressed = tostring(custom_PressedGCButton('1'))\n"
        "cap = __max_missions__\n"));

    CHECK(lua.Global("n_eras") == "3");
    CHECK(lua.Global("n_modes") == "9");
    CHECK(lua.Global("z_team") == "my.team1");
    CHECK(lua.Global("z_team2") == "common.sides.z.name");
    CHECK(lua.Global("siege") == "Siege");
    CHECK(lua.Global("z_name") == "Era Z");
    CHECK(lua.Global("button3") == "z=common.era.z");
    CHECK(lua.Global("gc_first") == "load");
    CHECK(lua.Global("gc_last") == "custom");
    CHECK(lua.Global("pressed") == "nil");
    CHECK(lua.Global("cap") == "4096");
}

TEST_CASE("script mod slots run each script once and skip empty files" * doctest::test_suite("unit"))
{
    const std::string sGlobals = ReadScript("../data/scripts/common/globals.lua");
    const size_t uiAt = sGlobals.find("function ScriptSlots_Load(Prefix)");
    REQUIRE(uiAt != std::string::npos);
    const std::string sLoad = sGlobals.substr(uiAt);

    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(
        "ran = ''\n"
        "read = ''\n"
        "function print() end\n"
        // slots 3, 10 and 11 hold scripts, 7 is a 0-byte placeholder; slot 10 loads 11
        // itself, like the 1.3 patch's own slot 10 did
        "usable = { ['user_script_3.lvl'] = true, ['user_script_10.lvl'] = true, ['user_script_11.lvl'] = true }\n"
        "function ScriptCB_IsLvlUsable(name) return usable[name] or false end\n"
        "function ReadDataFile(name) read = read .. name .. ';' end\n"
        // like the engine's, which does nothing unless it gets exactly one argument
        "function ScriptCB_DoFile(...)\n"
        "  if arg.n ~= 1 then error('ScriptCB_DoFile got ' .. arg.n .. ' arguments') end\n"
        "  local name = arg[1]\n"
        "  ran = ran .. name .. ';'\n"
        "  if name == 'user_script_10' then ReadDataFile('user_script_11.lvl'); ScriptCB_DoFile('user_script_11') end\n"
        "end\n" +
        sLoad +
        "ScriptSlots_Load('user_script_')\n"
        "first = ran\n"
        // outside the loader nothing is filtered
        "ran = ''\n"
        "ScriptCB_DoFile('user_script_3')\n"
        "again = ran\n"
        // the shell loads its own slots the same way
        "ran = ''\n"
        "usable = { ['custom_gc_1.lvl'] = true }\n"
        "ScriptSlots_Load('custom_gc_')\n"
        "shell = ran\n"));

    CHECK(lua.Global("first") == "user_script_3;user_script_10;user_script_11;");
    CHECK(lua.Global("read") == "user_script_3.lvl;user_script_10.lvl;user_script_11.lvl;custom_gc_1.lvl;");
    CHECK(lua.Global("again") == "user_script_3;");
    CHECK(lua.Global("shell") == "custom_gc_1;");
}

TEST_CASE("mission select offers and adds every era and mode a map has" * doctest::test_suite("unit"))
{
    const std::string sPc = ReadScript("../data/scripts/shell/ifs_missionselect_pcmulti.lua");
    const std::string sList = ReadScript("../data/scripts/shell/missionlist.lua");
    const std::string sChecks = Between(sPc, "ifs_mspc_kModeRows = 6", "function ifs_missionselect_pcMulti_fnSetMapListColor(");
    const std::string sShown = Between(sPc, "function ifs_missionselect_pcMulti_fnSelectedMaps( this )",
                                       "function ifs_missionselect_pcMulti_fnShowHideGameModesMulti( this )");
    const std::string sAdd = Between(sPc, "function ifs_missionselect_pcMulti_fnAddMap3(",
                                     "function ifs_missionselect_pcMulti_fnDeleteMapNew(");
    const std::string sExpand = Between(sList, "function missionlist_ExpandModelist(abbrev)", "\nfunction ");
    REQUIRE(!sChecks.empty());
    REQUIRE(!sShown.empty());
    REQUIRE(!sAdd.empty());
    REQUIRE(!sExpand.empty());

    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(RegistryCode()));
    REQUIRE(lua.Run(
        "function print() end\n"
        "function assert(v) return v end\n"
        "function IFObj_fnSetVis() end\n"
        "function IFText_fnSetUString() end\n"
        "textures = ''\n"
        "function IFImage_fnSetTexture(obj, tex) if obj and obj.tag then textures = textures .. obj.tag .. '=' .. tex .. ';' end end\n"
        "function ListManager_fnFillContents() end\n" +
        sChecks + sShown + sAdd + sExpand + "\n"
        "ifs_missionselect = {}\n" // the single player screen
        "ifs_mspc_MapList_layout = { SelectedIdx = 1 }\n"
        "ifs_mspc_ModeList_layout = {}\n"
        "gMissionselectModes = {}\n"
        // a BF1 Conversion Pack map: its KotOR era only shows in mode keys; and a stock map
        "missionselect_listbox_contents = {\n"
        "  { mapluafile = 'AB1%s_%s', era_c = 1, mode_con_c = 1, mode_bf1_c = 1, mode_xl_c = 1, mode_eli_c = 1,\n"
        "    mode_con_k = 1, bSelected = 1 },\n"
        "  { mapluafile = 'tat2%s_%s', era_g = 1, mode_con_g = 1, mode_ctf_g = 1 },\n"
        "}\n"
        "missionlist_DiscoverErasAndModes(missionselect_listbox_contents)\n"
        "function keys(list) local s = '' for i = 1, table.getn(list) do s = s .. list[i].key .. ',' end return s end\n"
        "local sp = ifs_missionselect\n"
        "sp_modes = keys(ifs_missionselect_pcMulti_fnShownModes(sp))\n"
        "mp_modes = keys(ifs_missionselect_pcMulti_fnShownModes({}))\n"
        "eras = keys(ifs_missionselect_pcMulti_fnShownEras(sp))\n"
        "added = ''\n"
        "function ifs_missionselect_pcMulti_fnAddMap4(this, Map, Mode, Era)\n"
        "  added = added .. string.format(Map.mapluafile, Era.subst, Mode.subst) .. ';'\n"
        "end\n"
        "sp.ShownEras = ifs_missionselect_pcMulti_fnShownEras(sp)\n"
        "ifs_missionselect_pcMulti_fnAddMapNew(sp)\n"
        "all = added\n"
        // untick Classic Conquest and the Clone Wars era by clicking their rows
        "sp.ShownModes = ifs_missionselect_pcMulti_fnShownModes(sp)\n"
        "sp.mode_checkbox = { { checkbox = { tag = 'check_mode1' } }, { checkbox = { tag = 'check_mode2' } } }\n"
        "for i = 3, ifs_mspc_kModeRows do sp.mode_checkbox[i] = { checkbox = {} } end\n"
        "sp.mode_checkbox[1].key = 'mode_bf1'\n"
        "sp.CurButton = 'check_mode1'\n"
        "ifs_missionselect_pcMulti_fnClickCheckButtons(sp)\n"
        "sp.era_checkbox = { { key = 'era_c', Check_Era = {} } }\n"
        "for i = 2, ifs_mspc_kEraRows do sp.era_checkbox[i] = { Check_Era = {} } end\n"
        "sp.CurButton = 'check_era1'\n"
        "ifs_missionselect_pcMulti_fnClickCheckButtons(sp)\n"
        "bf1_off = tostring(sp.ModeOff['mode_bf1'])\n"
        "c_off = tostring(sp.EraOff['era_c'])\n"
        "added = ''\n"
        "ifs_missionselect_pcMulti_fnAddMapNew(sp)\n"
        "some = added\n"
        // four modes, six rows: the scroll arrows clamp at the top
        "sp.CurButton = '_mode_down'\n"
        "ifs_missionselect_pcMulti_fnClickCheckButtons(sp)\n"
        "top = sp.ModeTop\n"));

    // retail's order first, Hero Assault shown as Assault, XL only in single player
    CHECK(lua.Global("sp_modes") == "mode_con,mode_assault,mode_xl,mode_bf1,");
    CHECK(lua.Global("mp_modes") == "mode_con,mode_assault,mode_bf1,");
    // the KotOR era counts although the entry has no era_k key
    CHECK(lua.Global("eras") == "era_c,era_k,");
    CHECK(lua.Global("all") == "AB1c_con;AB1c_eli;AB1c_xl;AB1c_bf1;AB1k_con;");
    CHECK(lua.Global("bf1_off") == "1");
    CHECK(lua.Global("c_off") == "1");
    CHECK(lua.Global("some") == "AB1k_con;");
    CHECK(lua.Global("top") == "0");
}

TEST_CASE("mod support is wired into the shell" * doctest::test_suite("regression"))
{
    const std::string sShell = ReadScript("../data/scripts/shell/shell_interface.lua");
    const std::string sList = ReadScript("../data/scripts/shell/missionlist.lua");
    const std::string sPick = ReadScript("../data/scripts/shell/ifs_freeform_pickscenario.lua");
    const std::string sDlc = ReadScript("../src/Battlefront2/Source/DownloadableContent.cpp");
    const std::string sDlcH = ReadScript("../src/Battlefront2/Source/DownloadableContent.h");
    const std::string sPc = ReadScript("../src/Battlefront2/Source/LuaCallbacks_PC.cpp");

    // custom_gc slots load after the mission lists and registry, before the screens
    const size_t uiList = sShell.find("ScriptCB_DoFile(\"missionlist\")");
    const size_t uiSlots = sShell.find("ScriptSlots_Load(\"custom_gc_\")");
    const size_t uiTabs = sShell.find("ScriptCB_DoFile(\"pctabs_options\")");
    REQUIRE(uiList != std::string::npos);
    REQUIRE(uiSlots != std::string::npos);
    REQUIRE(uiTabs != std::string::npos);
    CHECK(uiList < uiSlots);
    CHECK(uiSlots < uiTabs);

    // eras and modes the mods' addme scripts use are registered when the lists expand
    const std::string sExpand = Between(sList, "function missionlist_ExpandMaplist(bForMP)", "table.sort(sp_");
    CHECK(Has(sExpand, "missionlist_DiscoverErasAndModes(sp_missionselect_listbox_contents)"));
    CHECK(Has(sExpand, "missionlist_DiscoverErasAndModes(mp_missionselect_listbox_contents)"));

    // Galactic Conquest scenario buttons come from the hook, and script mods handle their own
    CHECK(Has(sPick, "buttonlist = custom_GetGCButtonList(),"));
    CHECK(Has(sPick, "if (this.CurButton and custom_PressedGCButton(this.CurButton)) then"));

    // past retail's silent 500-mission limit, and the 1.3 patch's own addon is left out
    CHECK(Has(sDlcH, "const int kMaxMissions = 4096;"));
    CHECK(Has(sDlc, "DownloadableMissionType mMissions[kMaxMissions];"));
    CHECK(Has(sDlc, "if (mNumMissions < kMaxMissions)"));
    CHECK(Has(sDlc, "AddOn: more than %d missions"));
    CHECK(Has(sDlc, "_stricmp(pcFolder, \"AAA-v1.3patch\") == 0"));
    CHECK(Has(sDlc, "if (IsSkippedAddOn(findData.name))"));
    CHECK(!Has(sDlc, "< 500"));

    CHECK(Has(sPc, "{ \"ScriptCB_IsLvlUsable\", ScriptCB_IsLvlUsable },"));
}
