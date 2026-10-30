--
-- Copyright (c) 2005 Pandemic Studios, LLC. All rights reserved.
--

-- Master include for StarWars: Frontline ingame interface, lua
-- component. The game should be able to include this file and nothing
-- else.

gPlatformStr = ScriptCB_GetPlatform()

-- Note: we don't call ScriptCB_GetOnlineService(), which returns what
-- the binary was compiled for (XLive vs GameSpy). We want the current
-- connection type, which is "LAN", "XLive" or "GameSpy"
gOnlineServiceStr = ScriptCB_GetConnectType()
--	gOnlineServiceStr = ScriptCB_GetOnlineService()

gLangStr,gLangEnum = ScriptCB_GetLanguage()

-- Load designer-specified globals
ScriptCB_DoFile("globals")

------------------------------------------------------------------------------------
-- OpenSWBF2: built-in 1.3-style mod support in game (our own code, compatible with scripts
-- made for the unofficial 1.3 patch): the utility API missions and user_script mods call,
-- then the user_script_<n> script mods themselves (ScriptSlots_Load, globals.lua). This runs
-- from the mission's ReadDataFile("ingame.lvl"), so the wrappers see every class it adds.
------------------------------------------------------------------------------------

__utility_functions__ = 1
uf_classes = {}

function uf_updateClassIndex(Class)
	if(Class and not uf_isKnownClass(Class)) then
		table.insert(uf_classes, Class)
	end
end

-- The class's index in uf_classes, or false
function uf_isKnownClass(Class)
	local i
	for i = 1, table.getn(uf_classes) do
		if(uf_classes[i] == Class) then
			return i
		end
	end
	return false
end

-- Props: { { name = "MaxHealth", value = "500" }, ... }, set on each class that exists
function uf_changeClassProperties(Classes, Props)
	local i, j
	for i = 1, table.getn(Classes) do
		if(FindEntityClass(Classes[i])) then
			for j = 1, table.getn(Props) do
				SetClassProperty(Classes[i], Props[j].name, Props[j].value)
			end
		end
	end
end

-- Calls Fn(player, unit, Prop, Value) for each spawned character on Teams (default 1 and 2);
-- Who is "ai", "human" or nil for both
function uf_applyFunctionOnTeamUnits(Fn, Prop, Value, Teams, Who)
	Teams = Teams or { 1, 2 }
	local i, j
	for i = 1, table.getn(Teams) do
		for j = 0, GetTeamSize(Teams[i]) - 1 do
			local Player = GetTeamMember(Teams[i], j)
			local bHuman = IsCharacterHuman(Player)
			if((not Who) or ((Who == "human") and bHuman) or ((Who == "ai") and not bHuman)) then
				local Unit = GetCharacterUnit(Player)
				if(Unit) then
					Fn(Player, Unit, Prop, Value)
				end
			end
		end
	end
end

function uf_changeObjectProperty(Prop, Value, Who)
	uf_applyFunctionOnTeamUnits(function(Player, Unit, P, V) SetProperty(Unit, P, V) end, Prop, Value, nil, Who)
end

function uf_GetOtherTeam(Team)
	if(Team == 1) then
		return 2
	end
	return 1
end

function uf_moveToTeam(Teams, Team)
	local i
	for i = 1, table.getn(Teams) do
		if(Teams[i] ~= Team) then
			BatchChangeTeams(Teams[i], Team, GetTeamSize(Teams[i]))
		end
	end
end

-- Kills the characters on Teams: the AI ones when bAI, else everyone
function uf_killUnits(Teams, bAI)
	local Who = nil
	if(bAI) then
		Who = "ai"
	end
	uf_applyFunctionOnTeamUnits(function(Player, Unit) KillObject(Unit) end, nil, nil, Teams, Who)
end

function uf_addToServerName(Text)
	if(ScriptCB_InNetGame() and ScriptCB_GetAmHost() and ScriptCB_GetGameName) then
		local Name = ScriptCB_GetGameName() or ""
		if(not string.find(Name, Text, 1, 1)) then
			ScriptCB_SetGameName(Text .. Name)
		end
	end
end

function uf_removeFromServerName(Text)
	if(ScriptCB_InNetGame() and ScriptCB_GetAmHost() and ScriptCB_GetGameName) then
		local Name = ScriptCB_GetGameName() or ""
		local i, j = string.find(Name, Text, 1, 1)
		if(i) then
			ScriptCB_SetGameName(string.sub(Name, 1, i - 1) .. string.sub(Name, j + 1))
		end
	end
end

function uf_print(Value, bNested, Depth)
	Depth = Depth or 0
	if(type(Value) ~= "table") then
		print(string.rep("  ", Depth) .. tostring(Value))
		return
	end
	local k, v
	for k, v in Value do
		print(string.rep("  ", Depth) .. tostring(k) .. " = " .. tostring(v))
		if(bNested and (type(v) == "table")) then
			uf_print(v, bNested, Depth + 1)
		end
	end
end

-- The patch's player list only ever worked in its old lobby; this calls back with none
function uf_processPlayers(Fn)
	Fn({})
end

-- Wrappers: they note the classes a mission adds (uf_classes) and the map's code and mode
-- (__thisMapsCode__ / __thisMapsMode__ from ReadDataFile("dc:ABC\\ABC.lvl", "ABC_conquest")),
-- then pass on exactly the arguments they got and return what the engine returns.
function uf_AddUnitClass(...)
	if(type(arg[2]) == "string") then
		uf_updateClassIndex(arg[2])
	end
	return RetailAddUnitClass(unpack(arg))
end

function uf_SetHeroClass(...)
	if(type(arg[2]) == "string") then
		uf_updateClassIndex(arg[2])
	end
	return RetailSetHeroClass(unpack(arg))
end

function uf_ReadDataFile(...)
	local File, Layer = arg[1], arg[2]
	if((type(File) == "string") and (type(Layer) == "string")) then
		local Path = File
		if(string.lower(string.sub(Path, 1, 3)) == "dc:") then
			Path = string.sub(Path, 4)
		end
		local _, _, Dir, Name = string.find(Path, "^([^\\/]+)[\\/]([^\\/]+)")
		if(Dir and Name and (string.len(Dir) >= 3) and
		   (string.lower(string.sub(Dir, -3)) == string.lower(string.sub(Name, 1, 3)))) then
			__thisMapsCode__ = string.lower(string.sub(Dir, -3))
			__thisMapsMode__ = string.lower(Layer)
		end
	end
	return RetailReadDataFile(unpack(arg))
end

if(not RetailAddUnitClass) then
	RetailAddUnitClass = AddUnitClass
	RetailSetHeroClass = SetHeroClass
	RetailReadDataFile = ReadDataFile
	AddUnitClass = uf_AddUnitClass
	SetHeroClass = uf_SetHeroClass
	ReadDataFile = uf_ReadDataFile
end

-- user_script_<n> script mods from data\_lvl_pc
ScriptSlots_Load("user_script_")

-- Load player stats points
ScriptCB_DoFile("points")

--
--
-- Load utility functions
ScriptCB_DoFile("interface_util")
ScriptCB_DoFile("ifelem_button")
ScriptCB_DoFile("ifelem_roundbutton")
ScriptCB_DoFile("ifelem_flatbutton")
ScriptCB_DoFile("ifelem_buttonwindow")
ScriptCB_DoFile("ifelem_segline")
ScriptCB_DoFile("ifelem_popup")
ScriptCB_DoFile("ifelem_listmanager")
ScriptCB_DoFile("ifelem_helptext")
ScriptCB_DoFile("ifelem_shellscreen")
ScriptCB_DoFile("ifelem_titlebar")
ScriptCB_DoFile("ifelem_borderrect")
ScriptCB_DoFile("ifelem_hslider")
ScriptCB_DoFile("ifelem_form")
ScriptCB_DoFile("ifelem_AnimationMgr")
ScriptCB_DoFile("ifs_movietrans_game")
if(gPlatformStr == "PC") then
	ScriptCB_DoFile("ifelem_tabmanager")
	ScriptCB_DoFile("ifutil_mouse")
	ScriptCB_DoFile("ifelem_editbox")
end

--
--
-- Load screens. Will be activated by gamecode, not us.

ScriptCB_DoFile("ifs_opt_top")

ScriptCB_DoFile("ifs_opt_controller_mode")

if(gPlatformStr == "PC") then
	ScriptCB_DoFile("pctabs_options")
end

if(gPlatformStr ~= "PC") then
	ScriptCB_DoFile("controller_presets")
	ScriptCB_DoFile("ifs_opt_controller_common")
	ScriptCB_DoFile("ifs_opt_controller_vehunit")
end

ScriptCB_DoFile("ifs_opt_general")
ScriptCB_DoFile("ifs_opt_sound")
ScriptCB_DoFile("ifs_opt_mp_listtags")
ScriptCB_DoFile("ifs_opt_mp")

if(gPlatformStr == "PC") then
	ScriptCB_DoFile("ifs_saveop")

	--ScriptCB_DoFile("ifs_opt_pckeyboard")
	ScriptCB_DoFile("ifs_opt_pccontrols")
	ScriptCB_DoFile("ifs_opt_pcvideo")
	ScriptCB_DoFile("ifs_pc_SpawnSelect")
	ScriptCB_DoFile("ifs_pc_spectator")
end

ScriptCB_DoFile("ifs_sideselect")
ScriptCB_DoFile("ifs_charselect")
ScriptCB_DoFile("ifs_mapselect")
ScriptCB_DoFile("ifs_readyselect")

ScriptCB_DoFile("popups_common")
ScriptCB_DoFile("popup_ok")

if(gPlatformStr ~= "PS2") then
	ScriptCB_DoFile("popup_ab")
	ScriptCB_DoFile("popup_busy")
end

ScriptCB_DoFile("popup_yesno")
ScriptCB_DoFile("popup_yesno_large")
ScriptCB_DoFile("error_popup")
if(gPlatformStr == "PC") then
	ScriptCB_DoFile("popup_loadsave2")
end

ScriptCB_DoFile("ifs_pausemenu")
ScriptCB_DoFile("ifs_fakeconsole")

ScriptCB_DoFile("ifs_teamstats")
ScriptCB_DoFile("ifs_personalstats")
ScriptCB_DoFile("ifs_awardstats")
ScriptCB_DoFile("ifs_careerstats")

ScriptCB_DoFile("ifs_mp_lobby")

ScriptCB_DoFile("popups_lobby")
ScriptCB_DoFile("popup_vote")

-- Pull in XBox-only pages (with mpxl in the name)
if(ScriptCB_GetOnlineService() == "XLive") then
	ScriptCB_DoFile("ifs_mpxl_friends")
	ScriptCB_DoFile("ifs_mpxl_feedback")
	ScriptCB_DoFile("ifs_mpxl_voicemail")
else
	ScriptCB_DoFile("ifs_mpgs_friends")
end
