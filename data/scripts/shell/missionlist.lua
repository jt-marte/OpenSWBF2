--
-- Copyright (c) 2005 Pandemic Studios, LLC. All rights reserved.
--

-- List of missions presented to the user for IA/MP/splitscreen/etc.
-- This list is kept in alphabetical order (in English, anyhow).
--
-- Each entry should be in the following form:
-- { -- starts a table entry
-- mapluafile = "END1", -- base name of map, w/o attacking side, no ".lua" either
--
--   OPTIONAL parts, depending on which era(s) are available for this map:
-- era_g = 1, -- [OPTIONAL] Put this in the table if there is a "g" version of the map
-- era_c = 1, -- [OPTIONAL] Put this in the table if there is a "c" version of the map
--
--   OPTIONAL parts, depending in which mode(s) are available for this map:
-- mode_con_c = 1, -- [OPTIONAL] The "_con" version of this map exists, in CLONEWARS
-- mode_con_g = 1, -- [OPTIONAL] The "_con" version of this map exists, in GCW
-- mode_ctf_c = 1, -- [OPTIONAL] The "_ctf" version of this map exists, in CLONEWARS
-- mode_ctf_g = 1, -- [OPTIONAL] The "_ctf" version of this map exists, in GCW
--   [Ditto for the other modes]
--
--
-- }, -- ends a table entry
--
-- Below, things are in one-entry-per-line string.format to make it easier to
-- comment in/out maps by commenting in/out a single line

if(gPCBetaBuild) then
	sp_missionselect_listbox_contents = {
		-- In the below list, the first '%s' will be replaced by the era,
		-- and the second will be replaced by the multiplayer variant name
		-- (the part after "mode_")
	}

	mp_missionselect_listbox_contents = {
		-- In the below list, the first '%s' will be replaced by the era,
		-- and the second will be replaced by the multiplayer variant name
		-- (the part after "mode_")
		{ mapluafile = "spa1%s_%s", era_g = 1,            mode_assault_g = 1, mode_1flag_g = 1,},
		{ mapluafile = "tan1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "uta1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
	}
	
else
	sp_missionselect_listbox_contents = {
		-- In the below list, the first '%s' will be replaced by the era,
		-- and the second will be replaced by the multiplayer variant name
		-- (the part after "mode_")
	--    { mapluafile = "TEST1%s",   era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1,},
	--    { mapluafile = "bes2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "cor1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "dag1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "dea1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1, },
		{ mapluafile = "end1%s_%s", era_g = 1,                            mode_con_g = 1, mode_hunt_g = 1, mode_1flag_g = 1, },
		{ mapluafile = "fel1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "geo1%s_%s",            era_c = 1, mode_con_c = 1, mode_ctf_c = 1, mode_hunt_c = 1, mode_xl_c = 1},
		{ mapluafile = "hot1%s_%s", era_g = 1,            mode_con_g = 1, mode_1flag_g = 1, mode_hunt_g = 1, mode_xl_g = 1},
		{ mapluafile = "kam1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "kas2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_hunt_c = 1,  mode_ctf_c = 1, mode_ctf_g = 1, mode_xl_c = 1, mode_xl_g = 1},
		{ mapluafile = "mus1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "myg1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "nab2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1,mode_ctf_c = 1, mode_ctf_g = 1, mode_hunt_c = 1,},
		{ mapluafile = "pol1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
	--    { mapluafile = "rhn2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "spa1%s_%s", era_g = 1,            mode_assault_g = 1, mode_1flag_g = 1,},
	--    { mapluafile = "spa2%s_%s",            era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "spa3%s_%s",            era_c = 1, mode_assault_c = 1, mode_1flag_c = 1,},
		--{ mapluafile = "spa4%s_%s", era_g = 1,            mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "spa6%s_%s",            era_c = 1, mode_assault_c = 1, mode_1flag_c = 1, },
		{ mapluafile = "spa7%s_%s",            era_c = 1, mode_assault_c = 1, mode_1flag_c = 1, },
		{ mapluafile = "spa8%s_%s", era_g = 1,            mode_assault_g = 1, mode_1flag_g = 1,},
		{ mapluafile = "spa9%s_%s", era_g = 1,            mode_assault_g = 1, mode_1flag_g = 1,},
		{ mapluafile = "tan1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "tat2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1, mode_hunt_g = 1, mode_eli_g = 1,},
		{ mapluafile = "tat3%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "uta1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "yav1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		--{ mapluafile = "kor1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		--{ mapluafile = "sal1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		--{ mapluafile = "spa3%s_%s", era_g = 1,            mode_con_c = 1, mode_con_g = 1, },
		--{ mapluafile = "spa4%s_%s", era_g = 1,            mode_con_c = 1, mode_con_g = 1, },
	}

	mp_missionselect_listbox_contents = {
		-- In the below list, the first '%s' will be replaced by the era,
		-- and the second will be replaced by the multiplayer variant name
		-- (the part after "mode_")
	--    { mapluafile = "TEST1%s",   era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1,},
	--    { mapluafile = "bes2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "cor1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "dag1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "dea1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1, },
		{ mapluafile = "end1%s_%s", era_g = 1,                            mode_con_g = 1, mode_hunt_g = 1, mode_1flag_g = 1, },
		{ mapluafile = "fel1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "geo1%s_%s",            era_c = 1, mode_con_c = 1, mode_ctf_c = 1, mode_hunt_c = 1,},
		{ mapluafile = "hot1%s_%s", era_g = 1,            mode_con_g = 1, mode_1flag_g = 1, mode_hunt_g = 1,},
		{ mapluafile = "kam1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "kas2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_hunt_c = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "mus1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "myg1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
		{ mapluafile = "nab2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1,mode_ctf_c = 1, mode_ctf_g = 1, mode_hunt_c = 1,},
		{ mapluafile = "pol1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1,},
	--    { mapluafile = "rhn2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "spa1%s_%s", era_g = 1,            mode_assault_g = 1, mode_1flag_g = 1,},
	--    { mapluafile = "spa2%s_%s",            era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "spa3%s_%s",            era_c = 1, mode_assault_c = 1,  mode_1flag_c = 1,},
		--{ mapluafile = "spa4%s_%s", era_g = 1,            mode_con_c = 1, mode_con_g = 1, },
		{ mapluafile = "spa6%s_%s",            era_c = 1, mode_assault_c = 1, mode_1flag_c = 1, },
		{ mapluafile = "spa7%s_%s",            era_c = 1, mode_assault_c = 1, mode_1flag_c = 1, },
		{ mapluafile = "spa8%s_%s", era_g = 1,            mode_assault_g = 1, mode_1flag_g = 1,},
		{ mapluafile = "spa9%s_%s", era_g = 1,            mode_assault_g = 1, mode_1flag_g = 1,},
		{ mapluafile = "tan1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "tat2%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_ctf_c = 1, mode_ctf_g = 1, mode_hunt_g = 1, mode_eli_g = 1,},
		{ mapluafile = "tat3%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "uta1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		{ mapluafile = "yav1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, mode_1flag_c = 1, mode_1flag_g = 1,},
		--{ mapluafile = "kor1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		--{ mapluafile = "sal1%s_%s", era_g = 1, era_c = 1, mode_con_c = 1, mode_con_g = 1, },
		--{ mapluafile = "spa3%s_%s", era_g = 1,            mode_con_c = 1, mode_con_g = 1, },
		--{ mapluafile = "spa4%s_%s", era_g = 1,            mode_con_c = 1, mode_con_g = 1, },
	}
end

-- OpenSWBF2: the stock multiplayer maps, before any mod's addme adds more (missionlist_DumpJSON
-- calls the rest mods)
gStockMPMaps = {}
do
	local i
	for i = 1, table.getn(mp_missionselect_listbox_contents) do
		gStockMPMaps[mp_missionselect_listbox_contents[i].mapluafile] = 1
	end
end

-- Singleplayer campaigns. Each of these tables has a set of
-- string.sub-tables, one string.sub-table per mission. These are run through in
-- order. Note: there is a math.max of 255 missions in a campaign. Todo:
-- add in listings for VOs, backgrounds, etc.
--
-- Optional params per line:
-- side = 2, -- forces the user to team 2 (defender). If omitted, team 1 (attacker) is forced
-- intromovie = "", --movie played before yoda
-- outtromovie = "", --movie played after yoda, before mission
-- exitmovie = "", --movie played after mission is done (and you win)
-- [More will be coming, which refer to text to print, voiceover, etc.]

SPCampaign1 = {
    {
        mapluafile = "geo1c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission1name",
        description = "ifs.sp.campaign1.mission1descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "spa2c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission2name",
        description = "ifs.sp.campaign1.mission1descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "myg1c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission6name",
        description = "ifs.sp.campaign1.mission3descr",
        
--          bDimmed = 1,
    },

    {
        mapluafile = "fel1c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission4name",
        description = "ifs.sp.campaign1.mission2descr",
        
--          bDimmed = 1,
    },
    
    {
        mapluafile = "spa3c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission5name",
        description = "ifs.sp.campaign1.mission1descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "kas2c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission7name",
        description = "ifs.sp.campaign1.mission4descr",
        
--          bDimmed = 1,
    },

    {
        mapluafile = "uta1c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission3name",
        description = "ifs.sp.campaign1.mission2descr",
        
--          bDimmed = 1,
    },

    {
        mapluafile = "cor1c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission8name",
        description = "ifs.sp.campaign1.mission5descr",
--          bDimmed = 1,
    },

    {
        mapluafile = "nab2g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission9name",
        description = "ifs.sp.campaign1.mission6descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "spa4g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission10name",
        description = "ifs.sp.campaign1.mission1descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "mus1c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission11name",
        description = "ifs.sp.campaign1.mission7descr",
--          bDimmed = 1,
    },

-- SPA5 is no more - NM 6/24/05    
--     {
--         mapluafile = "spa5g_c",
--         side = 1,
--         showstr = "ifs.sp.campaign1.mission12name",
--         description = "ifs.sp.campaign1.mission1descr",
-- --          bDimmed = 1,
--     },
    
    {
        mapluafile = "kam1c_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission13name",
        description = "ifs.sp.campaign1.mission8descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "dea1g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission14name",
        description = "ifs.sp.campaign1.mission9descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "pol1g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission15name",
        description = "ifs.sp.campaign1.mission9descr",
--          bDimmed = 1,
    },
    
    
    {
        mapluafile = "tan1g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission16name",
        description = "ifs.sp.campaign1.mission10descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "spa1g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission17name",
        description = "ifs.sp.campaign1.mission1descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "yav1g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission18name",
        description = "ifs.sp.campaign1.mission10descr",
--          bDimmed = 1,
    },
    
    {
        mapluafile = "hot1g_c",
        side = 1,
        showstr = "ifs.sp.campaign1.mission19name",
        description = "ifs.sp.campaign1.mission10descr",
--          bDimmed = 1,
    },
} -- end of Campaign 1

-- List of maps usable in attract mode. These should be the raw lua
-- filenames (without ".lua"), and one per line, so we can turn them
-- on/off easily. Historical missions could go in here if desired.

attract_mode_maps = {
    "cor1c_con",
    "cor1c_ctf",
    "cor1g_con",
    "cor1g_ctf",
    "dag1g_con",
    "dea1c_con",
    "dea1g_con",
    "dea1g_1flag",
    "end1g_con",
    "end1g_hunt",
    "fel1c_con",
    "fel1c_1flag",
    "fel1g_con",
    "fel1g_1flag",
    "geo1c_con",
    "hot1g_con",
    "kam1c_con",
    "kas2c_con",
    "kas2g_con",
    "mus1c_con",
    "mus1g_con",
    "myg1c_con",
    "myg1g_con",
    "nab2c_con",
    "nab2g_con",
    "pol1c_con",
    "pol1g_con",
--    "spa1g_con", -- no more spa _con
--    "spa2c_con", -- no more spa _con
    "spa3c_ass",
--    "spa4g_con", -- no more spa _con
    "spa6c_ass",
    "tan1c_con",
    "tan1c_1flag",
    "tan1g_con",
    "tan1g_1flag",
    "tat2c_con",
    "tat2c_ctf",
    "tat2g_con",
    "tat2g_ctf",
    "tat3c_con",
    "tat3g_con",
    "uta1c_con",
    "uta1g_con",
    "yav1c_con",
    "yav1c_1flag",
    "yav1g_con",
    "yav1g_1flag",
}

ifs_era_vbutton_layout = {
    --  yTop = -70,
    xWidth = 350,
    width = 350,
    xSpacing = 10,
    ySpacing = 5,
    font = "gamefont_medium",
    buttonlist = { 
        { tag = "c", string = "common.era.cw", },
        { tag = "g", string = "common.era.gcw", },
    },
    title = "ifs.sp.pick_era",
    -- rotY = 35,
}

-- These are a list of map mode suffixes. They're concatanated onto
-- "mode_" when expanding, etc.
gMapEras = {
    { key = "era_c", showstr = "common.era.cw",  subst = "c", 
        Team1Name = "common.sides.rep.name", Team2Name = "common.sides.cis.name",
        icon1 = "cis_icon", icon2 = "rep_icon",
    },
    { key = "era_g", showstr = "common.era.gcw", subst = "g",
        Team1Name = "common.sides.all.name", Team2Name = "common.sides.imp.name",
        icon1 = "imp_icon", icon2 = "all_icon",
    },
}

-- These are a list of map mode suffixes. They're concatanated onto
-- "mode_" when expanding, etc.
gMapModes = {
	{
		key = "mode_con", showstr = "modename.name.con", 
		descstr = "modename.description.con", subst = "con",
		icon = "mode_icon_con",
	},
	{
		key = "mode_ctf", showstr = "modename.name.ctf",
		descstr = "modename.description.ctf", subst = "ctf",
		icon = "mode_icon_2ctf",
	},
	{
		key = "mode_1flag", showstr = "modename.name.1flag",
		descstr = "modename.description.1flag", subst = "1flag",
		icon = "mode_icon_ctf",
	},
	{
		key = "mode_tdm", showstr = "modename.name.tdm",
		descstr = "modename.description.tdm", subst = "tdm",
		icon = "mode_icon_tdm",
	},
	--     {
	--         key = "mode_obj", showstr = "modename.name.obj",
	--         descstr = "modename.description.obj", subst = "obj",
	--         icon = "mode_icon_obj",
	--     },
	{
		key = "mode_hunt", showstr = "modename.name.hunt",
		descstr = "modename.description.hunt", subst = "hunt",
		icon = "mode_icon_hunt",
	},
	{
		key = "mode_assault", showstr = "modename.name.assault",
		descstr = "modename.description.assault", subst = "ass",
		icon = "mode_icon_ass",
	},
	{
		key = "mode_eli", showstr = "modename.name.assault",
		descstr = "modename.description.elimination", subst = "eli",
		icon = "mode_icon_ass",
	},
	{
		key = "mode_xl", showstr = "modename.name.xl",
		descstr = "modename.description.xl", subst = "xl",
		icon = "mode_icon_XL",
	},
}

------------------------------------------------------------------------------------
-- OpenSWBF2: built-in 1.3-style mod support (our own code, compatible with mods made for
-- the unofficial 1.3 patch, without the patch).
--
-- Any era and game mode works. A mod's mission list entries only need era_<id> and
-- mode_<mode>_<id> keys: ids nobody registered get a record when the lists are expanded
-- (missionlist_DiscoverErasAndModes). custom_gc_<n> script mods can also add records to
-- custom_gEras / custom_gModes, as with the patch ({ id = "k" } or a full record); they are
-- merged by missionlist_BuildRegistry. Names come from the mod's or the game's localize keys
-- (common.era.<id>, modename.name.<id>), else readable English (missionlist_EraUName /
-- missionlist_ModeUName).
------------------------------------------------------------------------------------

custom_gEras = {}
custom_gModes = {}

-- The patch's mission cap, for mods that read it; OpenSWBF2 lists up to 4096 (DownloadableContent.h)
__max_missions__ = 4096

-- English names for well-known mode ids, used when nothing localizes modename.name.<id>
gModeFallbackNames = {
	obj = "Objective", c = "Campaign", uber = "Uber", bf1 = "Battlefront 1", holo = "Holocron",
	ord66 = "Order 66", dm = "Deathmatch", space = "Space Battle", hctf = "Hero CTF",
	c1 = "Custom 1", c2 = "Custom 2", c3 = "Custom 3", c4 = "Custom 4",
	vh = "Vehicles", vhcon = "Vehicle Conquest", vhtdm = "Vehicle TDM", vhctf = "Vehicle CTF",
	lms = "Last Man Standing", race = "Race", koh = "King of the Hill", surv = "Survival",
	rpg = "RPG", wav = "Waves", ctrl = "Control", siege = "Siege", seige = "Siege",
}

-- English names for era ids when nothing localizes common.era.<id>. Era letters are shared by
-- many mods, so only k (the Conversion Pack's KotOR era, by far its most used) is named; the
-- rest read "Era X".
gEraFallbackNames = {
	k = "KotOR",
}

function missionlist_FindEra(Subst)
	local i
	for i = 1, table.getn(gMapEras) do
		if(gMapEras[i].subst == Subst) then
			return gMapEras[i]
		end
	end
end

-- Key is the whole key ("mode_con") or the part after "mode_"
function missionlist_FindMode(Key)
	if(string.sub(Key, 1, 5) ~= "mode_") then
		Key = "mode_" .. Key
	end
	local i
	for i = 1, table.getn(gMapModes) do
		if(gMapModes[i].key == Key) then
			return gMapModes[i]
		end
	end
end

-- A full era record from a bare { id = ... } (or a record that is missing fields)
function missionlist_MakeEra(Rec)
	local Id = Rec.id or Rec.subst or string.sub(Rec.key or "era_?", 5)
	Rec.subst = Rec.subst or Id
	Rec.key = Rec.key or ("era_" .. Id)
	Rec.showstr = Rec.showstr or ("common.era." .. Id)
	Rec.Team1Name = Rec.Team1Name or ("common.sides." .. Id .. ".name")
	Rec.Team2Name = Rec.Team2Name or ("common.sides." .. Id .. ".name")
	return Rec
end

-- A full mode record from a bare { id = ... }. The key keeps the mission flag spelling
-- (mode_<id>_<era>); subst is what the mission name uses.
function missionlist_MakeMode(Rec)
	local Id = Rec.id or string.sub(Rec.key or "mode_?", 6)
	Rec.key = Rec.key or ("mode_" .. Id)
	Rec.subst = Rec.subst or Id
	Rec.showstr = Rec.showstr or ("modename.name." .. Id)
	Rec.descstr = Rec.descstr or ("modename.description." .. Id)
	return Rec
end

function missionlist_AddEra(Rec)
	Rec = missionlist_MakeEra(Rec)
	if(not missionlist_FindEra(Rec.subst)) then
		table.insert(gMapEras, Rec)
		gRegistryVersion = (gRegistryVersion or 0) + 1
	end
	return missionlist_FindEra(Rec.subst)
end

function missionlist_AddMode(Rec)
	Rec = missionlist_MakeMode(Rec)
	if(not missionlist_FindMode(Rec.key)) then
		table.insert(gMapModes, Rec)
		gRegistryVersion = (gRegistryVersion or 0) + 1
	end
	return missionlist_FindMode(Rec.key)
end

-- Merges what custom_gc scripts put in custom_gEras / custom_gModes. Safe to call again.
function missionlist_BuildRegistry()
	local i
	for i = 1, table.getn(custom_gEras) do
		missionlist_AddEra(custom_gEras[i])
	end
	for i = 1, table.getn(custom_gModes) do
		missionlist_AddMode(custom_gModes[i])
	end
	custom_gEras = {}
	custom_gModes = {}
end

-- Registers every era and mode the entries of a mission list use: era_<id> keys, and
-- mode_<mode>_<id> keys (the era is after the last underscore).
function missionlist_DiscoverErasAndModes(List)
	local i, k, v
	for i = 1, table.getn(List) do
		for k, v in List[i] do
			if(type(k) == "string" and v) then
				if(string.sub(k, 1, 4) == "era_") then
					if(string.len(k) > 4) then
						missionlist_AddEra({ id = string.sub(k, 5) })
					end
				elseif(string.sub(k, 1, 5) == "mode_") then
					local _, _, Mode, Era = string.find(k, "^mode_(.+)_([^_]+)$")
					if(Mode and Era) then
						missionlist_AddMode({ key = "mode_" .. Mode })
						missionlist_AddEra({ id = Era })
					end
				end
			end
		end
	end
end

-- The Xbox DLC and the Battlefront Conversion Pack both bring these maps, and the pack's
-- versions are far more detailed. With both installed, the DLC's copy keeps only a Hero Assault
-- the pack's lacks, and goes when nothing is left.
gPreferredMaps = {
	{ keep = "bs2", drop = "bes2" },	-- Bespin: Cloud City
	{ keep = "rh1", drop = "rhn1" },	-- Rhen Var: Harbor
	{ keep = "rnv", drop = "rhn2" },	-- Rhen Var: Citadel
	{ keep = "yv2", drop = "yav2" },	-- Yavin 4: Arena
}

function missionlist_PreferMaps(List)
	local function Find(Code)
		local i
		for i = 1, table.getn(List) do
			if(List[i].mapluafile and string.lower(List[i].mapluafile) == Code .. "%s_%s") then
				return i
			end
		end
	end

	local p, i, k, v
	for p = 1, table.getn(gPreferredMaps) do
		local Keep = Find(gPreferredMaps[p].keep)
		local Drop = Find(gPreferredMaps[p].drop)
		if(Keep and Drop) then
			local Entry = List[Drop]
			local Gone = {}
			for k, v in Entry do
				if(type(k) == "string" and string.sub(k, 1, 5) == "mode_" and
				   (string.sub(k, 1, 9) ~= "mode_eli_" or List[Keep][k])) then
					table.insert(Gone, k)
				end
			end
			for i = 1, table.getn(Gone) do
				Entry[Gone[i]] = nil
			end

			local Used = {}
			local bAny = nil
			for k, v in Entry do
				local _, _, Era = string.find(type(k) == "string" and v and k or "", "^mode_.+_([^_]+)$")
				if(Era) then
					Used[Era] = 1
					bAny = 1
				end
			end
			if(bAny) then
				Gone = {}
				for k, v in Entry do
					if(type(k) == "string" and string.sub(k, 1, 4) == "era_" and not Used[string.sub(k, 5)]) then
						table.insert(Gone, k)
					end
				end
				for i = 1, table.getn(Gone) do
					Entry[Gone[i]] = nil
				end
			else
				table.remove(List, Drop)
			end
		end
	end
end

-- OpenSWBF2 -listmissions: the multiplayer mission list as JSON for a launcher's server manager,
-- after the mods' addme scripts (and missionlist_ExpandMaplist's registry) have run. Returns the
-- text and the number of maps. Names are as localized (-lang 0 gives English); a character past
-- ASCII is written as \u00xx of its low byte.
function missionlist_DumpJSON()
	missionlist_ExpandMaplist(1)

	local function Str(s)
		local Out = {}
		local i
		s = s or ""
		for i = 1, string.len(s) do
			local b = string.byte(s, i)
			if(b == 34) then
				table.insert(Out, "\\\"")
			elseif(b == 92) then
				table.insert(Out, "\\\\")
			elseif(b < 32 or b > 126) then
				table.insert(Out, string.format("\\u%04x", b))
			else
				table.insert(Out, string.char(b))
			end
		end
		return "\"" .. table.concat(Out) .. "\""
	end
	local function Name(UStr)
		return Str(UStr and ScriptCB_ununicode(UStr) or "")
	end

	local Eras = {}
	local Modes = {}
	local i, j, k
	for i = 1, table.getn(gMapEras) do
		local Era = gMapEras[i]
		table.insert(Eras, "{\"id\":" .. Str(Era.subst) .. ",\"key\":" .. Str(Era.key) ..
			",\"name\":" .. Name(missionlist_EraUName(Era)) .. "}")
	end
	for i = 1, table.getn(gMapModes) do
		local Mode = gMapModes[i]
		table.insert(Modes, "{\"id\":" .. Str(Mode.subst) .. ",\"key\":" .. Str(string.sub(Mode.key or "", 6)) ..
			",\"flag\":" .. Str(Mode.key) .. ",\"name\":" .. Name(missionlist_ModeUName(Mode)) .. "}")
	end

	local Maps = {}
	local List = mp_missionselect_listbox_contents
	for i = 1, table.getn(List) do
		local Entry = List[i]
		local File = Entry.mapluafile
		local _, _, Code = string.find(File or "", "^(.-)%%s_%%s$")
		if(Code) then
			local MapEras = {}
			local MapModes = {}
			for j = 1, table.getn(gMapEras) do
				local Era = gMapEras[j]
				local bUsed = Entry[Era.key]
				for k = 1, table.getn(gMapModes) do
					bUsed = bUsed or Entry[gMapModes[k].key .. "_" .. Era.subst]
				end
				if(bUsed) then
					table.insert(MapEras, "{\"id\":" .. Str(Era.subst) .. ",\"name\":" .. Name(missionlist_EraUName(Era)) .. "}")
				end
			end
			for k = 1, table.getn(gMapModes) do
				local Mode = gMapModes[k]
				local ModeEras = {}
				local Missions = {}
				for j = 1, table.getn(gMapEras) do
					local Era = gMapEras[j]
					if(Entry[Mode.key .. "_" .. Era.subst]) then
						table.insert(ModeEras, Str(Era.subst))
						table.insert(Missions, Str(Era.subst) .. ":" .. Str(string.format(File, Era.subst, Mode.subst)))
					end
				end
				if(table.getn(ModeEras) > 0) then
					table.insert(MapModes, "{\"id\":" .. Str(Mode.subst) .. ",\"key\":" .. Str(string.sub(Mode.key or "", 6)) ..
						",\"name\":" .. Name(missionlist_ModeUName(Mode)) .. ",\"eras\":[" .. table.concat(ModeEras, ",") ..
						"],\"missions\":{" .. table.concat(Missions, ",") .. "}}")
				end
			end
			local bMod = Entry.isModLevel or not gStockMPMaps[File]
			table.insert(Maps, "{\"code\":" .. Str(Code) .. ",\"file\":" .. Str(File) ..
				",\"name\":" .. Name(missionlist_GetLocalizedMapName(File)) ..
				",\"mod\":" .. (bMod and "true" or "false") ..
				",\"eras\":[" .. table.concat(MapEras, ",") .. "],\"modes\":[" .. table.concat(MapModes, ",") .. "]}")
		end
	end

	return "{\"format\":1,\"eras\":[" .. table.concat(Eras, ",") .. "],\"modes\":[" .. table.concat(Modes, ",") ..
		"],\"maps\":[\n" .. table.concat(Maps, ",\n") .. "\n]}\n", table.getn(Maps)
end

-- Display names as unicode: the localized key if there is one, else readable English
function missionlist_EraUName(Era)
	local UStr = Era.showstr and ScriptCB_getlocalizestr(Era.showstr, 1)
	if(UStr) then
		return UStr
	end
	return ScriptCB_tounicode(gEraFallbackNames[Era.subst or ""] or ("Era " .. string.upper(Era.subst or "?")))
end

function missionlist_ModeUName(Mode)
	local UStr = Mode.showstr and ScriptCB_getlocalizestr(Mode.showstr, 1)
	if(UStr) then
		return UStr
	end
	local Id = string.sub(Mode.key or "mode_?", 6)
	return ScriptCB_tounicode(gModeFallbackNames[Id] or string.upper(Id))
end

-- The patch's getters, for mod scripts that call them
function custom_GetGMapEras()
	missionlist_BuildRegistry()
	return gMapEras
end

function custom_GetGMapModes()
	missionlist_BuildRegistry()
	return gMapModes
end

function custom_GetEraButtonList()
	missionlist_BuildRegistry()
	local List = {}
	local i
	for i = 1, table.getn(gMapEras) do
		table.insert(List, { tag = gMapEras[i].subst, string = gMapEras[i].showstr })
	end
	return List
end

-- Galactic Conquest scenario buttons. custom_gc scripts wrap these to add their own
-- scenarios (ifs_freeform_pickscenario builds its buttons from the list and asks
-- custom_PressedGCButton first; true means the script handled the press).
function custom_GetGCButtonList()
	return {
		{ tag = "load", string = "ifs.meta.load.btnload", },
		{ tag = "1", string = "ifs.meta.Configs.1", },
		{ tag = "2", string = "ifs.meta.Configs.2", },
		{ tag = "3", string = "ifs.meta.Configs.3", },
		{ tag = "4", string = "ifs.meta.Configs.4", },
		{ tag = "custom", string = "ifs.meta.Configs.custom", },
	}
end

function custom_PressedGCButton(Tag)
	return nil
end

gAllMapsStr = "_all_maps_"
gDelAllMapsStr = "_remove_all_"
gAllModesStr = "_all_modes_"
gAllErasStr = "_all_eras_"

function missionlist_mapsorthelper(a, b)
	local Name1, Name2
	Name1 = missionlist_GetLocalizedMapName(a.mapluafile)
	Name1 = ScriptCB_ununicode(Name1)
	Name2 = missionlist_GetLocalizedMapName(b.mapluafile)
	Name2 = ScriptCB_ununicode(Name2)

	return Name1 < Name2
end

-- Expands the maplist from a bunch of modes to a flatter list.
-- If bForMP is true, it expands the MP list. If false, it expands
-- the SP list. 
function missionlist_ExpandMaplist(bForMP)

	if(not gSortedMaplist) then
		-- OpenSWBF2: the mods' addme scripts have run by now; register their eras and modes
		missionlist_BuildRegistry()
		missionlist_PreferMaps(sp_missionselect_listbox_contents)
		missionlist_PreferMaps(mp_missionselect_listbox_contents)
		missionlist_DiscoverErasAndModes(sp_missionselect_listbox_contents)
		missionlist_DiscoverErasAndModes(mp_missionselect_listbox_contents)
		table.sort(sp_missionselect_listbox_contents, missionlist_mapsorthelper)
		table.sort(mp_missionselect_listbox_contents, missionlist_mapsorthelper) 
		gSortedMaplist = 1
	end

    local i,j,k,v,Num
    local SourceList
    if(bForMP) then
        SourceList = mp_missionselect_listbox_contents
    else
        SourceList = sp_missionselect_listbox_contents
    end

    -- Blank dest list for starters
    missionselect_listbox_contents = {}

    local expand_maps_for_pc = false
    if( expand_maps_for_pc ) then
        if(gPlatformStr == "PC") then
            local FilterList = gMapModes
        Num = 1 -- where next entry in missionselect_listbox_contents will go
        for i = 1,table.getn(SourceList) do
                for j = 1,table.getn(FilterList) do
                    local Tag = FilterList[j].key
                    if(SourceList[i][Tag]) then
                        -- Start with blank row
                        missionselect_listbox_contents[Num] = {}
                        -- Copy all items in this table row
                        for k,v in SourceList[i] do
                            missionselect_listbox_contents[Num][k] = v
                            --                  print(" Copying ", k, missionselect_listbox_contents[Num][k])
                        end
                        -- But, we want to rename it in the process, adding in the mapname
                        missionselect_listbox_contents[Num].mapluafile = 
                            string.format(SourceList[i].mapluafile, "%s", FilterList[j].subst)
                        --              print("Added luafile ", missionselect_listbox_contents[Num].mapluafile)

                        Num = Num + 1 -- move on in output list
                    end -- SourceList[i].Tag exists
                end -- k loop over filters
        end -- i loop over input maps
            return
        end
    end

    for i = 1,table.getn(SourceList) do
        if(SourceList[i].mapluafile ~= gAllMapsStr) then
            -- Copy row
            missionselect_listbox_contents[i] = SourceList[i]
            missionselect_listbox_contents[i].bIsWildcard = nil
        end -- Mapluafile is not our magic constant
        -- for multiple selection
        missionselect_listbox_contents[i].bSelected = nil
    end -- i loop over input maps
    --print("++++bSelected clear")
    --remove "all maps" because we have "select all" button
    --missionselect_listbox_contents[table.getn(SourceList) + 1] = { mapluafile = gAllMapsStr, bIsWildcard = 1,}

    -- TODO: alphabetize the list now?
end

-- From a maplist abbreviation (the mapluafile entry from one row of
-- the missionselect_listbox_contents array), returns a table as to
-- which modes are available. Table format is like this:
-- {
--   { showstr = "modename.name.con"}, 
-- }
function missionlist_ExpandModelist(abbrev)
    local i,j,k,Num
    local ret = {}
    Num = 1

    -- Path 1 - if "all maps" is selected, then expand it to be a list
    -- of all modes on all maps
    if(abbrev == gAllMapsStr) then
        local Added = {}
        for i=1, table.getn(missionselect_listbox_contents) do
            for j=1, table.getn(gMapModes) do
                local Tag = gMapModes[j].key
                                for k=1, table.getn(gMapEras) do
                                    local Tag2 = Tag .. "_" .. gMapEras[k].subst

                                    if((missionselect_listbox_contents[i][Tag2]) and (not Added[Tag])) then
                    ret[Num] = gMapModes[j]
                    ret[Num].bIsWildcard = nil
                    Num = Num + 1
                    Added[Tag] = 1 -- note we added this modetype
                                    end -- mode exists on this map, not added yet.
                                end -- k loop over all possible eras
            end -- j loop over modes
        end -- i loop over maps

        -- Assume that there will be > 1 mode when all the maps are expanded
        ret[Num] = { showstr = "modename.name._all_modes_", bIsWildcard = 1, descstr = "modename.description._all_modes_", }
        Num = Num + 1
    else
        -- Path 2 - need to count modes for a particular map
        for i=1, table.getn(missionselect_listbox_contents) do
            if(missionselect_listbox_contents[i].mapluafile == abbrev) then
                local Count = 0
                -- Path 2a - count # of modes available
                            local Added = {}
                for j=1, table.getn(gMapModes) do
                    local Tag = gMapModes[j].key
                                        for k=1, table.getn(gMapEras) do
                                            local Tag2 = Tag .. "_" .. gMapEras[k].subst
                                            if((missionselect_listbox_contents[i][Tag2]) and (not Added[Tag])) then
                        Count = Count + 1
                                                Added[Tag] = 1
                                            end
                                        end -- k loop over possible eras
                end -- j loop counting # of modes

                -- Now, add the modes for real
                                Added = {}
                for j=1, table.getn(gMapModes) do
                    local Tag = gMapModes[j].key
                                        for k=1, table.getn(gMapEras) do
                                            local Tag2 = Tag .. "_" .. gMapEras[k].subst
                                            if((missionselect_listbox_contents[i][Tag2]) and (not Added[Tag])) then
                        ret[Num] = gMapModes[j]
                        ret[Num].bIsWildcard = nil
                        Num = Num + 1
                                                Added[Tag] = 1
                                            end -- mode exists on this map
                                        end -- k loop over eras
                end -- j loop on modes

                -- Put "All modes" at bottom of list if multiple modes.
                if(Count > 1) then
                    ret[Num] = { showstr = "modename.name._all_modes_", bIsWildcard = 1, descstr = "modename.description._all_modes_", }
                    Num = Num + 1
                end
            end -- abbrev matched
        end -- i loop over all maps
    end

    return ret
end

-- From a maplist abbreviation (the mapluafile entry from one row of
-- the missionselect_listbox_contents array), returns a table as to
-- which modes are available. Table format is like this:
-- {
--   { showstr = "modename.name.con"}, 
-- }
--
-- If ModeEntry is nil, it does no filtering. However, if ModeEntry
-- is non-nil, it myst be a row from gMapModes, and the list of eras
-- is filtered by the li
function missionlist_ExpandEralist(abbrev, ModeEntry)
    local i,j,Num
    local ret = {}
    Num = 1

    -- Path 1 - if "all maps" is selected, then expand it to be a list
    -- of all modes on all maps
    if(abbrev == gAllMapsStr) then
        local Added = {}
        for i=1, table.getn(missionselect_listbox_contents) do
            for j=1, table.getn(gMapEras) do
                local Key = gMapEras[j].key
                if((missionselect_listbox_contents[i][Key]) and (not Added[Key])) then
                    ret[Num] = gMapEras[j]
                    ret[Num].bIsWildcard = nil
                    Num = Num + 1
                    Added[Key] = 1 -- note we added this modetype
                end -- mode exists on this map, not added yet.
            end -- j loop over modes
        end -- i loop over maps

        -- Assume that multiple modes will be on all maps
        ret[Num] = { showstr = "common.era._all_eras_", bIsWildcard = 1,}
        Num = Num + 1
    else
        -- Path 2 - need to count modes for a particular map
        for i=1, table.getn(missionselect_listbox_contents) do
            if(missionselect_listbox_contents[i].mapluafile == abbrev) then
                local Count = 0
                -- Path 2a - count # of eras available

                            -- If no mode restriction, then use old path
                            if((not ModeEntry) or (ModeEntry.bIsWildcard)) then
                for j=1, table.getn(gMapEras) do
                    local Key = gMapEras[j].key
                    if(missionselect_listbox_contents[i][Key]) then
                        Count = Count + 1
                    end
                end -- j loop counting # of modes
                            else
                                -- Mode restriction is in place. Filter list to only include
                                -- maps with this mode in place.
                for j=1, table.getn(gMapEras) do
                    local Key = ModeEntry.key .. "_" .. gMapEras[j].subst
                    if(missionselect_listbox_contents[i][Key]) then
                        Count = Count + 1
                    end
                end -- j loop counting # of modes
                            end

                -- Now, add the eras for real
                            -- If no mode restriction, then use old path
                            if((not ModeEntry) or (ModeEntry.bIsWildcard)) then
                for j=1, table.getn(gMapEras) do
                    local Key = gMapEras[j].key
                    if(missionselect_listbox_contents[i][Key]) then
                        ret[Num] = gMapEras[j]
                        ret[Num].bIsWildcard = nil
                        Num = Num + 1
                    end -- mode exists on this map
                end -- j loop on modes

                -- Put "All eras" at bottom of list if multiple eras.
                if(Count > 1) then
                    ret[Num] = { showstr = "common.era._all_eras_", bIsWildcard = 1, }
                    Num = Num + 1
                end
                            else
                                -- Mode restriction is in place. Filter list to only include
                                -- maps with this mode in place.
                for j=1, table.getn(gMapEras) do
                    local Key = ModeEntry.key .. "_" .. gMapEras[j].subst
                    if(missionselect_listbox_contents[i][Key]) then
                        ret[Num] = gMapEras[j]
                        ret[Num].bIsWildcard = nil
                        Num = Num + 1
                    end -- mode exists on this map
                end -- j loop on modes
                            end


            end -- abbrev matched
        end -- i loop over all maps
    end

    return ret
end

-- Tables for cached mapname lookups. The key for each entry will
-- be the 'abbrev' passed into missionlist_GetLocalizedMapName(abbrev)
-- if present.
gMapName0Table = {}
gMapName1Table = {}
gMapName2Table = {}

-- API
-- ShowUStr, iSource = missionlist_GetLocalizedName("tat3a")
--
-- Given an abbreviation (e.g. "tat3a"), returns a localized unicode
-- string that should be shown. Also returns an index of where that
-- string came from:
-- 0: localize database
-- 1: addon map w/ English-only string
-- 2: fallback (abbrev upconverted to Unicode)
function missionlist_GetLocalizedMapName(abbrev)
    -- Look in cached lookups first
    local CacheUStr

    CacheUStr = gMapName0Table[abbrev]
    if(CacheUStr) then
        --      print("gMapName0Table["..abbrev.."] found")
        return CacheUStr, 0
    end

    CacheUStr = gMapName1Table[abbrev]
    if(CacheUStr) then
        --      print("gMapName1Table["..abbrev.."] found")
        return CacheUStr, 1
    end

    CacheUStr = gMapName2Table[abbrev]
    if(CacheUStr) then
        --      print("gMapName2Table["..abbrev.."] found")
        return CacheUStr, 2
    end

    if(abbrev == gAllMapsStr) then
        return ScriptCB_getlocalizestr("mapname.name._all_maps_"), -1
    end

    if(abbrev == gDelAllMapsStr) then
        return ScriptCB_getlocalizestr("mapname.name._remove_all_"), -1
    end

    ------- Not cached. Do lookup, stick it in cache

    local i,j,l,l2
    l = string.len(abbrev)
    l2 = l

    -- Split string at the '_', if present
    for i = 1,(l-1) do
        if(string.sub(abbrev, i, i) == "_") then
            l2 = i
        end
    end

    -- Pass #1: try and find it in localize DB
    for i = 0,(l2-1) do
        local TrimmedStr = string.sub(abbrev, 1, l2 - i)
        local DisplayUStr = ScriptCB_getlocalizestr("mapname.name."..TrimmedStr, 1) -- 2nd param: return nil if not found
        if(DisplayUStr) then
            --          print("Got left name!",string.sub(abbrev,l2+1))
            local ModeUStr = ScriptCB_getlocalizestr("modename." .. string.sub(abbrev,l2+1), 1)
            if(ModeUStr) then
                local SpaceUStr = ScriptCB_tounicode(" ")
                local ShowUStr = ScriptCB_usprintf("common.pctspcts", DisplayUStr, SpaceUStr)
                ShowUStr = ScriptCB_usprintf("common.pctspcts", ShowUStr, ModeUStr)
                
                gMapName0Table[abbrev] = ShowUStr
                return ShowUStr, 0
            else
                gMapName0Table[abbrev] = DisplayUStr
                return DisplayUStr, 0
            end
        end
    end -- i loop over shortened strings

    -- Pass #2, try and find it in current missionlist
    if(missionselect_listbox_contents) then
        for i = 0,1 do
            local TrimmedStr = string.upper(string.sub(abbrev, 1, l - i))
            for j = 1,table.getn(missionselect_listbox_contents) do
                if(TrimmedStr == string.upper(missionselect_listbox_contents[j].mapluafile)) then
                    local DisplayUStr
                    local Entry = missionselect_listbox_contents[j]
                    local CurLang = ScriptCB_GetLanguage()
                    if(CurLang == "uk_english") then
                        CurLang = "english"
                    end
                    local Tag = "showstr_" .. CurLang
                    --                      print("Checking tag ", Tag , Entry.Tag, Entry[Tag], Entry.showstr)
                    if(Entry[Tag]) then
                        DisplayUStr = ScriptCB_tounicode(Entry[Tag])
                    elseif (Entry.showstr) then
                        DisplayUStr = ScriptCB_tounicode(Entry.showstr)
                    else
                        DisplayUStr = ScriptCB_tounicode(Entry.mapluafile)
                    end

                    gMapName1Table[abbrev] = DisplayUStr
                    return DisplayUStr, 1
                end
            end -- j loop over missionselect_listbox_contents
        end -- i loop over trimming suffixes
    end

    -- Final fallback: show what was received
    local DisplayUStr = ScriptCB_tounicode(abbrev)
    gMapName2Table[abbrev] = DisplayUStr
    return DisplayUStr, 2
end

-- Given an row from the missionlist table, returns a string which is
-- the basename for the movie
function missionlist_fnGetMovieName(Selection)
    local movieName = nil
    local movieFile = nil

    if(not Selection) then
        return movieName, movieFile -- emergency bailout
    end

    movieName = Selection.mapluafile
    if(Selection.dnldable) then
        movieFile = movieName
    end

    -- HACK - only use first 4 chars of moviename. This may need to
    -- change eventually. (Trim off everything after first %s, I
    -- suspect)
    movieName = string.sub(movieName,1,4)
    if(movieName == "TEST") then
        movieName = nil
    end

    return movieName, movieFile
end

-- Tables for cached map description lookups. The key for each entry
-- will be the 'abbrev' passed into
-- missionlist_GetLocalizedMapName(abbrev) if present.

gMapDescr0Table = {}
gMapDescr1Table = {}
gMapDescr2Table = {}

-- API
-- ShowUStr, iSource = missionlist_GetLocalizedName("tat3a")
--
-- Given an abbreviation (e.g. "tat3a"), returns a localized unicode
-- string that should be shown. Also returns an index of where that
-- string came from:
-- 0: localize database
-- 1: addon map w/ English-only string
-- 2: fallback (abbrev upconverted to Unicode)
function missionlist_GetLocalizedMapDescr(abbrev)
    -- Look in cached lookups first
    local CacheUStr

    CacheUStr = gMapDescr0Table[abbrev]
    if(CacheUStr) then
        --      print("gMapDescr0Table["..abbrev.."] found")
        return CacheUStr, 0
    end

    CacheUStr = gMapDescr1Table[abbrev]
    if(CacheUStr) then
        --      print("gMapDescr1Table["..abbrev.."] found")
        return CacheUStr, 1
    end

    CacheUStr = gMapDescr2Table[abbrev]
    if(CacheUStr) then
        --      print("gMapDescr2Table["..abbrev.."] found")
        return CacheUStr, 2
    end

    if(abbrev == gAllMapsStr) then
        return ScriptCB_getlocalizestr("mapname.description._all_maps_"), -1
    end

    if(abbrev == gDelAllMapsStr) then
        return ScriptCB_getlocalizestr("mapname.description._remove_all_"), -1
    end

    ------- Not cached. Do lookup, stick it in cache

    local i,j,l,l2
    l = string.len(abbrev)
    l2 = l

    -- Split string at the '_', if present
    for i = 1,(l-1) do
        if(string.sub(abbrev, i, i) == "_") then
            l2 = i
        end
    end

    -- Pass #1: try and find it in localize DB
    for i = 0,(l2-1) do
        local TrimmedStr = string.sub(abbrev, 1, l2 - i)
        local DisplayUStr = ScriptCB_getlocalizestr("mapname.description."..TrimmedStr, 1) -- 2nd param: return nil if not found
        if(DisplayUStr) then
            --          print("Got left name!",string.sub(abbrev,l2+1))
            local ModeUStr = ScriptCB_getlocalizestr("modename." .. string.sub(abbrev,l2+1), 1)
            if(ModeUStr) then
                local SpaceUStr = ScriptCB_tounicode(" ")
                local ShowUStr = ScriptCB_usprintf("common.pctspcts", DisplayUStr, SpaceUStr)
                ShowUStr = ScriptCB_usprintf("common.pctspcts", ShowUStr, ModeUStr)
                
                gMapDescr0Table[abbrev] = ShowUStr
                return ShowUStr, 0
            else
                gMapDescr0Table[abbrev] = DisplayUStr
                return DisplayUStr, 0
            end
        end
    end -- i loop over shortened strings

    -- Pass #2, try and find it in current missionlist
    if(missionselect_listbox_contents) then
        for i = 0,1 do
            local TrimmedStr = string.upper(string.sub(abbrev, 1, l - i))
            for j = 1,table.getn(missionselect_listbox_contents) do
                if(TrimmedStr == string.upper(missionselect_listbox_contents[j].mapluafile)) then
                    local DisplayUStr
                    local Entry = missionselect_listbox_contents[j]
                    local CurLang = ScriptCB_GetLanguage()
                    if(CurLang == "uk_english") then
                        CurLang = "english"
                    end
                    local Tag = "showstr_" .. CurLang
                    --                      print("Checking tag ", Tag , Entry.Tag, Entry[Tag], Entry.showstr)
                    if(Entry[Tag]) then
                        DisplayUStr = ScriptCB_tounicode(Entry[Tag])
                    elseif (Entry.showstr) then
                        DisplayUStr = ScriptCB_tounicode(Entry.showstr)
                    else
                        DisplayUStr = ScriptCB_tounicode(Entry.mapluafile)
                    end

                    gMapDescr1Table[abbrev] = DisplayUStr
                    return DisplayUStr, 1
                end
            end -- j loop over missionselect_listbox_contents
        end -- i loop over trimming suffixes
    end

    -- Final fallback: show what was received
    local DisplayUStr = ScriptCB_tounicode(abbrev)
    gMapDescr2Table[abbrev] = DisplayUStr
    return DisplayUStr, 2
end

-- Given an row from the missionlist table, returns a string which is
-- the basename for the movie
function missionlist_fnGetMovieName(Selection)
    local movieName = nil
    local movieFile = nil

    if(not Selection) then
        return movieName, movieFile -- emergency bailout
    end

    movieName = Selection.mapluafile
    if(Selection.dnldable) then
        movieFile = movieName
    end

    -- HACK - only use first 4 chars of moviename. This may need to
    -- change eventually. (Trim off everything after first %s, I
    -- suspect)
    movieName = string.sub(movieName,1,4)
    if(movieName == "TEST") then
        movieName = nil
    end

    return movieName, movieFile
end

-- Takes a map luafile (e.g. "end1g_con") and returns the row from gMapModes
-- that's appropriate for that map. Returns nil if the mode isn't found
function missionlist_GetMapMode(abbrev)
    local i,j,l,l2
    l = string.len(abbrev)
    l2 = 0

    -- Split string at the '_', if present
    for i = 1,(l-1) do
        if(string.sub(abbrev, i, i) == "_") then
            l2 = i
        end
    end

    local CheckMode = string.sub(abbrev, l2+1)
    for i = 1, table.getn(gMapModes) do
        if(gMapModes[i].subst == CheckMode) then
            return gMapModes[i]
        end     
    end

    return nil -- not found
end

-- Takes a map luafile (e.g. "end1g_con") and returns the row from gMapEras
-- that's appropriate for that map. Returns nil if the mode isn't found
function missionlist_GetMapEra(abbrev)
    local i,j,l,l2
    l = string.len(abbrev)
    l2 = l

    -- Split string at the '_', if present
    for i = 1,(l-1) do
        if(string.sub(abbrev, i, i) == "_") then
            l2 = i
        end
    end

    local CheckChar = string.sub(abbrev, l2-1, l2-1)
    for i = 1, table.getn(gMapEras) do
        if(gMapEras[i].subst == CheckChar) then
            return gMapEras[i]
        end     
    end

    return nil -- not found
end

-- Fill in some items that differ per-platform
if (gPlatformStr == "PS2") then
    SPCampaign1[1].outtromovie = "tutorial01cw"
    SPCampaign1[1].outtromovie_left = 50
    SPCampaign1[1].outtromovie_top = 60
    SPCampaign1[1].outtromovie_width = 400
    SPCampaign1[1].outtromovie_height = 336
    SPCampaign1[1].outtromovielocalized = 1

    --     SPCampaign2[1].outtromovie = "tutorial01gcw"
    --     SPCampaign2[1].outtromovie_left = 50
    --     SPCampaign2[1].outtromovie_top = 60
    --     SPCampaign2[1].outtromovie_width = 400
    --     SPCampaign2[1].outtromovie_height = 336
    --     SPCampaign2[1].outtromovielocalized = 1
elseif (gPlatformStr == "XBox") then
    SPCampaign1[1].outtromovie = "tutorial01cw"
    SPCampaign1[1].outtromovie_left = 90
    SPCampaign1[1].outtromovie_top = 60
    SPCampaign1[1].outtromovie_width = 460
    SPCampaign1[1].outtromovie_height = 350
    SPCampaign1[1].outtromovielocalized = 1

    --     SPCampaign2[1].outtromovie = "tutorial01gcw"
    --     SPCampaign2[1].outtromovie_left = 90
    --     SPCampaign2[1].outtromovie_top = 60
    --     SPCampaign2[1].outtromovie_width = 460
    --     SPCampaign2[1].outtromovie_height = 350
    --     SPCampaign2[1].outtromovielocalized = 1
elseif (gPlatformStr == "PC") then
    -- Zap out all 'unlockable' flags
    local i
    for i=1,table.getn(SPCampaign1) do
        SPCampaign1[i].unlockable = nil
    end

    --     for i=1,table.getn(SPCampaign2) do
    --         SPCampaign2[i].unlockable = nil
    --     end
end

