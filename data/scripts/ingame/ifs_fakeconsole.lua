--
-- Copyright (c) 2005 Pandemic Studios, LLC. All rights reserved.
--

-- Multiplayer game name screen.

-- Helper function. Given a layout (x,y,width, height), returns a
-- fully-built item.
function Fakeconsole_Listbox_CreateItem(layout)

	local insidewidth = layout.width - 20;
	-- Make a coordinate system pegged to the top-left of where the cursor would go.
	local Temp = NewIFContainer {
		x = layout.x - 0.5 * insidewidth, y=layout.y + 2,
		bInertPos = 1,
	}
	local FontHeight = fakeconsole_listbox_layout.fontheight
	Temp.showstr = NewIFText{
		x = 10, y = FontHeight * -0.5, textw = insidewidth, texth = layout.height,
		halign = "left", valign = "vcenter",
		font = fakeconsole_listbox_layout.font,
		ColorR= 255, ColorG = 255, ColorB = 255,
		style = "normal",
		nocreatebackground=1,
		inert_all = 1,
	}

	return Temp
end

-- Helper function. For a destination item (previously created w/
-- CreateItem), fills it in with data, which may be nil (==blank it)
function Fakeconsole_Listbox_PopulateItem(Dest, Data, bSelected, iColorR, iColorG, iColorB, fAlpha)
	if(Data) then
		-- Contents to show. Do so.
		if(gBlankListbox) then
			IFText_fnSetString(Dest.showstr,"") -- reduce glyphcache usage
		elseif(Data.bLua) then
			-- OpenSWBF2: a mod's command (ff_AddCommand) is plain text, not a localize key
			IFText_fnSetUString(Dest.showstr,ScriptCB_tounicode(Data.ShowStr))
		else
			IFText_fnSetString(Dest.showstr,Data.ShowStr)
		end

		IFObj_fnSetColor(Dest.showstr, iColorR, iColorG, iColorB)
		IFObj_fnSetAlpha(Dest.showstr, fAlpha)
	end

	IFObj_fnSetVis(Dest,Data) -- Show if there are contents	
end

fakeconsole_listbox_layout = {
	-- Height is calculated from yHeight, Spacing, showcount.
	yHeight = 22,
	ySpacing  = 0,
	showcount = 9,
	font = gListboxItemFont,

 	width = 320,
	x = 0,
	slider = 1,
	CreateFn = Fakeconsole_Listbox_CreateItem,
	PopulateFn = Fakeconsole_Listbox_PopulateItem,
}

gConsoleCmdList = {}

------------------------------------------------------------------------------------
-- OpenSWBF2: built-in 1.3-style console API (our own code, compatible with scripts made for
-- the unofficial 1.3 patch). A mod sets SupportsCustomFCCommands and defines AddFCCommands(),
-- which calls ff_AddCommand; that runs each time the console opens, and the mod's commands
-- are listed after the engine's own under "[Custom Map Commands]".
------------------------------------------------------------------------------------

__fakeconsole_functions__ = 1
gFFCommands = {}

-- RunFn runs when the command is picked. InfoStr and NowFn are kept for scripts that read
-- them back.
function ff_AddCommand(ShowStr, InfoStr, RunFn, NowFn)
	table.insert(gFFCommands, { ShowStr = ShowStr, info = InfoStr, run = RunFn, now = NowFn, bLua = 1 })
end

function ff_rebuildFakeConsoleList()
	gConsoleCmdList = {}
	ScriptCB_GetConsoleCmds() -- puts the engine's commands in gConsoleCmdList
	gFFCommands = {}
	if(SupportsCustomFCCommands and AddFCCommands) then
		AddFCCommands()
	end
	if(table.getn(gFFCommands) > 0) then
		table.insert(gConsoleCmdList, { ShowStr = "[Custom Map Commands]", bLua = 1, bHeader = 1 })
		local i
		for i = 1, table.getn(gFFCommands) do
			table.insert(gConsoleCmdList, gFFCommands[i])
		end
	end
end

-- Runs a mod's command by name, else an engine command
function ff_DoCommand(ShowStr)
	local i
	for i = 1, table.getn(gFFCommands) do
		if(gFFCommands[i].ShowStr == ShowStr) then
			if(gFFCommands[i].run) then
				gFFCommands[i].run()
			end
			return true
		end
	end
	ScriptCB_DoConsoleCmd(ShowStr)
	return false
end

-- Asks a yes/no question (a localize key or plain text); Callback(bYes)
function ff_AskUser(Title, Callback)
	Popup_YesNo.CurButton = "no"
	Popup_YesNo.fnDone = function(bResult)
		Popup_YesNo:fnActivate(nil)
		if(Callback) then
			Callback(bResult)
		end
	end
	Popup_YesNo:fnActivate(1)
	if(ScriptCB_getlocalizestr(Title, 1)) then
		gPopup_fnSetTitleStr(Popup_YesNo, Title)
	else
		gPopup_fnSetTitleUStr(Popup_YesNo, ScriptCB_tounicode(Title))
	end
end

-- Award effects off: the buffs still work, their visual and sound effects go
function ff_CommandRemoveAwardEffects()
	uf_changeClassProperties(uf_classes, {
		{ name = "BuffHealthEffect", value = "" },
		{ name = "BuffOffenseEffect", value = "" },
		{ name = "BuffDefenseEffect", value = "" },
	})
end

function ff_healthRegen(Rate, Who)
	uf_changeObjectProperty("AddHealth", Rate, Who)
end

function ff_changeAIDamageThreshold(Value)
	SetAIDamageThreshold("all", Value)
end

-- Marks a multiplayer game whose host used console commands, as the patch did
function ff_serverDidFCCmd()
	uf_addToServerName("[console] ")
end

ifs_fakeconsole = NewIFShellScreen {
	nologo = 1,

	Enter = function(this, bFwd)


						-- MUST do this after AddIFScreen! This is done here, and not in
						-- Enter to make the memory footprint more consistent.
						fakeconsole_listbox_layout.FirstShownIdx = 1
						fakeconsole_listbox_layout.SelectedIdx = 1
						fakeconsole_listbox_layout.CursorIdx = 1
						ff_rebuildFakeConsoleList() -- the engine's commands, then the mods' (OpenSWBF2)

		ListManager_fnFillContents(ifs_fakeconsole.listbox,gConsoleCmdList,fakeconsole_listbox_layout)
	end,

	Exit = function(this, bFwd)
		gBlankListbox = 1
		ListManager_fnFillContents(ifs_fakeconsole.listbox,gConsoleCmdList,fakeconsole_listbox_layout)
		gBlankListbox = nil
	end,

	-- Accept button bumps the page
	Input_Accept = function(this)
		if(gMouseListBoxSlider) then
			ListManager_fnScrollbarClick(gMouseListBoxSlider)
			return
		end
		if(gMouseListBox) then
			ScriptCB_SndPlaySound("shell_select_change")
			gMouseListBox.Layout.SelectedIdx = gMouseListBox.Layout.CursorIdx
			ListManager_fnFillContents(gMouseListBox,gMouseListBox.Contents,gMouseListBox.Layout)
--			return
		end

		if(this.CurButton == "_back") then -- Make PC work better - NM 8/5/04
			this:Input_Back()
			return
		end

		local Selection = gConsoleCmdList[fakeconsole_listbox_layout.SelectedIdx]
		-- OpenSWBF2: the "[Custom Map Commands]" header does nothing; a mod's command runs its Lua
		if((not Selection) or Selection.bHeader) then
			return
		end
		ScriptCB_SndPlaySound("shell_menu_enter");
		if(Selection.bLua) then
			if(Selection.run) then
				Selection.run()
			end
		else
			ScriptCB_DoConsoleCmd(Selection.ShowStr)
		end
		ScriptCB_PopScreen()
	end,

	--Back button quits this screen
	Input_Back = function(this)
		ScriptCB_SndPlaySound("shell_menu_exit");
		ScriptCB_PopScreen()
	end,

	Input_GeneralUp = function(this)
		ListManager_fnNavUp(this.listbox,gConsoleCmdList,fakeconsole_listbox_layout)
	end,
	Input_GeneralDown = function(this)
		ListManager_fnNavDown(this.listbox,gConsoleCmdList,fakeconsole_listbox_layout)
	end,

	Input_LTrigger = function(this)
		ListManager_fnPageUp(this.listbox,gConsoleCmdList,fakeconsole_listbox_layout)
	end,
	Input_RTrigger = function(this)
		ListManager_fnPageDown(this.listbox,gConsoleCmdList,fakeconsole_listbox_layout)
	end,

	-- No L/R functionality possible on this screen (gotta have stubs
	-- here, or the base class will override)
	Input_GeneralLeft = function(this)
	end,
	Input_GeneralRight = function(this)
	end,
}

function ifs_fakeconsole_fnBuildScreen(this)
	fakeconsole_listbox_layout.fontheight = ScriptCB_GetFontHeight(fakeconsole_listbox_layout.font)
	fakeconsole_listbox_layout.yHeight = fakeconsole_listbox_layout.fontheight

	this.listbox = NewButtonWindow {
		ZPos = 200, x=0, y = 0,
		ScreenRelativeX = 0.5, -- center
		ScreenRelativeY = 0.48, -- middle of screen

		width = fakeconsole_listbox_layout.width + 35,
		height = fakeconsole_listbox_layout.showcount * (fakeconsole_listbox_layout.yHeight + fakeconsole_listbox_layout.ySpacing) + 30,
	}
	ListManager_fnInitList(this.listbox,fakeconsole_listbox_layout)
end

-- Set up listbox

ifs_fakeconsole_fnBuildScreen(ifs_fakeconsole)
ifs_fakeconsole_fnBuildScreen = nil

AddIFScreen(ifs_fakeconsole,"ifs_fakeconsole")
ifs_fakeconsole = DoPostDelete(ifs_fakeconsole)

