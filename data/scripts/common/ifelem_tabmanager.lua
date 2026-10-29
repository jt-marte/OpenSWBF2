--
-- Copyright (c) 2005 Pandemic Studios, LLC. All rights reserved.
--

-- General tab managing code for PC options interface screens.
-- Provides support code for all of the screens, and handles switching
-- between tabs (screens).

-- Tabs are implemented as a lightweight layer added to screens, with
-- elements added to all of them. Events in the tabs are handled by
-- this file, and when another tab is selected, it swaps screens in
-- the background.

function ifelem_tabmanager_DoCreateTabs( aLayout )
	local w,aw,h,ah
	w,h = ScriptCB_GetSafeScreenInfo()
	aw,ah = ScriptCB_GetScreenInfo()
	if( not aLayout ) then
		return nil
	end
	
	mTabs = NewIFContainer { --container for all the backgrounds
		ScreenRelativeX = 0.0,
		ScreenRelativeY = 0.0,
		ZPos = 60,
		UseSafezone = 0, 
	}

	local tabheight = 18
	if(ah ~= 600) then
		tabheight = tabheight * (ah / 600)
	end

	local i
	local NUM_TABS = table.getn(aLayout)
	local xWidth = 800 / NUM_TABS
	local deltaXPos = math.floor(xWidth) - 5
	
	local xPos = 10		--deltaXPos / 2 --(xWidth * 0.65) + 30
	local yPos = 15

-- Loadspam commented out NM 9/8/05. Re-enable locally if you need it
--	print( "w=", w, "aw=", aw, "NUM_TABS=", NUM_TABS, "xWidth=", xWidth, "xPos=", xPos )
	for i=1, NUM_TABS do
		local TagName = aLayout[i].tag
		local width = deltaXPos
		--xPos = deltaXPos + i * deltaXPos
 		if( aLayout[i].width ) then
			width = aLayout[i].width
		end 	
		
		if((aw ~= 800)) then
			local scaleX = (aw / 800)
			width = width * scaleX
		end
		
		xPos = xPos + (width/2) -- carryover from last time, if our xPos isn't set...
		
		local manualXPos = false
		if( aLayout[i].xPos ) then
			xPos = aLayout[i].xPos
			manualXPos = true
		end 		
		local manualYPos = false
 		if( aLayout[i].yPos ) then
			yPos = aLayout[i].yPos
			manualYPos = true
		end 	
		
		-- attempt at screen-relative everything - GAW, the things that people request...
		if((aw ~= 800) or (ah ~= 600)) then
			local scaleX = (aw / 800)
			local scaleY = (ah / 600)
			
			if(manualXPos) then
				xPos = xPos * scaleX
			end
			--if(manualYPos) then
			--	yPos = yPos * scaleY
			--end
		end
		
		local hotspotX = width / 2 - 4 -- yay for magic numbers!
		local hotspotY = 6 -- yay again!
 		if( aLayout[i].hotspot_x ) then
			hotspotX = aLayout[i].hotspot_x
		end 		
 		if( aLayout[i].hotspot_y ) then
			hotspotY = aLayout[i].hotspot_y
		end 		

		local hotspotW = width
		local hotspotH = tabheight
 		if( aLayout[i].hotspot_width ) then
			hotspotW = aLayout[i].hotspot_width
		end 		
 		if( aLayout[i].hotspot_height ) then
			hotspotH = aLayout[i].hotspot_height
		end

-- Loadspam commented out NM 9/8/05. Re-enable locally if you need it
--		print( "xPos", i, " = ", xPos, aLayout[i].tag )
		mTabs[TagName] = NewClickableIFButton {
			tag = TagName,
			font = aLayout.font or "gamefont_medium",
 			ZPos = 50,
 			x = xPos,
 			y = yPos,
			btnw = width,
			btnh = tabheight,
			string = aLayout[i].string,
			bg_width = width - 20,
			bStyleTabbed = 1,
			halign = "hcenter", 
			hotspot_x = hotspotX,
			hotspot_y = hotspotY,
			hotspot_width = width,
			hotspot_height = hotspotH,
			--bHidden = 1,
		}
		--mTabs[TagName].label.string = aLayout[i].string
		--mTabs[TagName].label.bgleft = "BF2_radiobutton_on"
		--mTabs[TagName].label.bgright = ""
		--mTabs[TagName].label.bgmid = "border_dropdown"
		--RoundIFButtonLabel_fnSetString(mTabs[TagName], aLayout[i].string)
--		IFObj_fnSetVis( mTabs[TagName], nil )
		xPos = xPos + (width/2)
	end
	
	return mTabs
end


-- Creates options tabs on the specified screen. This must be called
-- before AddIFScreen() is called. TODO: pass in layout, so this is
-- more general
function ifelem_tabmanager_Create(this, aLayout, aLayout1, aLayout2, aLayout3)
	this._Tabs = ifelem_tabmanager_DoCreateTabs( aLayout )
	if( aLayout1 ) then
		this._Tabs1 = ifelem_tabmanager_DoCreateTabs( aLayout1 )
	end
	if( aLayout2 ) then
		this._Tabs2 = ifelem_tabmanager_DoCreateTabs( aLayout2 )
	end
	if( aLayout3 ) then
		this._Tabs3 = ifelem_tabmanager_DoCreateTabs( aLayout3 )
	end

	-- OpenSWBF2: remember the layouts so a pad can cycle the tabs
	this._TabsLayout = aLayout
	this._Tabs1Layout = aLayout1
	this._Tabs2Layout = aLayout2
	this._Tabs3Layout = aLayout3
	-- Each tab knows its row by name (a table link would loop the mouse code's
	-- recursion), so the rows LB/RB/LT/RT cycle can keep the pad's cursor off.
	ifelem_tabmanager_MarkRow(this, "_Tabs", aLayout)
	ifelem_tabmanager_MarkRow(this, "_Tabs1", aLayout1)
	ifelem_tabmanager_MarkRow(this, "_Tabs2", aLayout2)
	ifelem_tabmanager_MarkRow(this, "_Tabs3", aLayout3)
	if( not this._bPadTabs ) then
		this._bPadTabs = 1
		ifelem_tabmanager_WrapPadInput(this, "Input_LTrigger", -1, "top")
		ifelem_tabmanager_WrapPadInput(this, "Input_RTrigger", 1, "top")
		ifelem_tabmanager_WrapPadInput(this, "Input_LTrigger2", -1, "lower")
		ifelem_tabmanager_WrapPadInput(this, "Input_RTrigger2", 1, "lower")
		PadSnap_WrapScreen(this)

		this._PadHint = ifelem_tabmanager_MakePadHint()
		local fnUpdate = this.Update
		this.Update = function(this, a, b)
			if( fnUpdate ) then
				fnUpdate(this, a, b)
			end
			ifelem_tabmanager_UpdatePadHint(this)
			PadSnap_Update(this)
		end
	end
end

-- OpenSWBF2: PC tabs are mouse-only buttons, so pads switch them like
-- console: LB/RB (Input_LTrigger/RTrigger) cycle the top row, LT/RT
-- (Input_LTrigger2/RTrigger2) the row nearest the content. The screen's own
-- handler (list paging, volume steps) still runs when no tab was switched.
function ifelem_tabmanager_WrapPadInput(this, FuncName, iDir, Which)
	local fnOriginal = this[FuncName]
	this[FuncName] = function(this, a, b)
		if( ifelem_tabmanager_PadCycle(this, iDir, Which) ) then
			return
		end
		if( fnOriginal ) then
			return fnOriginal(this, a, b)
		end
	end
end

-- Tab row a pad cycles: "top" is the first row; "lower" is the first visible
-- row under it. Deeper rows (Instant Action's Playlist/Global/Host... page
-- buttons, the controls sub-tabs) are in-page buttons, not screen tabs.
function ifelem_tabmanager_PadRow(this, Which)
	if( Which == "top" ) then
		if( this._Tabs and this._TabsLayout and IFObj_fnGetVis(this._Tabs) ) then
			return this._Tabs, this._TabsLayout
		end
		return nil
	end
	if( this._Tabs1 and this._Tabs1Layout and IFObj_fnGetVis(this._Tabs1) ) then
		return this._Tabs1, this._Tabs1Layout
	end
	if( this._Tabs2 and this._Tabs2Layout and IFObj_fnGetVis(this._Tabs2) ) then
		return this._Tabs2, this._Tabs2Layout
	end
	if( this._Tabs3 and this._Tabs3Layout and IFObj_fnGetVis(this._Tabs3) ) then
		return this._Tabs3, this._Tabs3Layout
	end
	return nil
end

function ifelem_tabmanager_MarkRow(this, RowKey, Layout)
	local Tabs = this[RowKey]
	if( (not Tabs) or (not Layout) ) then
		return
	end
	local i
	for i=1, table.getn(Layout) do
		local Tab = Tabs[Layout[i].tag]
		if( Tab ) then
			Tab._PadTabRowKey = RowKey
		end
	end
end

-- True for a tab in a row the bumpers/triggers cycle on the current screen.
-- Those are reached with LB/RB/LT/RT only, never a stick or the D-pad.
function ifelem_tabmanager_IsPadCycledTab(Tab)
	local this = gCurScreenTable
	if( (not this) or (not Tab._PadTabRowKey) ) then
		return nil
	end
	local Row = this[Tab._PadTabRowKey]
	if( (not Row) or (Row[Tab.tag] ~= Tab) ) then
		return nil
	end
	return (Row == ifelem_tabmanager_PadRow(this, "top")) or (Row == ifelem_tabmanager_PadRow(this, "lower"))
end

-- A row is worth a pad hint when it has a usable tab besides the selected one.
function ifelem_tabmanager_PadRowUsable(Tabs, Layout)
	if( not Tabs ) then
		return nil
	end
	local i
	for i=1, table.getn(Layout) do
		local Entry = Layout[i]
		local Tab = Tabs[Entry.tag]
		if( Tab and (not Tab.bSelected) and (not Entry.bNoPadCycle) and (Tab.bDimmed ~= 1) and IFObj_fnGetVis(Tab) ) then
			return 1
		end
	end
	return nil
end

-- Console-style button hints at the bottom centre, shown only while a pad
-- drives the menus: "[LB][RB] Menu" for the top row, "[LT][RT] Section" for
-- the row under it. Badges reuse the PC tab button art.
-- The pill end caps draw ~10 units past bg_width, so badges sit 40 apart and
-- the label starts after the second one.
function ifelem_tabmanager_MakePadBadge(xPos)
	return NewIFText {
		x = xPos, y = -5,
		textw = 26, texth = 10,
		halign = "hcenter", valign = "vcenter",
		font = "gamefont_tiny",
		bgleft = "headerbuttonleft", bgmid = "headerbuttonmid", bgright = "headerbuttonright",
		bg_width = 26,
		ColorR = 255, ColorG = 255, ColorB = 255,
	}
end

function ifelem_tabmanager_MakePadHintGroup(xPos)
	return NewIFContainer {
		x = xPos,
		left = ifelem_tabmanager_MakePadBadge(0),
		right = ifelem_tabmanager_MakePadBadge(40),
		label = NewIFText {
			x = 80, y = -5,
			textw = 100, texth = 10,
			halign = "left", valign = "vcenter",
			font = "gamefont_tiny",
			ColorR = gHelptextTextColor[1], ColorG = gHelptextTextColor[2], ColorB = gHelptextTextColor[3],
			nocreatebackground = 1,
		},
	}
end

-- In the black strip above the tabs, level with the PC title text
-- ("Profile Name: ..." / version, AddPCTitleText): the one spot that is free
-- on every tabbed screen.
function ifelem_tabmanager_MakePadHint()
	return NewIFContainer {
		ScreenRelativeX = 0.5,
		ScreenRelativeY = 0.0,
		y = -24, -- -27 clipped the pills' top edge at 1440p borderless
		ZPos = 40,
		top = ifelem_tabmanager_MakePadHintGroup(-180),
		lower = ifelem_tabmanager_MakePadHintGroup(20),
	}
end

-- Raw (unlocalized) strings must be pushed once the C++ objects exist.
function ifelem_tabmanager_SetPadHintStrings(Group, Left, Right, Label)
	IFText_fnSetUString(Group.left, ScriptCB_tounicode(Left))
	IFText_fnSetUString(Group.right, ScriptCB_tounicode(Right))
	IFText_fnSetUString(Group.label, ScriptCB_tounicode(Label))
end

function ifelem_tabmanager_UpdatePadHint(this)
	local Hint = this._PadHint
	if( not Hint ) then
		return
	end

	local bTop = nil
	local bLower = nil
	if( ScriptCB_IsPadNavigating and ScriptCB_IsPadNavigating() and (not ScriptCB_IsPopupOpen()) ) then
		bTop = ifelem_tabmanager_PadRowUsable(ifelem_tabmanager_PadRow(this, "top"))
		bLower = ifelem_tabmanager_PadRowUsable(ifelem_tabmanager_PadRow(this, "lower"))
	end

	if( Hint.bInited and (Hint.bTop == bTop) and (Hint.bLower == bLower) ) then
		return
	end
	if( not Hint.bInited ) then
		Hint.bInited = 1
		ifelem_tabmanager_SetPadHintStrings(Hint.top, "LB", "RB", "Menu")
		ifelem_tabmanager_SetPadHintStrings(Hint.lower, "LT", "RT", "Section")
	end
	Hint.bTop = bTop
	Hint.bLower = bLower
	IFObj_fnSetVis(Hint, bTop or bLower)
	IFObj_fnSetVis(Hint.top, bTop)
	IFObj_fnSetVis(Hint.lower, bLower)
end

-- Selects the next usable tab in iDir (wrapping) exactly as a click would:
-- the screen's Input_Accept runs ifelem_tabmanager_HandleInputAccept, so
-- callbacks and save-before-switch behave the same. Only while a pad drives
-- the menus, and for LB/RB only with no editbox focused, since keyboard 3/4
-- send the same events (nothing on the keyboard sends LT/RT). Returns nil
-- when nothing was switched.
function ifelem_tabmanager_PadCycle(this, iDir, Which)
	if( (not ScriptCB_IsPadNavigating) or (not ScriptCB_IsPadNavigating()) ) then
		return nil
	end
	if( gCurEditbox and (Which == "top") ) then
		return nil
	end

	local Tabs, Layout = ifelem_tabmanager_PadRow(this, Which)
	if( not Tabs ) then
		return nil
	end

	local NUM_TABS = table.getn(Layout)
	local iCur = 0
	local i
	for i=1, NUM_TABS do
		local Tab = Tabs[Layout[i].tag]
		if( Tab and Tab.bSelected ) then
			iCur = i
		end
	end

	local iNext = iCur
	local step
	for step=1, NUM_TABS do
		iNext = iNext + iDir
		if( iNext < 1 ) then
			iNext = NUM_TABS
		elseif( iNext > NUM_TABS ) then
			iNext = 1
		end
		if( iNext == iCur ) then
			return nil
		end
		local Entry = Layout[iNext]
		local Tab = Tabs[Entry.tag]
		if( Tab and (not Entry.bNoPadCycle) and (Tab.bDimmed ~= 1) and IFObj_fnGetVis(Tab) ) then
			-- forget what the (hidden) mouse last hovered, or Input_Accept
			-- would act on that instead of the tab
			gMouseListBox = nil
			gMouseListBoxSlider = nil
			gMouseOverHorz = nil
			if( gCurEditbox ) then
				IFEditbox_fnHilight(gCurEditbox, nil)
				gCurEditbox = nil
			end
			local PrevButton = this.CurButton
			this.CurButton = Entry.tag
			this:Input_Accept()
			-- don't leave the tab as this screen's button: coming back, the
			-- D-pad would start from a tag that isn't in this.buttons
			if( this.CurButton == Entry.tag ) then
				if( PrevButton and this.buttons and this.buttons[PrevButton] ) then
					this.CurButton = PrevButton
				else
					this.CurButton = nil
				end
			end
			return 1
		end
	end
	return nil
end


function ifelem_tabmanager_SelectTabGroup(this, bGroup, bGroup1, bGroup2, bGroup3 )
	IFObj_fnSetVis(this._Tabs, bGroup)
	if( this._Tabs1 ) then
		IFObj_fnSetVis(this._Tabs1, bGroup1)
	end
	if( this._Tabs2 ) then
		IFObj_fnSetVis(this._Tabs2, bGroup2)
	end
	if( this._Tabs3 ) then
		IFObj_fnSetVis(this._Tabs3, bGroup3)
	end
end


-- Sets the specified tab as selected. The rest of the tabs are
-- dimmed. The parameter in 'BrightTag' should be the tag parameter of
-- the selected one. TODO: pass in layout, so this is more general
function ifelem_tabmanager_SetSelected(this, aLayout, BrightTag, useTab2 )
	local i
	local NUM_TABS = table.getn(aLayout)
	
	local Tabs = this._Tabs
		
	if( useTab2 == 1 ) then
		Tabs = this._Tabs1
	elseif( useTab2 == 2 ) then
		Tabs = this._Tabs2
	elseif( useTab2 == 3 ) then
		Tabs = this._Tabs3
	end

	for i=1, NUM_TABS do
		local TagName = aLayout[i].tag
		local Alpha = 1 --0.6 -- no longer do we change the alpha of the tabs at all, ever
		local color_r = 150 --200
		local color_g = 150 --200
		local color_b = 150 --200
		
		-- please note that since the "highlighted" textures are darker, these are backwards -
		-- you use "highlighted" if you are unselected, and normal otherwise
		local texRight = "headerbuttonright_highlighted"
		local texMid = "headerbuttonmid_highlighted"
		local texLeft = "headerbuttonleft_highlighted"
		
		if(TagName == BrightTag) then
			--Alpha = 1
			color_r = 230
			color_g = 230
			color_b = 230
			texRight = "headerbuttonright"
			texMid = "headerbuttonmid"
			texLeft = "headerbuttonleft"
		end

		ScriptCB_IFFlashyText_SetBackground(Tabs[TagName].label.cp, texLeft, texMid, texRight)

		IFObj_fnSetAlpha(Tabs[TagName],Alpha)
		IFObj_fnSetAlpha(Tabs[TagName].label,Alpha)
		
		--IFObj_fnSetColor(Tabs[TagName], color_r, color_g, color_b)
		-- no longer do we change the color of the text at all, ever
		IFObj_fnSetColor(Tabs[TagName].label, color_r, color_g, color_b)
		
		Tabs[TagName].bSelected = (TagName == BrightTag)
		
		-- prevent the button from ending up falsely selected since we switched screens
		IFButton_fnSelect(Tabs[TagName], false, false)
	end
end


-- To be called from a screen's Input_Accept. If the user has
-- clicked on a tab, it handles the switching to that screen.
--
-- Note: returns true(1) if the user did click on a tab, nil
-- otherwise. This allows the calling screen to exit immediately from
-- their Input_Accept handler.
--
-- Also, if bDelayedSwitch is true, then this code doesn't force a
-- switch, but instead, returns the name of the screen to switch
-- to. This is non-nil, allowing screens to do their own processing.
-- This is so the options screens can force a save before going to the
-- next screen.
--
function ifelem_tabmanager_HandleInputAccept(this, aLayout, useTab, bDelayedSwitch)
	local bHit = nil
	local i
	local NUM_TABS = table.getn(aLayout)

	local Tabs = this._Tabs
	if(useTab == 1) then
		Tabs = this._Tabs1
	elseif(useTab == 2) then
		Tabs = this._Tabs2
	elseif(useTab == 3) then
		Tabs = this._Tabs3
	end

--	print("CurButton = ", gCurScreenTable.CurButton)

	for i=1, NUM_TABS do
		local TagName = aLayout[i].tag
		if(TagName == gCurScreenTable.CurButton) then
			if(not Tabs[TagName].bSelected) then
				-- callback is used when you just need to do something, without going to
				-- another screen at the end
				if(aLayout[i].callback) then
					aLayout[i].callback(this, aLayout[i].screen)
					return -1 -- callback handled this
				else
					-- callback2 is used when you just need to do something before going
					-- to that screen
					if(aLayout[i].callback2) then
						aLayout[i].callback2(this, aLayout[i].screen)
					end

					ifelm_shellscreen_fnPlaySound(this.acceptSound)
					if(bDelayedSwitch) then
						return aLayout[i].screen
					else
						ScriptCB_SetIFScreen(aLayout[i].screen)
					end
				end
			end -- Current tab matched, isn't currently selected
			return aLayout[i].screen
		end -- tab 
	end -- loop over tabs

	return nil -- didn't hit any tab buttons.
end

-- Sets the specified tab as unactived
function ifelem_tabmanager_SetDimmed(this, aLayout, DimmedTag, DimmedFlag, useTab )
	local i
	local NUM_TABS = table.getn(aLayout)
	
	local Tabs = this._Tabs
	if(useTab == 1) then
		Tabs = this._Tabs1
	elseif(useTab == 2) then
		Tabs = this._Tabs2
	elseif(useTab == 3) then
		Tabs = this._Tabs3
	end

	for i=1, NUM_TABS do
		local TagName = aLayout[i].tag
		if(TagName == DimmedTag) then
			Tabs[TagName].bDimmed = DimmedFlag
			local Alpha = 1
			if( DimmedFlag == 1 ) then
				Alpha = 0.2
			end
			IFObj_fnSetAlpha(Tabs[TagName],Alpha)			
			IFObj_fnSetAlpha(Tabs[TagName].label,Alpha)			
		end		
	end
end

-- Sets the specified tab as unactived
function ifelem_tabmanager_SetVisable(this, aLayout, VisTag, VisFlag, useTab )
	local i
	local NUM_TABS = table.getn(aLayout)
	
	local Tabs = this._Tabs
	if(useTab == 1) then
		Tabs = this._Tabs1
	elseif(useTab == 2) then
		Tabs = this._Tabs2
	elseif(useTab == 3) then
		Tabs = this._Tabs3
	end

	for i=1, NUM_TABS do
		local TagName = aLayout[i].tag
		if(TagName == VisTag) then
			IFObj_fnSetVis(Tabs[TagName],VisFlag)
		end		
	end
end

-- Sets the specified tab as unactived
function ifelem_tabmanager_SetPos(this, aLayout, Tag, useTab, x, y )
	local i
	local NUM_TABS = table.getn(aLayout)
	
	local Tabs = this._Tabs
	if(useTab == 1) then
		Tabs = this._Tabs1
	elseif(useTab == 2) then
		Tabs = this._Tabs2
	elseif(useTab == 3) then
		Tabs = this._Tabs3
	end

	for i=1, NUM_TABS do
		local TagName = aLayout[i].tag
		if(TagName == Tag) then
			local x_pos = x or Tabs[TagName].x
			local y_pos = y or Tabs[TagName].y			
			IFObj_fnSetPos( Tabs[TagName], x_pos, y_pos )
		end		
	end
end

local MAX_RADIO_BUTTON_GROUPS = 50

---- radio button stuff	below, mostly copied from above	:^)	----

local RadioButtonTextureOn = "BF2_radiobutton_on"
local RadioButtonTextureOff = "BF2_radiobutton_off"

-- are we mouseover any of them?
function ifelem_GetRadioButtonMouseOverTag(this)

	if(this.radiobuttons ==	nil) then --well obviously we didn't hit one
		return nil
	end
			
	for	row=1, MAX_RADIO_BUTTON_GROUPS do
		if((this.radiobuttons[row] ~= nil) and (this.radiobuttons[row].bInitialized ~= nil)) then -- it exists
			local tag = ifelem_CheckRadioButtonRowMouseOver(this.radiobuttons[row])
			if(tag) then
				return tag
			end
		end
	end
	
	for key, group in pairs(this.radiobuttons) do
		if ( type(group) == "table" ) then
			if(group.bInitialized == true) then
				local tag = ifelem_CheckRadioButtonRowMouseOver(group)
				if(tag) then
					return tag, group
				end
			end
		end
	end

	return nil -- didn't hit any radio buttons
end

function ifelem_CheckRadioButtonRowMouseOver(this)
	if((not gCurScreenTable) or (not gCurScreenTable.CurButton)) then
		return nil
	end
	
	local NUM_TABS = table.getn(this)

	for	i=1, NUM_TABS do
		local TagName = this[i].radiobutton.tag
		if(TagName == gCurScreenTable.CurButton) then
			return TagName
		end
	end
	return nil
end


-- calls callbacks associated with radio buttons
function ifelem_HandleRadioButtonInputAccept(this)

	if(this.radiobuttons ==	nil) then --well obviously we didn't hit one
		return nil
	end
			
	for	row=1, MAX_RADIO_BUTTON_GROUPS do
		if((this.radiobuttons[row] ~= nil) and (this.radiobuttons[row].bInitialized ~= nil)) then -- it exists
			if(ifelem_CheckRadioButtonRowInputAccept(this.radiobuttons[row])) then
				ifelm_shellscreen_fnPlaySound(this.acceptSound)
				return -1  -- handled
			end
		end
	end
	
	for key, group in pairs(this.radiobuttons) do
		if ( type(group) == "table" ) then
			if(group.bInitialized == true) then
				if(ifelem_CheckRadioButtonRowInputAccept(group)) then
					ifelm_shellscreen_fnPlaySound(this.acceptSound)
					return -1  -- handled
				end
			end
		end
	end

	return nil -- didn't hit any radio buttons
end

function ifelem_CheckRadioButtonRowInputAccept(this)
	if((not gCurScreenTable) or (not gCurScreenTable.CurButton)) then
		return nil
	end
	
	local NUM_TABS = table.getn(this)

	for	i=1, NUM_TABS do
		local TagName = this[i].radiobutton.tag
		if(TagName == gCurScreenTable.CurButton) then
			ifelem_SelectRadioButton(this, i)
			return true
		end
	end
	return nil
end

-- gives back the number of the button that is selected, or 0 if none
function ifelem_GetSelectedRadioButton(this)
	-- quick search (TM)
	local NUM_TABS = table.getn(this)

	for	i=1, NUM_TABS do
		local btn = this[i].radiobutton
		if(btn.bSelected) then
			return i
		end		
	end
	return 0
end

-- callback may return non-nil to indicate that we really should select a different button!
function ifelem_SelectRadioButton(this, btnNum, bSkipCallback)
	-- if already selected, do nothing
	if(this[btnNum].radiobutton.bSelected) then
		return
	end 
	
	-- it's time to do work

	local selectThisOne = btnNum

	-- the callback
	if((bSkipCallback == nil) and (this.callback)) then
		local retVal = this.callback(this, btnNum)
		if(retVal ~= nil) then -- the callback can return a button to be selected
			selectThisOne = retVal
		end
	end

	
	-- now change the art
	local NUM_TABS = table.getn(this)

	for	i=1, NUM_TABS do
		local btn = this[i].radiobutton
		
		if(i == selectThisOne) then
			btn.bSelected = true
			IFImage_fnSetTexture(btn,RadioButtonTextureOn)
		else
			btn.bSelected = nil
			IFImage_fnSetTexture(btn,RadioButtonTextureOff)
		end
	end
end

-- doesn't really have to be globally unique, but doesn't hurt anything
local uniqueRadioButtonTagID = 1

function ifelem_CreateRadioButtonGroup(	aLayout, screenX, screenY )
			local w,aw,h,ah
			w,h	= ScriptCB_GetSafeScreenInfo()
			aw,ah =	ScriptCB_GetScreenInfo()
			if(	(not aLayout) or (not aLayout.strings)) then
				return nil
			end
			
			local myFont = aLayout.font or "gamefont_tiny"
			local spacing = aLayout.spacing or 150
			
			mTabs =	NewIFContainer { --container for all the buttons
						x =	screenX	or 0,
						y =	screenY	or 0,
						--ScreenRelativeX	= 0.0,
						--ScreenRelativeY	= 0.0,
						ZPos = 0,
						--UseSafezone	= 0, 
			}

			local i
			local NUM_TABS = table.getn(aLayout.strings) 

			--print( "w=", w, "aw=", aw, "NUM_TABS=", NUM_TABS,	"xWidth=", xWidth, "xPos=",	xPos )
			local xPos = 0
			for	i=1, NUM_TABS do
				
				local useTexture = RadioButtonTextureOn
				local selected = true
				
				if(i > 1) then
					useTexture = RadioButtonTextureOff
					selected = nil		
				end
				
				local myTag = "radio"..uniqueRadioButtonTagID
				uniqueRadioButtonTagID = uniqueRadioButtonTagID + 1
								
				local buttonSize = 14
				
				mTabs[i] = { --NewIFContainer {	--container	for	all	the	buttons
						
				radiobutton	= NewIFImage {
						ZPos = 0, 
						x =	xPos, y = 0,
						texture	= useTexture,
						localpos_l = 0, localpos_t = -buttonSize*0.5,
						localpos_r = buttonSize, localpos_b	= buttonSize*0.5,
						AutoHotspot	= 1, tag = myTag,
						bIsFlashObj	= 1, flash_alpha = 1.0,
						bSelected = selected
					},
				radiotext =	NewIFText {	
						x =	xPos + buttonSize + 5,
						y =	-buttonSize + 2,
						halign = "left", valign	= "vcenter",
						font = myFont, 
						textw =	spacing - buttonSize - 5,
						textbreak = "none",
						nocreatebackground=1,
						string = aLayout.strings[i],
						--tag	= "text",
					},
				}
				xPos = xPos + spacing + buttonSize + 5
			end

			mTabs.callback = aLayout.callback
			mTabs.bInitialized = true
			return mTabs
end

-- Creates a new group of radio	buttons	on the specified screen
-- returns the group which can be passed to other radio button functions
function ifelem_AddRadioButtonGroup(this, screenX, screenY,	aLayout, title)

	if(not aLayout) then
		return nil
	end

	if(this.radiobuttons ==	nil) then -- set them up
		this.radiobuttons =	NewIFContainer
		{
			--ScreenRelativeX	= 0.0,
			--ScreenRelativeY	= 0.0,
			--ZPos = 0,
			--UseSafezone	= 0, 
		}
		for	num=1, MAX_RADIO_BUTTON_GROUPS do
			this.radiobuttons[num] = {} -- just so it actually exists or	else Lua complains below when we index it
		end
	end
	
	if ( title ) then
		if(not this.radiobuttons[title] or this.radiobuttons[title].bInitialized == nil) then
			this.radiobuttons[title] = ifelem_CreateRadioButtonGroup(aLayout, screenX, screenY)
			return this.radiobuttons[title]
		end
	else
		for	i=1, MAX_RADIO_BUTTON_GROUPS do
			if(this.radiobuttons[i].bInitialized == nil) then
				this.radiobuttons[i] = ifelem_CreateRadioButtonGroup(aLayout, screenX, screenY)
				return this.radiobuttons[i]
			end
		end
	end
	
	-- should never get here
	return nil
end


----------------------------------------------------------------------------
-- OpenSWBF2: pad "snap" navigation for the mouse-only PC screens (options
-- forms, the multiplayer pages, key bindings). They have no console button
-- links, so the stick/D-pad hop between the visible clickable widgets and
-- hover each one exactly as the mouse would; A then clicks it through the
-- screen's own Input_Accept. Opt-in per screen (bPadSnap = 1), so screens
-- with working console navigation keep it. The tab rows LB/RB/LT/RT cycle
-- are never targets.

function PadSnap_WrapScreen(this)
	PadSnap_WrapDir(this, "Input_GeneralUp", 0, -1)
	PadSnap_WrapDir(this, "Input_GeneralDown", 0, 1)
	PadSnap_WrapDir(this, "Input_GeneralLeft", -1, 0)
	PadSnap_WrapDir(this, "Input_GeneralRight", 1, 0)
end

function PadSnap_WrapDir(this, FuncName, iDX, iDY)
	local fnOriginal = this[FuncName]
	this[FuncName] = function(this, a, b)
		if( PadSnap_Active(this) and PadSnap_Move(this, iDX, iDY) ) then
			return
		end
		if( fnOriginal ) then
			return fnOriginal(this, a, b)
		end
	end
end

function PadSnap_Active(this)
	if( (not ScriptCB_IsPadNavigating) or (not ScriptCB_IsPadNavigating()) or gCurPopup ) then
		return nil
	end
	return this.bPadSnap
end

function PadSnap_Center(Obj)
	if( Obj and Obj.cp and ScriptCB_IFObj_GetHotSpotCenter ) then
		return ScriptCB_IFObj_GetHotSpotCenter(Obj.cp)
	end
	return nil
end

function PadSnap_Add(List, Obj, bModal)
	local fX, fY = PadSnap_Center(Obj)
	if( fX ) then
		table.insert(List, { x = fX, y = fY, bModal = bModal })
	end
end

-- Same targets the mouse code hit-tests (HandleMouseRecurse): buttons, tagged
-- images (radio choices, arrows), sliders, editboxes and list rows.
function PadSnap_Collect(this, List, Seen, iDepth)
	if( Seen[this] or (iDepth > 16) ) then
		return
	end
	Seen[this] = 1
	if( this.bHidden or (this.hidden == 1) or (this.alpha == 0) ) then
		return
	end
	if( this.cp and (not IFObj_fnGetVis(this)) ) then
		return
	end

	local Type = this.type
	if( Type == "button" ) then
		if( this.label and (not this.bDimmed) and
			  not (this._PadTabRowKey and ifelem_tabmanager_IsPadCycledTab(this)) ) then
			PadSnap_Add(List, this.label)
		end
		return
	elseif( (Type == "image") or (Type == "maskimage") ) then
		if( this.tag ) then
			PadSnap_Add(List, this)
		end
		return
	elseif( Type == "hslider" ) then
		PadSnap_Add(List, this.sliderbg)
		return
	elseif( Type == "editbox" ) then
		PadSnap_Add(List, this)
		return
	elseif( Type == "listbox" ) then
		if( this.Contents and this.Layout ) then
			local Count = table.getn(this.Contents)
			local First = this.Layout.FirstShownIdx or 1
			local i
			for i = 1, (this.Layout.showcount or 0) do
				if( this[i] and ((First + i - 1) <= Count) ) then
					PadSnap_Add(List, this[i], this.bPadModal)
				end
			end
		end
		return
	elseif( (Type == "text") or (Type == "model") ) then
		return
	end

	local k, v
	for k, v in pairs(this) do
		if( (type(v) == "table") and (k ~= "Layout") and (k ~= "Contents") and
			  (k ~= "EntryScreenTable") and (k ~= "calledFrom") and (k ~= "_PadHint") ) then
			PadSnap_Collect(v, List, Seen, iDepth + 1)
		end
	end
end

-- Nearest target in the pushed direction (within ~70 degrees), favouring
-- ones in line. No position yet: the top-left one. No direction: the one
-- closest to where we were.
function PadSnap_Pick(List, fX, fY, iDX, iDY)
	local Best = nil
	local fBest = nil
	local i
	for i = 1, table.getn(List) do
		local Item = List[i]
		local fScore = nil
		if( not fX ) then
			fScore = Item.y * 4 + Item.x
		else
			local fOX = Item.x - fX
			local fOY = Item.y - fY
			if( (iDX == 0) and (iDY == 0) ) then
				fScore = fOX * fOX + fOY * fOY
			else
				local fFwd = fOX * iDX + fOY * iDY
				local fSide = math.abs(fOX * iDY) + math.abs(fOY * iDX)
				if( (fFwd > 4) and (fSide <= fFwd * 3) ) then
					fScore = fFwd + fSide * 2
				end
			end
		end
		if( fScore and ((not fBest) or (fScore < fBest)) ) then
			Best = Item
			fBest = fScore
		end
	end
	return Best
end

-- Returns nil only when the screen has nothing to land on.
function PadSnap_Move(this, iDX, iDY)
	-- in a list, up/down walk (and scroll) its rows before leaving it
	if( (iDY ~= 0) and gMouseListBox and PadSnap_ListStep(this, gMouseListBox, iDY) ) then
		return 1
	end
	-- left/right change a hovered slider; past either end they move on, so
	-- two-column pages (video, controls) can be crossed
	if( (iDX ~= 0) and PadSnap_StepSlider(this, iDX) ) then
		return 1
	end

	local List = PadSnap_Targets(this)
	if( table.getn(List) == 0 ) then
		return nil
	end
	local Best = PadSnap_Pick(List, this.fPadSnapX, this.fPadSnapY, iDX, iDY)
	if( Best ) then
		this.fPadSnapX = Best.x
		this.fPadSnapY = Best.y
		PadSnap_Hover(this, Best.x, Best.y)
	end
	return 1
end

-- An open dropdown list (bPadModal) sits over the page; while it's up, its
-- rows are the only targets, like a popup.
function PadSnap_Targets(this)
	local List = {}
	PadSnap_Collect(this, List, {}, 0)
	local Modal = {}
	local i
	for i = 1, table.getn(List) do
		if( List[i].bModal ) then
			table.insert(Modal, List[i])
		end
	end
	if( table.getn(Modal) > 0 ) then
		return Modal
	end
	return List
end

function PadSnap_Hover(this, fX, fY)
	if( this.HandleMouse ) then
		this.HandleMouse(this, fX, fY)
	else
		gHandleMouse(this, fX, fY)
	end
	-- a slider hover records where it was "clicked"; keep its value, so A
	-- doesn't jump it to the middle
	if( gMouseOverHorz and gMouseOverHorz.thumbposn ) then
		gMouseOverHorz.fHitX = gMouseOverHorz.thumbposn
	end
end

function PadSnap_ListStep(this, Listbox, iDY)
	local Layout = Listbox.Layout
	if( (not Layout) or (not Listbox.Contents) or (not Layout.CursorIdx) ) then
		return nil
	end
	local iNew = Layout.CursorIdx + iDY
	if( (iNew < 1) or (iNew > table.getn(Listbox.Contents)) ) then
		return nil -- off the end: leave the list
	end
	Layout.CursorIdx = iNew
	local iFirst = Layout.FirstShownIdx or 1
	local iShow = Layout.showcount or 1
	if( iNew < iFirst ) then
		Layout.FirstShownIdx = iNew
	elseif( iNew > iFirst + iShow - 1 ) then
		Layout.FirstShownIdx = iNew - iShow + 1
	end
	ListManager_fnFillContents(Listbox, Listbox.Contents, Layout)
	ScriptCB_SndPlaySound("shell_select_change")
	if( gCurScreenTable.UpdateUI ) then
		gCurScreenTable.UpdateUI(gCurScreenTable)
	end
	local fX, fY = PadSnap_Center(Listbox[iNew - (Layout.FirstShownIdx or 1) + 1])
	if( fX ) then
		this.fPadSnapX = fX
		this.fPadSnapY = fY
	end
	return 1
end

-- Like a click a twentieth of the way along; the screen's Input_Accept
-- applies fHitX as it does for the mouse. Nil at the end of the range.
function PadSnap_StepSlider(this, iDX)
	local Slider = gMouseOverHorz
	if( (not Slider) or (not Slider.thumbposn) ) then
		return nil
	end
	if( ((iDX < 0) and (Slider.thumbposn <= 0.001)) or ((iDX > 0) and (Slider.thumbposn >= 0.999)) ) then
		return nil
	end
	Slider.fHitX = math.max(0, math.min(1, Slider.thumbposn + 0.05 * iDX))
	this:Input_Accept()
	ScriptCB_SndPlaySound("shell_select_change")
	if( Slider.thumbposn ) then
		Slider.fHitX = Slider.thumbposn
	end
	return 1
end

-- Called every frame: once a pad drives the menus, land on something (the
-- spot we were at last time, or the top-left widget) so it's never blank.
function PadSnap_Update(this)
	if( (gPadSnapPrimed == this) or (not PadSnap_Active(this)) ) then
		return
	end
	if( PadSnap_Move(this, 0, 0) ) then
		gPadSnapPrimed = this
	end
end
