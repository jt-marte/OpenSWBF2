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

TEST_CASE("modes and eras a mod brings get icons in Instant Action" * doctest::test_suite("unit"))
{
    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(RegistryCode()));
    REQUIRE(lua.Run(
        "missionlist_DiscoverErasAndModes({\n"
        "  { mapluafile = 'bs1%s_%s', mode_bf1_k = 1, mode_hctf_g = 1, mode_ord66_c = 1, mode_xyzctf_c = 1,\n"
        "    mode_odd_c = 1, mode_con_k = 1 },\n"
        "})\n"
        "table.insert(custom_gEras, { id = 'z', icon1 = 'zz_icon', icon2 = 'yy_icon' })\n"
        "missionlist_BuildRegistry()\n"
        "bf1 = missionlist_FindMode('bf1').icon\n"
        "hctf = missionlist_FindMode('hctf').icon\n"
        "ord66 = missionlist_FindMode('ord66').icon\n"
        "xyzctf = missionlist_FindMode('xyzctf').icon\n"
        "odd = missionlist_FindMode('odd').icon\n"
        "con = missionlist_FindMode('con').icon\n"
        "k_row = missionlist_FindEra('k').icon2\n"
        "z_row = missionlist_FindEra('z').icon2\n"
        "q_row = tostring((missionlist_FindEra('q')))\n"));

    CHECK(lua.Global("bf1") == "mode_icon_con");
    CHECK(lua.Global("hctf") == "mode_icon_2ctf");
    CHECK(lua.Global("ord66") == "mode_icon_hunt");
    CHECK(lua.Global("xyzctf") == "mode_icon_2ctf"); // by the shape of the name
    CHECK(lua.Global("odd") == "mode_icon_con");
    CHECK(lua.Global("con") == "mode_icon_con"); // the stock record keeps its own
    CHECK(lua.Global("k_row") == "rep_icon"); // KotOR's Republic
    CHECK(lua.Global("z_row") == "yy_icon");  // a mod era keeps its own
    CHECK(lua.Global("q_row") == "nil");      // an era nobody registered

    // The playlist shows an era's icon2, which is what KotOR has (retail asked for icon1)
    const std::string sPc = ReadScript("../data/scripts/shell/ifs_missionselect_pcmulti.lua");
    const std::string sRow = Between(sPc, "function ifs_mspc_PlayList_PopulateItem(", "ifs_mspc_PlayList_layout = {");
    REQUIRE(!sRow.empty());
    CHECK(sRow.find("if((MapEra) and (MapEra.icon2)) then") != std::string::npos);
    CHECK(sRow.find("(MapEra.icon1)) then") == std::string::npos);
}

TEST_CASE("the Conversion Pack's maps win over the Xbox DLC's copies" * doctest::test_suite("unit"))
{
    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(RegistryCode()));
    REQUIRE(lua.Run(
        // the pairs as the two packs' addme scripts list them (shortened)
        "local function Lists()\n"
        "  return {\n"
        "    { mapluafile = 'bes2%s_%s', era_c = 1, era_g = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_g = 1, mode_eli_g = 1 },\n"
        "    { mapluafile = 'rhn1%s_%s', era_c = 1, era_g = 1, mode_con_c = 1, mode_con_g = 1, mode_eli_g = 1 },\n"
        "    { mapluafile = 'cor1%s_%s', era_g = 1, mode_eli_g = 1 },\n"
        "  }\n"
        "end\n"
        "local function Has(List, Code)\n"
        "  local i\n"
        "  for i = 1, table.getn(List) do\n"
        "    if List[i].mapluafile == Code .. '%s_%s' then return List[i] end\n"
        "  end\n"
        "end\n"
        "local function Keys(Entry)\n"
        "  local Out, k = {}\n"
        "  for k in Entry do if k ~= 'mapluafile' then table.insert(Out, k) end end\n"
        "  table.sort(Out)\n"
        "  return table.concat(Out, ',')\n"
        "end\n"
        // the DLC alone: nothing changes
        "local alone = Lists()\n"
        "missionlist_PreferMaps(alone)\n"
        "alone_count = table.getn(alone)\n"
        "alone_rhn1 = Keys(Has(alone, 'rhn1'))\n"
        // with the pack: its Cloud City has Hero Assault (but no CTF), its Harbor has none
        "local both = Lists()\n"
        "table.insert(both, { mapluafile = 'BS2%s_%s', era_c = 1, era_g = 1, era_k = 1, mode_con_c = 1, mode_con_g = 1,\n"
        "  mode_hctf_g = 1, mode_eli_g = 1, mode_con_k = 1 })\n"
        "table.insert(both, { mapluafile = 'rh1%s_%s', era_c = 1, era_g = 1, mode_con_c = 1, mode_con_g = 1, mode_xl_c = 1 })\n"
        "missionlist_PreferMaps(both)\n"
        "both_count = table.getn(both)\n"
        "both_bes2 = Has(both, 'bes2') and 'kept' or 'gone'\n"
        "both_rhn1 = Keys(Has(both, 'rhn1'))\n"
        "both_cor1 = Keys(Has(both, 'cor1'))\n"
        "both_rh1 = Keys(Has(both, 'rh1'))\n"));

    CHECK(lua.Global("alone_count") == "3");
    CHECK(lua.Global("alone_rhn1") == "era_c,era_g,mode_con_c,mode_con_g,mode_eli_g");
    CHECK(lua.Global("both_count") == "4"); // bes2 went, CTF and all: the pack's version wins
    CHECK(lua.Global("both_bes2") == "gone");
    CHECK(lua.Global("both_rhn1") == "era_g,mode_eli_g"); // only a Hero Assault the pack lacks is left
    CHECK(lua.Global("both_cor1") == "era_g,mode_eli_g"); // maps only the DLC has are untouched
    CHECK(lua.Global("both_rh1") == "era_c,era_g,mode_con_c,mode_con_g,mode_xl_c");
}

TEST_CASE("the mission list dump writes what the launcher needs" * doctest::test_suite("unit"))
{
    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(RegistryCode()));
    REQUIRE(lua.Run(
        // the shell's own pieces the dump calls, stubbed: the list expansion runs the registry, and
        // names come from the entries
        "function missionlist_ExpandMaplist(bForMP)\n"
        "  missionlist_BuildRegistry()\n"
        "  missionlist_PreferMaps(mp_missionselect_listbox_contents)\n"
        "  missionlist_DiscoverErasAndModes(mp_missionselect_listbox_contents)\n"
        "end\n"
        "function missionlist_GetLocalizedMapName(f)\n"
        "  local names = { ['geo1%s_%s'] = 'Geonosis', ['BS2%s_%s'] = 'Bespin: \"Cloud\" City\\\\', ['xyz%s_%s'] = 'Caf\\233' }\n"
        "  return names[f] or f, 0\n"
        "end\n"
        "function ScriptCB_ununicode(s) return s end\n"
        "mp_missionselect_listbox_contents = {\n"
        "  { mapluafile = 'geo1%s_%s', era_c = 1, mode_con_c = 1, mode_ctf_c = 1 },\n"
        "  { mapluafile = 'bes2%s_%s', era_g = 1, mode_con_g = 1 },\n"
        "  { mapluafile = 'BS2%s_%s', era_c = 1, era_k = 1, mode_con_c = 1, mode_con_k = 1, mode_bf1_c = 1 },\n"
        "  { mapluafile = 'xyz%s_%s', isModLevel = 1, mode_assault_g = 1 },\n"
        "  { mapluafile = '_all_maps_' },\n"
        "}\n"
        "gStockMPMaps = { ['geo1%s_%s'] = 1, ['xyz%s_%s'] = 1 }\n"
        "json, maps = missionlist_DumpJSON()\n"));

    const std::string sJson = lua.Global("json");
    CAPTURE(sJson);
    CHECK(lua.Global("maps") == "3"); // bes2 gave way to the Conversion Pack's BS2; no "all maps" row
    CHECK(Has(sJson, "{\"format\":1,\"eras\":["));
    CHECK(Has(sJson, "{\"id\":\"k\",\"key\":\"era_k\",\"name\":\"KotOR\"}"));
    CHECK(Has(sJson, "{\"id\":\"ass\",\"key\":\"assault\",\"flag\":\"mode_assault\",\"name\":\"ASSAULT\"}")); // unlocalized here
    CHECK(Has(sJson, "{\"code\":\"geo1\",\"file\":\"geo1%s_%s\",\"name\":\"Geonosis\",\"mod\":false,"
                     "\"eras\":[{\"id\":\"c\",\"name\":\"Clone Wars\"}],"
                     "\"modes\":[{\"id\":\"con\",\"key\":\"con\",\"name\":\"Conquest\",\"eras\":[\"c\"],\"missions\":{\"c\":\"geo1c_con\"}},"
                     "{\"id\":\"ctf\",\"key\":\"ctf\",\"name\":\"CTF\",\"eras\":[\"c\"],\"missions\":{\"c\":\"geo1c_ctf\"}}]}"));
    // a mod map, with quotes and backslashes escaped, a mod mode and the KotOR era
    CHECK(Has(sJson, "\"code\":\"BS2\",\"file\":\"BS2%s_%s\",\"name\":\"Bespin: \\\"Cloud\\\" City\\\\\",\"mod\":true,"));
    CHECK(Has(sJson, "\"missions\":{\"c\":\"BS2c_con\",\"k\":\"BS2k_con\"}"));
    CHECK(Has(sJson, "\"id\":\"bf1\",\"key\":\"bf1\",\"name\":\"Battlefront 1\",\"eras\":[\"c\"],\"missions\":{\"c\":\"BS2c_bf1\"}"));
    // isModLevel marks a mod even when its file matches a stock one; past-ASCII bytes are escaped
    CHECK(Has(sJson, "\"code\":\"xyz\",\"file\":\"xyz%s_%s\",\"name\":\"Caf\\u00e9\",\"mod\":true,"));
    CHECK(Has(sJson, "\"missions\":{\"g\":\"xyzg_ass\"}"));
    CHECK(!Has(sJson, "bes2"));
    CHECK(!Has(sJson, "_all_maps_"));
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

TEST_CASE("in-game mod API wraps class and data calls without changing them" * doctest::test_suite("unit"))
{
    const std::string sGame = ReadScript("../data/scripts/ingame/game_interface.lua");
    const std::string sApi = Between(sGame, "__utility_functions__ = 1", "-- user_script_<n> script mods");
    REQUIRE(!sApi.empty());

    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(
        // the engine's functions: they report how many arguments they got
        "function AddUnitClass(...) return arg.n .. ' args' end\n"
        "function SetHeroClass(...) return 'hero ' .. arg[2] end\n"
        "function ReadDataFile(...) return 'read ' .. arg[1], arg.n end\n"
        "props = ''\n"
        "function FindEntityClass(c) return c == 'rep_inf_trooper' end\n"
        "function SetClassProperty(c, p, v) props = props .. c .. '.' .. p .. '=' .. v .. ';' end\n"
        "engine_add = AddUnitClass\n" +
        sApi +
        "\n"
        "added = AddUnitClass(1, 'rep_inf_trooper', 1, 4)\n"
        "hero = SetHeroClass(1, 'rep_hero_yoda')\n"
        "AddUnitClass(2, 'rep_inf_trooper', 1, 4)\n" // already known
        "known = table.getn(uf_classes) .. ' ' .. tostring(uf_isKnownClass('rep_hero_yoda')) .. ' ' .. tostring(uf_isKnownClass('x'))\n"
        "local r, n = ReadDataFile('side\\\\rep.lvl', 'rep_inf_trooper')\n"
        "read = r .. ' ' .. n\n"
        "code_side = tostring(__thisMapsCode__)\n"
        "ReadDataFile('dc:ABC\\\\ABC.lvl', 'ABC_Conquest')\n"
        "code = __thisMapsCode__ .. ' ' .. __thisMapsMode__\n"
        "ReadDataFile('tat\\\\tat2.lvl', 'tat2_con')\n"
        "stock = __thisMapsCode__ .. ' ' .. __thisMapsMode__\n"
        "uf_changeClassProperties({ 'rep_inf_trooper', 'missing' }, { { name = 'MaxHealth', value = '500' } })\n"
        // running game_interface's API again wraps nothing twice
        "wrapped_once = tostring(RetailAddUnitClass == engine_add)\n" +
        sApi +
        "\n"
        "again = tostring(RetailAddUnitClass == engine_add) .. ' ' .. AddUnitClass(1, 'x', 1, 2)\n"
        "other = uf_GetOtherTeam(1) .. uf_GetOtherTeam(2)\n"));

    CHECK(lua.Global("added") == "4 args");
    CHECK(lua.Global("hero") == "hero rep_hero_yoda");
    CHECK(lua.Global("known") == "2 2 false");
    CHECK(lua.Global("read") == "read side\\rep.lvl 2");
    CHECK(lua.Global("code_side") == "nil");
    CHECK(lua.Global("code") == "abc abc_conquest");
    CHECK(lua.Global("stock") == "tat tat2_con");
    CHECK(lua.Global("props") == "rep_inf_trooper.MaxHealth=500;");
    CHECK(lua.Global("wrapped_once") == "true");
    CHECK(lua.Global("again") == "true 4 args");
    CHECK(lua.Global("other") == "21");
}

TEST_CASE("console lists mods' commands after the engine's" * doctest::test_suite("unit"))
{
    const std::string sConsole = ReadScript("../data/scripts/ingame/ifs_fakeconsole.lua");
    const std::string sApi = Between(sConsole, "__fakeconsole_functions__ = 1", "ifs_fakeconsole = NewIFShellScreen {");
    REQUIRE(!sApi.empty());

    Lua lua;
    CAPTURE(lua.sError);
    REQUIRE(lua.Run(
        "engine = ''\n"
        "function ScriptCB_GetConsoleCmds() gConsoleCmdList = { { ShowStr = 'fps' } } end\n"
        "function ScriptCB_DoConsoleCmd(s) engine = engine .. s .. ';' end\n" +
        sApi +
        "\n"
        "ff_rebuildFakeConsoleList()\n"
        "plain = table.getn(gConsoleCmdList)\n"
        // a user_script mod's commands, as the ISM Skinchanger adds them
        "SupportsCustomFCCommands = true\n"
        "function AddFCCommands() ff_AddCommand('Swap skins', nil, function() ran = 'swapped' end) end\n"
        "ff_rebuildFakeConsoleList()\n"
        "ff_rebuildFakeConsoleList()\n" // opening the console again doesn't double them
        "local l = gConsoleCmdList\n"
        "list = table.getn(l) .. ' ' .. l[1].ShowStr .. ' ' .. tostring(l[2].bHeader) .. ' ' .. l[3].ShowStr\n"
        "found = tostring(ff_DoCommand('Swap skins'))\n"
        "ff_DoCommand('fps')\n"));

    CHECK(lua.Global("plain") == "1");
    CHECK(lua.Global("list") == "3 fps 1 Swap skins");
    CHECK(lua.Global("found") == "true");
    CHECK(lua.Global("ran") == "swapped");
    CHECK(lua.Global("engine") == "fps;");
}

TEST_CASE("mod support is wired into the game" * doctest::test_suite("regression"))
{
    const std::string sGame = ReadScript("../data/scripts/ingame/game_interface.lua");
    const std::string sConsole = ReadScript("../data/scripts/ingame/ifs_fakeconsole.lua");
    const std::string sPause = ReadScript("../data/scripts/common/ifs_pausemenu.lua");
    const std::string sNet = ReadScript("../src/Battlefront2/Source/NetGame.cpp");
    const std::string sText = ReadScript("../src/RedEngineFL/Util/RedLocalizeDB.cpp");
    const std::string sMission = ReadScript("../src/Battlefront2/Source/LuaCallbacks_Mission.cpp");

    // user_script mods load after globals (ScriptSlots_Load) and the utility API
    const size_t uiGlobals = sGame.find("ScriptCB_DoFile(\"globals\")");
    const size_t uiApi = sGame.find("RetailAddUnitClass = AddUnitClass");
    const size_t uiSlots = sGame.find("ScriptSlots_Load(\"user_script_\")");
    REQUIRE(uiGlobals != std::string::npos);
    REQUIRE(uiApi != std::string::npos);
    REQUIRE(uiSlots != std::string::npos);
    CHECK(uiGlobals < uiApi);
    CHECK(uiApi < uiSlots);

    // the console builds its list with the mods' commands and runs a mod's command as Lua
    CHECK(Has(sConsole, "ff_rebuildFakeConsoleList() -- the engine's commands, then the mods'"));
    CHECK(Has(sConsole, "if(Selection.bLua) then"));

    // free camera and console for single player and a multiplayer host
    CHECK(Has(sPause, "local bNoTools = ScriptCB_InNetGame() and not ScriptCB_GetAmHost()"));
    CHECK(Has(sPause, "this.buttons.freecam.hidden = bNoTools"));
    CHECK(Has(sPause, "this.buttons.console.hidden = bNoTools"));
    CHECK(Has(sPause, "{ tag = \"console\", string = \"game.pause.console\", },"));
    CHECK(Has(sText, "{ 0x74e7a80c, L\"Console\" },"));

    // the 1.3 patch's custom side names on the team buttons
    CHECK(Has(sNet, "LuaHelper::GetGlobal(\"SupportsCustomEraTeams\");"));
    CHECK(Has(sNet, "CustomEraTeamName(j + 1, ausCustom, 64) ? ausCustom : pTeam->GetName();"));

    // a script can't leave a class with no model (the ISM Skinchanger names models only another
    // mod loads; the first command post built then crashed)
    CHECK(Has(sMission, "if (PblHash(pcProperty) == 0x47c86b4a && !GameModel::Find(PblHash(pcSafe))) // GeometryName"));
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
    // before that, the Conversion Pack's maps win over the Xbox DLC's copies
    CHECK(sExpand.find("missionlist_PreferMaps(sp_missionselect_listbox_contents)") <
          sExpand.find("missionlist_DiscoverErasAndModes(sp_missionselect_listbox_contents)"));
    CHECK(Has(sExpand, "missionlist_PreferMaps(mp_missionselect_listbox_contents)"));

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
