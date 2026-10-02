-- Console-style local splitscreen on PC, using the game's OWN console code
-- (2026-09-27).
--
-- The stock Lua is the same on PS4 and PC (T7LuaRepo ship dumps) and holds the
-- console behaviour; the PC build only switches it off:
--   * CoDMenu.lua CoD.Menu.HandleButtonPress: an unused controller's button becomes
--     "unused_gamepad_button" -> Lobby.lua -> LobbyAddLocalClient(menu, c) ->
--     Engine.SigninLocalClient(c)  (press A to join) - but only
--     `elseif not CoD.isPC then`; on PC the press is dropped (measured: controller
--     1's A reached UIRootFull and nothing happened).
--   * B on a non-primary controller -> LobbyRemoveLocalClientFromLobby(c)
--     (Lobby.lua), wired through CoDMenu's button-model subscriptions for
--     controllers 0..GetMaxLocalControllers()-1.
--   * Offline room: Lobby_SetMaxLocalPlayers(4) capped by GetMaxLocalControllers.
-- The component sets GetMaxControllerCount / GetMaxLocalControllers to 4 (the
-- controllers that have seats; PS4 4). This script takes the console branch of
-- CoD.Menu.HandleButtonPress on PC as well - the stock body, unchanged otherwise.
--
-- The PC-only button (LobbySplitscreenToggle, FE_ListAdditonal) only ever adds
-- or removes CONTROLLER 1. Its label (SplitscreenLobbyButtonPC) takes the first
-- matching state of Hide, MapController, Available (ACTIVATE: play available,
-- IsSplitscreenLobbyRoomAvailable(), any pad), Active (DEACTIVATE: more than one
-- player), AddController. Offline there is room for 4, so with 2 or 3 players
-- ACTIVATE always won and nobody could be removed. Now, with guests in the lobby,
-- there is "room" only while a connected pad has no seat; otherwise the label
-- reads DEACTIVATE. The click asks the same question as the label: ACTIVATE adds
-- that pad's controller through the stock join (alone: the stock toggle, which
-- also serves keyboard + one pad), DEACTIVATE removes every extra local player.
--
-- BOIII's LUI sandbox has no pcall: every global is checked for nil first.
-- The overrides are (re)installed from a timer, so load order does not matter.

if rawget(_G, "__zz_splitscreen_loaded") then
	return
end
rawset(_G, "__zz_splitscreen_loaded", true)

if Engine == nil or Dvar == nil or LUI == nil or LUI.roots == nil
	or LUI.UIElement == nil or LUI.UITimer == nil then
	return
end

local SUPPORTED = 4   -- local players the component can seat (seat records 0..3)

-- The log is for DEVELOPMENT ONLY. In a player's BOIII (started without
-- -unsafe-lua) io and os are NOT nil: every one of these functions is a stub
-- that pops BOIII's "Unsafe lua function called" warning
-- (ui_scripting.cpp patch_unsafe_lua_functions). So they must not even be
-- called unless the development-only zz_probe script, which is never shipped,
-- has loaded (it sets __zz_probe_loaded first and sorts before this file).
local LOG = nil
if rawget(_G, "__zz_probe_loaded")
	and io ~= nil and io.open ~= nil and os ~= nil and os.getenv ~= nil and os.getenv("LOCALAPPDATA") ~= nil then
	LOG = os.getenv("LOCALAPPDATA") .. "\\boiii\\splitscreen_lua.txt"
end
local function log(line)
	if LOG == nil then
		return
	end
	local f = io.open(LOG, "a")
	if f then
		f:write(line .. "\n")
		f:close()
	end
end

local wrapped_toggle = nil
local wrapped_add = nil
local wrapped_handle_press = nil
local wrapped_should_open = nil

-- Lobby.lua's PostLoadFunc runs `if CoD.isPC then f0_local0(menu) end`, and the
-- PC f0_local0 registers `unused_gamepad_button -> return true` on the Lobby
-- menu, replacing the console handler created a few lines earlier (Lobby.lua
-- line 1770). The PS4 f0_local0 has no such line. Put the stock console handler
-- back after the menu is built - its body, verbatim.
local function install_lobby_join_handler(menu, controller)
	if menu == nil or menu.registerEventHandler == nil or menu.__zz_console_join then
		return
	end
	menu.__zz_console_join = true
	log("Lobby: console join handler restored")
	menu:registerEventHandler("unused_gamepad_button", function(self, event)
		local handled = nil
		log("Lobby unused_gamepad_button c" .. tostring(event.controller) .. " -> LobbyAddLocalClient")
		LobbyAddLocalClient(self, event.controller or controller)
		if not handled then
			handled = self:dispatchEventToChildren(event)
		end
		return handled
	end)
end


-- The stock CoD.Menu.HandleButtonPress (CoDMenu.lua), with the console branch
-- taken on PC too. UI-editor menus such as Menu.Lobby swallow "gamepad_button"
-- (CoDMenu.lua registers `return true` on the instance) and read buttons through
-- the per-controller ButtonBits models instead (subscribed for controllers
-- 0..GetMaxLocalControllers()-1 on anyControllerAllowed menus); an unused
-- controller's press lands here and becomes "unused_gamepad_button" ->
-- Lobby.lua -> LobbyAddLocalClient(menu, c) - on console. Measured on PC
-- (2026-09-27): controller 1's A reached UIRootFull, Menu.Lobby (owner 0,
-- anyControllerAllowed, own gamepad_button handler) and nothing followed.
local function console_handle_button_press(menu, controller, button, model)
	if Engine.IsControllerBeingUsed(controller) or menu.unusedControllerAllowed then
		local list = menu:GetElementAndFunctionTableForButton(button, "buttonFunctions")
		for _, entry in ipairs(list) do
			if entry.fn(entry.element, menu, controller, model) then
				Engine.SetModelValue(model, 0)
				break
			end
		end
		if #list > 0 then
			Engine.SetModelValue(model, 0)
		end
	else
		-- Offline lobbies only (PLAY OFFLINE: network mode 0 LOCAL / 1 LAN). In the
		-- online menus (2 LIVE) an extra controller's press is dropped, as on stock
		-- PC. Measured 2026-09-29: a controller plugged in at the online main menu
		-- joined on its first A press, the game stalled ~25 s re-hosting the online
		-- party and showed a second player. Players 3/4 are offline-only anyway.
		if Engine.GetLobbyNetworkMode == nil or Engine.GetLobbyNetworkMode() == 2 then
			return
		end
		-- A joins, nothing else (measured 2026-10-01: the D-pad of a freshly plugged
		-- pad joined player 2). Without the enum the old behaviour stays.
		local a_button = Enum ~= nil and Enum.LUIButton ~= nil and Enum.LUIButton.LUI_KEY_XBA_PSCROSS or nil
		log("unused press c" .. tostring(controller) .. " button " .. tostring(button) .. " A=" .. tostring(a_button))
		if a_button ~= nil and button ~= a_button then
			return
		end
		if IsGameTypeDOA ~= nil and IsGameTypeDOA() and Engine.IsSplitscreen() then
			menu:setOwner(controller)
		end
		log("unused_gamepad_button c" .. tostring(controller) .. " on " .. tostring(menu.menuName))
		if menu.menuName == "Lobby" then
			-- Lazily, on the menu that is actually open: timers do not tick
			-- reliably in the frontend, so no load-time hook is relied upon.
			install_lobby_join_handler(menu, menu.m_ownerController)
		end
		menu:processEvent({
			name = "unused_gamepad_button",
			controller = controller
		})
	end
end

-- The stock LobbyAddLocalClient warns once the third controller is in
-- (actions.lua: GetUsedControllerCount() == 3 -> UI_ShowWarningMessageDialog
-- "MENU_RESTRICTED_TO_LOCAL_GAMES", "no networked games with 3 or more
-- controllers"). The dialog sets anyControllerAllowed, so player 4's first A
-- only closed it (measured 2026-09-29: controller 3's first press never
-- reached SigninLocalClient, the second joined). Offline, where 3-4 players
-- exist, the warning says nothing new: it is dropped where menus decide to
-- open a pending message (CoDMenu.lua -> ShouldOpenMessageDialog), and the
-- pending count is reset so the lobby's menu change does not reopen it.
-- (LuaUtils is a read-only table whose fields pairs() does not list, so the
-- warning call itself cannot be wrapped.)
local RESTRICTED_WARNING = "MENU_RESTRICTED_TO_LOCAL_GAMES"

local function drop_offline_restricted_warning()
	if Engine.GetLobbyNetworkMode == nil or Engine.GetLobbyNetworkMode() == 2 then
		return false
	end
	local dialog = Engine.GetModel(Engine.GetGlobalModel(), "messageDialog")
	if dialog == nil then
		return false
	end
	local pending = Engine.GetModel(dialog, "messagePending")
	local message = Engine.GetModel(dialog, "message")
	if pending == nil or message == nil or (Engine.GetModelValue(pending) or 0) <= 0
		or Engine.GetModelValue(message) ~= RESTRICTED_WARNING then
		return false
	end
	log("dropped " .. RESTRICTED_WARNING .. " (offline)")
	Engine.SetModelValue(pending, 0)
	return true
end

-- A controller with a connected pad and no seat. Engine.GamepadsConnectedIsActive
-- is the per-controller test of GetNonUsedControllerCount (PS4 0xD5BBA0: pad active,
-- not being used); the component widens both to four controllers.
local function free_pad_controller()
	if Engine.GamepadsConnectedIsActive == nil then
		return nil
	end
	for c = 1, SUPPORTED - 1 do
		if Engine.IsControllerBeingUsed(c) ~= true and Engine.GamepadsConnectedIsActive(c) == true then
			return c
		end
	end
	return nil
end

local function guests_seated()
	return Engine.GetUsedControllerCount() > 1
end

-- The stock "room" (CoD.LobbyBase.SplitscreenLobbyRoomAvailable; used <
-- lobby_maxLocalPlayers), narrowed while guests are in: then only a pad without a
-- seat makes room. Alone the stock answer stands.
local stock_room = nil
local wrapped_room = nil
local function stock_room_left()
	if stock_room ~= nil then
		return stock_room() == true
	end
	if Dvar.lobby_maxLocalPlayers == nil then
		return false
	end
	return Engine.GetUsedControllerCount() < Dvar.lobby_maxLocalPlayers:get()
end

local function room_for_another()
	if not stock_room_left() then
		return false
	end
	return not guests_seated() or free_pad_controller() ~= nil
end

-- The button's own decision (SplitscreenLobbyButtonPC "Available"), so the click
-- always does what the label says.
local function button_reads_activate()
	if IsSplitscreenPlayAvailable ~= nil and not IsSplitscreenPlayAvailable() then
		return false
	end
	if GamepadsConnectedAny ~= nil and not GamepadsConnectedAny() then
		return false
	end
	return room_for_another()
end

local function install()
	if CoD ~= nil and CoD.Menu ~= nil and CoD.Menu.HandleButtonPress ~= nil
		and CoD.Menu.HandleButtonPress ~= wrapped_handle_press then
		wrapped_handle_press = console_handle_button_press
		CoD.Menu.HandleButtonPress = wrapped_handle_press
		log("CoD.Menu.HandleButtonPress: console branch installed")
	end

	if IsSplitscreenLobbyRoomAvailable ~= nil and IsSplitscreenLobbyRoomAvailable ~= wrapped_room then
		stock_room = IsSplitscreenLobbyRoomAvailable
		wrapped_room = function()
			return room_for_another()
		end
		IsSplitscreenLobbyRoomAvailable = wrapped_room
		log("IsSplitscreenLobbyRoomAvailable: guests need a free pad")
	end

	if LobbySplitscreenToggle ~= nil and LobbySplitscreenToggle ~= wrapped_toggle then
		local stock_toggle = LobbySplitscreenToggle
		wrapped_toggle = function(menu, controller)
			if LuaUtils ~= nil and LuaUtils.LobbyProcessQueueEmpty ~= nil
				and not LuaUtils.LobbyProcessQueueEmpty() then
				return
			end
			if not button_reads_activate() then
				-- DEACTIVATE: every extra local player leaves, in the engine's own
				-- order (LobbyRemoveAllLocalSplitscreenClient walks 1..n).
				log("toggle: deactivate all, used=" .. tostring(Engine.GetUsedControllerCount()))
				if LobbyRemoveLocalClientFromLobby ~= nil then
					for c = 1, SUPPORTED - 1 do
						if Engine.IsControllerBeingUsed(c) == true then
							LobbyRemoveLocalClientFromLobby(menu, c)
						end
					end
				end
				return
			end
			if not guests_seated() then
				log("toggle: activate controller 1 (stock)")
				return stock_toggle(menu, controller)
			end
			-- ACTIVATE with guests already in: the controller of a pad without a seat
			-- joins through the stock join (the same call its own A press makes).
			local c = free_pad_controller()
			if c ~= nil and LobbyAddLocalClient ~= nil then
				log("toggle: activate controller " .. c .. " (stock join)")
				LobbyAddLocalClient(menu, c)
			end
		end
		LobbySplitscreenToggle = wrapped_toggle
	end

	if ShouldOpenMessageDialog ~= nil and ShouldOpenMessageDialog ~= wrapped_should_open then
		local stock_should_open = ShouldOpenMessageDialog
		wrapped_should_open = function(menu, controller)
			if drop_offline_restricted_warning() then
				return false
			end
			return stock_should_open(menu, controller)
		end
		ShouldOpenMessageDialog = wrapped_should_open
	end

	if LobbyAddLocalClient ~= nil and LobbyAddLocalClient ~= wrapped_add then
		local stock_add = LobbyAddLocalClient
		wrapped_add = function(menu, controller)
			local max = Dvar.lobby_maxLocalPlayers ~= nil and Dvar.lobby_maxLocalPlayers:get() or -1
			log("LobbyAddLocalClient c" .. tostring(controller) .. " used="
				.. tostring(Engine.GetUsedControllerCount()) .. " max=" .. tostring(max))
			return stock_add(menu, controller)
		end
		LobbyAddLocalClient = wrapped_add
	end
end

-- Update notice. The component asks GitHub once per start whether a newer release
-- exists and, if so, sets splitscreen_update_current and splitscreen_update_latest
-- (game thread, menus only). Shown once per game start with the game's own message
-- dialog (LuaUtils.ShowMessageDialog, the call BOIII uses for its own notices), in
-- the menus and while one player is in, so it never takes a joining player's press.
-- "Shown" is kept in the dvar: this script starts again after every match.
-- Engine.DvarString answers "" for a dvar that does not exist (PC 0x01FD52C0).
local UPDATE_SHOWN = "shown"
local update_shown = false

local function is_version(text)
	return type(text) == "string" and string ~= nil and string.match ~= nil
		and string.match(text, "^%d+%.%d+%.%d+$") ~= nil
end

local function update_notice()
	if update_shown or Engine.DvarString == nil or Engine.Exec == nil
		or Engine.IsInGame == nil or Engine.IsInGame() then
		return
	end
	if LuaUtils == nil or LuaUtils.ShowMessageDialog == nil then
		return
	end
	local latest = Engine.DvarString(nil, "splitscreen_update_latest")
	if not is_version(latest) then
		return
	end
	if Engine.GetUsedControllerCount() > 1 then
		return
	end
	if LuaUtils.LobbyProcessQueueEmpty ~= nil and not LuaUtils.LobbyProcessQueueEmpty() then
		return
	end
	local current = Engine.DvarString(nil, "splitscreen_update_current")
	if not is_version(current) then
		current = "?"
	end
	update_shown = true
	Engine.Exec(0, "set splitscreen_update_latest " .. UPDATE_SHOWN)
	log("update notice: " .. latest .. " (installed " .. current .. ")")
	-- The address is text: BOIII stubs OpenURL for Lua (unsafe-Lua warning).
	LuaUtils.ShowMessageDialog(0, 0,
		"Version " .. latest .. " is available (you have " .. current .. ").\n"
			.. "Download: nexusmods.com/callofdutyblackops3/mods/53\n(or the GitHub releases page)",
		"BO3 Local Splitscreen")
end

install()
log("loaded")

-- In the menus the game routes the whole UI to UIRootFull (UI_CoD_GetRootNameForController
-- answers "UIRootFull" while the byte at 0x03394BE8 is set: 1 in the frontend, measured
-- 2026-10-02), so a timer on UIRoot0 does not tick there. A watcher sits on both roots.
-- "splitscreen_update" is raised by the component (Live_RaiseLUIEvent, controller 0)
-- once the update check has an answer, so the notice does not depend on a timer.
local function attach_watcher(root, id)
	if root == nil then
		return
	end
	local watcher = LUI.UIElement.new()
	watcher.id = id
	watcher:registerEventHandler("zz_splitscreen_tick", function(self, event)
		install()
		update_notice()
		return true
	end)
	watcher:registerEventHandler("splitscreen_update", function(self, event)
		update_notice()
		return true
	end)
	root:addElement(watcher)
	watcher:addElement(LUI.UITimer.new(500, "zz_splitscreen_tick", false, watcher))
end

attach_watcher(LUI.roots.UIRoot0, "zz_splitscreen")
if LUI.roots.UIRootFull ~= nil and LUI.roots.UIRootFull ~= LUI.roots.UIRoot0 then
	attach_watcher(LUI.roots.UIRootFull, "zz_splitscreen_full")
end
