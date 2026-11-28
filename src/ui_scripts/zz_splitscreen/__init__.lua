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
-- Guest A is owned through physical release, using independent subscriptions
-- even when stock/ezz captured a smaller controller bound during construction.
-- The PC-only activate/deactivate button is hidden and disabled; the parent
-- FE_ListAdditonal selects its stock StringA play-hint clip while a pad can join.
-- (Hide/MapController belong to the child PC button, not that parent.)
--
-- BOIII's LUI sandbox has no pcall: every global is checked for nil first.
-- The overrides are (re)installed from a timer, so load order does not matter.

if rawget(_G, "__zz_splitscreen_loaded") then
	return
end

if Engine == nil or Dvar == nil or LUI == nil or LUI.roots == nil
	or LUI.UIElement == nil or LUI.UITimer == nil then
	return
end
rawset(_G, "__zz_splitscreen_loaded", true)

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
local wrapped_remove = nil
local stock_remove = nil
local wrapped_create_lobby = nil
local toggle_widgets_wrapped = setmetatable({}, {__mode = "k"})
local prompt_widgets = setmetatable({}, {__mode = "k"})
local guest_a = {} -- One physical ButtonBits gesture, shared by every lobby/root.

local function offline_frontend(menu)
	local network = Engine.GetLobbyNetworkMode ~= nil and Engine.GetLobbyNetworkMode() or nil
	return menu ~= nil and menu.menuName == "Lobby"
		and (menu.getParent == nil or menu:getParent() ~= nil)
		and (network == 0 or network == 1)
		and (Engine.IsInGame == nil or not Engine.IsInGame())
		and (Engine.GetCurrentMap == nil or Engine.GetCurrentMap() == "core_frontend")
end

local function lobby_accepts_input(menu)
	return offline_frontend(menu) and not menu.occludedBy
		and not menu.m_inputDisabled and not menu.m_disableAllButtonActions
end

-- A button-model update may reach more than one menu before the asynchronous
-- lobby queue publishes the new seat. Reserve that controller (and known pad
-- port) before calling stock code, with a bounded retry for failed joins.
local join_pending = {}
local join_epoch = 0
local JOIN_RETRY_TICKS = 6   -- watcher ticks are 500 ms; no sandbox clock needed
local JOIN_MAX_ATTEMPTS = 3 -- includes the initial stock call that acquires files
local leave_pending = {}
local join_call_in_progress = false
local leave_call_in_progress = false

local function lobby_queue_ready()
	return LuaUtils == nil or LuaUtils.LobbyProcessQueueEmpty == nil
		or LuaUtils.LobbyProcessQueueEmpty()
end

local function process_pending_leaves()
	for c = 1, SUPPORTED - 1 do
		local request = leave_pending[c]
		if request ~= nil then
			if Engine.IsControllerBeingUsed(c) ~= true then
				leave_pending[c] = nil
				join_pending[c] = nil
			elseif request.expires <= join_epoch or (Engine.IsInGame ~= nil and Engine.IsInGame())
				or Engine.GetLobbyNetworkMode == nil or Engine.GetLobbyNetworkMode() == 2 then
				leave_pending[c] = nil
			elseif request.last_attempt ~= join_epoch and lobby_queue_ready() and stock_remove ~= nil then
				-- Publish before the call: model callbacks may re-enter this handler.
				-- A refused leave must be retried, not mistaken for a completed one.
				request.last_attempt = join_epoch
				leave_call_in_progress = true
				stock_remove(request.menu, c)
				leave_call_in_progress = false
			end
		end
	end
end

local function request_guest_leave(menu, controller)
	if leave_pending[controller] == nil then
		join_pending[controller] = nil
		leave_pending[controller] = {menu = menu, expires = join_epoch + JOIN_RETRY_TICKS}
	end
	process_pending_leaves()
end

local function known_pad_port(controller)
	if Engine.GamepadsConnectedPort == nil or Engine.GamepadsConnectedValidPort == nil then
		return nil
	end
	local port = Engine.GamepadsConnectedPort(controller)
	if port ~= nil and Engine.GamepadsConnectedValidPort(port) == true then
		return port
	end
	return nil
end

local function lobby_nav_model()
	if Engine.GetGlobalModel ~= nil and Engine.GetModel ~= nil then
		return Engine.GetModel(Engine.GetGlobalModel(), "lobbyRoot.lobbyNav")
	end
	return nil
end

local function same_join_context(request, controller)
	return offline_frontend(request.menu)
		and Engine.GetLobbyNetworkMode() == request.network
		and (request.port == nil or known_pad_port(controller) == request.port)
		and (request.nav_model == nil or (lobby_nav_model() == request.nav_model
			and Engine.GetModelValue(request.nav_model) == request.nav))
end

local function reserve_join(menu, controller, stock_add)
	if type(controller) ~= "number" or controller < 0 or controller >= SUPPORTED
		or controller % 1 ~= 0 or Engine.IsControllerBeingUsed(controller) == true then
		return false
	end
	if leave_pending[controller] ~= nil then
		if Engine.IsControllerBeingUsed(controller) ~= true then
			leave_pending[controller] = nil
			join_pending[controller] = nil
		else
			return false
		end
	end
	if not offline_frontend(menu) then
		return false
	end
	local max = Dvar.lobby_maxLocalPlayers ~= nil and Dvar.lobby_maxLocalPlayers:get() or 2
	if Engine.GetUsedControllerCount() >= max then
		return false
	end
	local port = known_pad_port(controller)
	for c = 0, SUPPORTED - 1 do
		local pending = join_pending[c]
		if pending ~= nil and pending.expires <= join_epoch then
			join_pending[c] = nil
			pending = nil
		end
		if (c == controller and pending ~= nil)
			or (c ~= controller and port ~= nil
				and ((pending ~= nil and pending.port == port)
					or (Engine.IsControllerBeingUsed(c) == true and known_pad_port(c) == port))) then
			return false
		end
	end
	-- Never replace this menu/callback on duplicate presses or late wrappers.
	local nav_model = lobby_nav_model()
	local request = {port = port, menu = menu, stock_add = stock_add,
		network = Engine.GetLobbyNetworkMode(), nav_model = nav_model,
		nav = nav_model ~= nil and Engine.GetModelValue(nav_model) or nil,
		expires = join_epoch + JOIN_RETRY_TICKS, attempts = 0}
	join_pending[controller] = request
	return request
end

-- Installed ezz splitscreen_join arms on DOWN, then toggles on release. Never
-- clear DOWN here: that re-enters its subscription before it can arm, and joining
-- on DOWN lets later stock subscribers treat the very same press as seated A.
-- Own all three guest models independently of the bound captured by stock/ezz.
local function install_lobby_a_handlers(menu)
	if menu == nil or menu.menuName ~= "Lobby" or menu.subscribeToModel == nil
		or Engine.GetModelForController == nil or Engine.GetModel == nil
		or Enum == nil or Enum.LUIButton == nil or Enum.LUIButtonFlags == nil
		or CoD == nil or CoD.BitUtility == nil or CoD.BitUtility.IsBitwiseAndNonZero == nil then return end
	local button, flag = Enum.LUIButton.LUI_KEY_XBA_PSCROSS, Enum.LUIButtonFlags.FLAG_DOWN
	if button == nil or flag == nil then return end
	menu.__zz_guest_join_models = menu.__zz_guest_join_models or {}
	for c = 1, SUPPORTED - 1 do
		local guest = c
		local parent = Engine.GetModelForController(guest)
		local model = parent ~= nil and Engine.GetModel(parent, "ButtonBits." .. button) or nil
		if model == nil and parent ~= nil and Engine.CreateModel ~= nil then
			model = Engine.CreateModel(parent, "ButtonBits." .. button)
		end
		if model ~= nil and menu.__zz_guest_join_models[guest] ~= model then
			menu.__zz_guest_join_models[guest] = model
			menu:subscribeToModel(model, function(value_model)
				local gesture = guest_a[guest]
				local down = CoD.BitUtility.IsBitwiseAndNonZero(Engine.GetModelValue(value_model) or 0, flag)
				if down then
					if gesture == nil and lobby_accepts_input(menu)
						and (Engine.GetPrimaryController == nil or guest ~= Engine.GetPrimaryController())
						and Engine.IsControllerBeingUsed(guest) ~= true then
						local nav_model = lobby_nav_model()
						guest_a[guest] = {menu = menu, model = value_model,
							network = Engine.GetLobbyNetworkMode(), port = known_pad_port(guest),
							nav_model = nav_model, nav = nav_model ~= nil and Engine.GetModelValue(nav_model) or nil}
					end
				elseif gesture ~= nil and gesture.menu == menu and gesture.model == value_model then
					-- Retire BEFORE stock/model callbacks can re-enter; no synthetic reset.
					guest_a[guest] = nil
					if same_join_context(gesture, guest) and lobby_accepts_input(menu) and LobbyAddLocalClient ~= nil then
						LobbyAddLocalClient(menu, guest)
					end
				end
			end, false)
		end
	end
end

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
		log("Lobby unused_gamepad_button c" .. tostring(event.controller) .. " -> LobbyAddLocalClient")
		if LobbyAddLocalClient ~= nil then
			LobbyAddLocalClient(self, event.controller or controller)
		end
		-- This is a join request, not another navigation event. Do not send the
		-- same press down to a child that may enqueue a second join/toggle.
		return true
	end)
end
	-- ezz adds its own A-release subscription to this very same Lobby menu (including
	-- Zombies). Subscribe B explicitly too: stock subscriptions may have been built
	-- before the extra-controller bound was available. Never handle an occluded menu.
	local function install_lobby_leave_handlers(menu)
		if menu == nil or menu.menuName ~= "Lobby" or menu.subscribeToModel == nil
			or Engine.GetModelForController == nil or Engine.GetModel == nil
			or Enum == nil or Enum.LUIButton == nil or Enum.LUIButtonFlags == nil
			or CoD == nil or CoD.BitUtility == nil or CoD.BitUtility.IsBitwiseAndNonZero == nil then
			return
		end
		local button = Enum.LUIButton.LUI_KEY_XBB_PSCIRCLE
		local flag = Enum.LUIButtonFlags.FLAG_DOWN
		if button == nil or flag == nil then return end
		menu.__zz_guest_leave_models = menu.__zz_guest_leave_models or {}
		for c = 1, SUPPORTED - 1 do
			local guest = c
			local model = Engine.GetModel(Engine.GetModelForController(guest), "ButtonBits." .. button)
			if model ~= nil and not menu.__zz_guest_leave_models[guest] then
				menu.__zz_guest_leave_models[guest] = true
				menu:subscribeToModel(model, function(value_model)
					if not menu.occludedBy and not menu.m_inputDisabled and not menu.m_disableAllButtonActions
						and Engine.GetLobbyNetworkMode ~= nil and Engine.GetLobbyNetworkMode() ~= 2
						and (Engine.IsInGame == nil or not Engine.IsInGame())
						and (Engine.GetPrimaryController == nil or guest ~= Engine.GetPrimaryController())
						and Engine.IsControllerBeingUsed(guest) == true
						and CoD.BitUtility.IsBitwiseAndNonZero(Engine.GetModelValue(value_model) or 0, flag) then
						Engine.SetModelValue(value_model, 0)
						request_guest_leave(menu, guest)
					end
				end, false)
			end
		end
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
	install_lobby_a_handlers(menu)
	install_lobby_leave_handlers(menu)
	local a = Enum ~= nil and Enum.LUIButton ~= nil and Enum.LUIButton.LUI_KEY_XBA_PSCROSS or nil
	if a ~= nil and button == a and menu.__zz_guest_join_models ~= nil
		and menu.__zz_guest_join_models[controller] == model
		and (Engine.IsControllerBeingUsed(controller) ~= true or guest_a[controller] ~= nil) then
		-- The independent subscription owns this physical DOWN/release, not select.
		return
	end
	-- The PC's mapped B action is not reliably the console guest-leave action.
	-- Handle only seated guests on the actual offline Lobby, never dialogs,
	-- submenus, the primary controller, or in-game pause menus.
	local buttons = Enum ~= nil and Enum.LUIButton or nil
	if menu.menuName == "Lobby" and type(controller) == "number" and controller > 0 and controller < SUPPORTED
		and (Engine.GetPrimaryController == nil or controller ~= Engine.GetPrimaryController())
		and Engine.IsControllerBeingUsed(controller) == true
		and buttons ~= nil and buttons.LUI_KEY_XBB_PSCIRCLE ~= nil and button == buttons.LUI_KEY_XBB_PSCIRCLE
		and Engine.GetLobbyNetworkMode ~= nil and Engine.GetLobbyNetworkMode() ~= 2
		and (Engine.IsInGame == nil or not Engine.IsInGame())
		and LobbyRemoveLocalClientFromLobby ~= nil then
		Engine.SetModelValue(model, 0)
		request_guest_leave(menu, controller)
		return
	end
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
		-- pad joined player 2). Without the enum do not guess which button is A.
		local a_button = Enum ~= nil and Enum.LUIButton ~= nil and Enum.LUIButton.LUI_KEY_XBA_PSCROSS or nil
		log("unused press c" .. tostring(controller) .. " button " .. tostring(button) .. " A=" .. tostring(a_button))
		if a_button == nil or button ~= a_button then
			return
		end
		Engine.SetModelValue(model, 0)
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
	if Engine.GetLobbyNetworkMode == nil or Engine.GetLobbyNetworkMode() == 2
		or Engine.GetGlobalModel == nil or Engine.GetModel == nil then
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

local function attempt_join(request, controller)
	-- Publish guards before stock: sign-in and prompt/model callbacks can re-enter.
	request.last_attempt = join_epoch
	request.attempts = request.attempts + 1
	join_call_in_progress = true
	if request.attempts > 1 then drop_offline_restricted_warning() end
	local result = request.stock_add(request.menu, controller)
	if Engine.IsControllerBeingUsed(controller) == true then request.seated = true end
	if ForceLobbyButtonUpdate ~= nil then ForceLobbyButtonUpdate(controller) end
	join_call_in_progress = false
	return result
end

local function process_pending_joins()
	if join_call_in_progress then return end
	for c = 0, SUPPORTED - 1 do
		local request = join_pending[c]
		if request ~= nil then
			local menu = request.menu
			local network = Engine.GetLobbyNetworkMode ~= nil and Engine.GetLobbyNetworkMode() or nil
			local max = Dvar.lobby_maxLocalPlayers ~= nil and Dvar.lobby_maxLocalPlayers:get() or 2
			if request.expires <= join_epoch or leave_pending[c] ~= nil then
				join_pending[c] = nil
			elseif Engine.IsControllerBeingUsed(c) == true then
				-- Stop retries, but retain the host-toggle dedup reservation to deadline.
				if not request.seated then
					request.seated = true
					if ForceLobbyButtonUpdate ~= nil then ForceLobbyButtonUpdate(c) end
				end
			elseif not same_join_context(request, c) or menu == nil or menu.menuName ~= "Lobby"
				or (menu.getParent ~= nil and menu:getParent() == nil)
				or (network ~= 0 and network ~= 1)
				or (Engine.IsInGame ~= nil and Engine.IsInGame())
				or (Engine.GetCurrentMap ~= nil and Engine.GetCurrentMap() ~= "core_frontend")
				or Engine.GetUsedControllerCount() >= max then
				join_pending[c] = nil
			elseif not request.seated and request.attempts < JOIN_MAX_ATTEMPTS
				and request.last_attempt ~= join_epoch
				and not menu.occludedBy and not menu.m_inputDisabled and not menu.m_disableAllButtonActions
				and lobby_queue_ready() and (request.attempts == 0
					or (Engine.AreLocalFilesReady ~= nil and Engine.AreLocalFilesReady(c) == true)) then
				-- Bypass reservation only for this exact menu/controller and captured stock.
				attempt_join(request, c)
			end
		end
	end
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

-- Controller choice per player (docs/SIGNIN_REDESIGN.md D4). The PC's Options > Controls
-- gamepad list (StartMenu_Options_Controls_PC.lua, OptionGamepadSettingsPC) has one
-- "Splitscreen controller" entry, fixed to controller 1 (optionController = 1):
-- Engine.GamepadsConnectedMap(controller, port) gives that pad to the controller
-- (0x02284C30; the previous owner is left without one, 0x02284FC0) and the stock code
-- repairs only "the other of two". PCUtility passes optionController to getFunction and
-- setFunction, so one entry per player works with the stock widget. Entries for players
-- 3/4 appear only while they are seated: their pad slots exist only once the pad table
-- is relocated, which a seated guest guarantees. Choosing a pad that another player
-- holds swaps the two pads.
local wrapped_gamepad_prepare = nil

local function controller_has_pad_slot(c)
	return c < 2 or Engine.IsControllerBeingUsed(c) == true
end

local function map_controller_pad(c, port)
	if Engine.GamepadsConnectedMap == nil or Engine.GamepadsConnectedPort == nil then
		return
	end
	local old = Engine.GamepadsConnectedPort(c)
	local owner = nil
	for o = 0, SUPPORTED - 1 do
		if o ~= c and owner == nil and controller_has_pad_slot(o) and Engine.GamepadsConnectedPort(o) == port then
			owner = o
		end
	end
	log("controller choice: c" .. tostring(c) .. " port " .. tostring(port) .. " (had " .. tostring(old)
		.. ", owner " .. tostring(owner) .. ")")
	Engine.GamepadsConnectedMap(c, port)
	if owner == nil then
		return
	end
	if old ~= nil and old ~= port and Engine.GamepadsConnectedValidPort ~= nil
		and Engine.GamepadsConnectedValidPort(old) == true then
		Engine.GamepadsConnectedMap(owner, old)
	elseif Engine.GamepadsConnectedMapAny ~= nil then
		Engine.GamepadsConnectedMapAny(owner)
	end
end

local function controller_entry(c)
	return {
		label = "Player " .. tostring(c + 1) .. " controller",
		description = "PLATFORM_SPLITSCREEN_CONTROLLER_DESC",
		profileVarName = "splitscreen_controller_" .. tostring(c),
		profileType = "function",
		optionController = c,
		datasource = "OptionGamepadMap",
		widgetType = "dropdown",
		getFunction = function(controller)
			return Engine.GamepadsConnectedPort(controller)
		end,
		setFunction = function(controller, port)
			map_controller_pad(controller, port)
		end,
		disabledFunction = function()
			return Engine.GamepadsConnectedCount() == 0
		end
	}
end

-- After the stock list is built: the stock entry (controller 1) gets the swap and the
-- label "Player 2 controller"; players 1, 3 and 4 get entries of the same kind around it.
local function add_controller_entries(list)
	if list == nil or list.customDataSourceHelper == nil or ListHelper_GetListHelperModel == nil
		or ListHelper_CreateModelsFromTable == nil or Engine.GamepadsConnectedPort == nil then
		return
	end
	local items = list[list.customDataSourceHelper]
	if items == nil then
		return
	end
	local stock_at = nil
	for i, item in ipairs(items) do
		local var = item.model ~= nil and Engine.GetModel(item.model, "profileVarName") or nil
		if var ~= nil and Engine.GetModelValue(var) == "splitscreen_controller" then
			stock_at = i
		end
	end
	if stock_at == nil then
		return
	end
	local stock = items[stock_at]
	Engine.SetModelValue(Engine.CreateModel(stock.model, "label"), "Player 2 controller")
	Engine.SetModelValue(Engine.CreateModel(stock.model, "setFunction"), function(controller, port)
		map_controller_pad(controller, port)
	end)
	local root = ListHelper_GetListHelperModel(list, true)
	local next_index = #items
	local function make(c)
		next_index = next_index + 1
		local old_model = Engine.GetModel(root, next_index)
		if old_model ~= nil then
			Engine.UnsubscribeAndFreeModel(old_model)
		end
		local model = Engine.CreateModel(root, next_index)
		ListHelper_CreateModelsFromTable(model, controller_entry(c))
		return { model = model, properties = stock.properties }
	end
	table.insert(items, stock_at, make(0))
	local at = stock_at + 2
	for c = 2, SUPPORTED - 1 do
		if controller_has_pad_slot(c) then
			table.insert(items, at, make(c))
			at = at + 1
		end
	end
end

-- Real controller names in the controller list (Options > Controls > Split Screen). With Steam
-- Input every pad reaches the game as XInput, so the list reads "XINPUT Controller 1..4" whatever
-- the pad is. The component asks Steam which controller each XInput slot really is and answers
-- Engine.GamepadType(8 + slot) with 100 + Steam's type (stock calls use 0..3 and get 0..3 back).
-- A slot Steam does not know keeps the stock text.
local PAD_TYPE_QUERY_BASE = 8
local PAD_TYPE_ANSWER_BASE = 100
local steam_pad_names = {
	[1] = "Steam Controller",
	[2] = "Xbox 360 Controller",
	[3] = "Xbox One Controller",
	[4] = "Gamepad",
	[5] = "PS4 Controller",
	[8] = "Joy-Con Pair",
	[9] = "Joy-Con",
	[10] = "Switch Pro Controller",
	[12] = "PS3 Controller",
	[13] = "PS5 Controller",
	[14] = "Steam Deck",
	[16] = "Switch 2 Pro Controller",
}

local function steam_pad_name(slot)
	if Engine.GamepadType == nil or pcall == nil then
		return nil
	end
	local ok, answer = pcall(Engine.GamepadType, PAD_TYPE_QUERY_BASE + slot)
	if not ok or type(answer) ~= "number" or answer <= PAD_TYPE_ANSWER_BASE then
		return nil
	end
	return steam_pad_names[math.floor(answer - PAD_TYPE_ANSWER_BASE + 0.5)]
end

-- "XINPUT Controller 3" -> "PS4 Controller (3)", keeping the number so two alike pads stay apart.
local function rename_pad_text(text)
	if type(text) ~= "string" then
		return nil
	end
	local n = string.match(string.upper(text), "XINPUT%s*CONTROLLER%s*(%d)")
	if n == nil then
		return nil
	end
	local name = steam_pad_name(tonumber(n) - 1)
	if name == nil then
		return nil
	end
	return name .. " (" .. n .. ")"
end

local wrapped_localize = nil

local function install()
	-- Keep ezz's press-A hint, but remove the clickable PC-only activate/deactivate
	-- control. Its constructor owner is the host, not the pad that pressed A.
	-- Keep LobbySplitscreenToggle itself: ezz invokes it with the guest on release.
	local additional = CoD ~= nil and CoD.FE_ListAdditonal or nil
	local make_additional = additional ~= nil and additional.new or nil
	if type(make_additional) == "function" and not toggle_widgets_wrapped[make_additional] then
		local wrapper = function(menu, controller)
			local widget = make_additional(menu, controller)
			local button = widget.SplitscreenLobbyButtonPC
			if button ~= nil then
				-- Parent state clips call setAlpha again after construction. A one-time
				-- hide leaves an invisible but clickable control, or reappears later.
				local set_alpha = button.setAlpha
				if set_alpha ~= nil then
					button.setAlpha = function(self) return set_alpha(self, 0) end
					button:setAlpha(0)
				end
				if button.setMouseDisabled ~= nil then button:setMouseDisabled(true) end
				if button.makeNotFocusable ~= nil then button:makeNotFocusable() end
				if button.registerEventHandler ~= nil then
					button:registerEventHandler("button_action", function() return true end)
					button:registerEventHandler("mouse_left_click", function() return true end)
				end
			end
			local function hint_eligible()
				local max = Dvar.lobby_maxLocalPlayers ~= nil and Dvar.lobby_maxLocalPlayers:get() or 2
				return lobby_accepts_input(menu) and Engine.GetUsedControllerCount() < max
					and free_pad_controller() ~= nil
			end
			for _, state in ipairs(widget.stateConditions or {}) do
				local original = state.condition
				if state.stateName == "SplitscreenButtonPC" then
					state.condition = function() return false end
				elseif state.stateName == "AddControllerState_Play" then
					state.condition = function(...)
						if offline_frontend(menu) then return hint_eligible() end
						return original == nil or original(...)
					end
				else
					state.condition = function(...)
						-- CoD.Menu.updateElementState stops at the FIRST match. A late
						-- integration's Hide/MapController must not beat the eligible hint.
						if hint_eligible() then return false end
						return original == nil or original(...)
					end
				end
			end
			-- Stock PC ship FE_ListAdditonal's play clip sets StringA alpha=1 and
			-- PLATFORM_FEEDER_SECONDARY_CONTROLLER_PLAY. Use that actual clip, not
			-- a second text widget. Refresh now and on ticks as well as stock models.
			prompt_widgets[widget] = menu
			if menu.updateElementState ~= nil then
				menu:updateElementState(widget, {name = "model_validation", menu = menu})
			end
			return widget
		end
		toggle_widgets_wrapped[wrapper] = true
		additional.new = wrapper
	end
	-- Wrap the action as well as button dispatch: the Zombies lobby's existing
	-- B callback and host UI integrations can call this without passing our handler.
	if LobbyRemoveLocalClientFromLobby ~= nil and LobbyRemoveLocalClientFromLobby ~= wrapped_remove then
		local remove_client = LobbyRemoveLocalClientFromLobby
		stock_remove = remove_client
		wrapped_remove = function(menu, controller)
			if leave_call_in_progress then return remove_client(menu, controller) end
			if type(controller) == "number" and controller > 0 and controller < SUPPORTED
				and controller % 1 == 0
				and (Engine.GetPrimaryController == nil or controller ~= Engine.GetPrimaryController())
				and Engine.GetLobbyNetworkMode ~= nil and Engine.GetLobbyNetworkMode() ~= 2
				and (Engine.IsInGame == nil or not Engine.IsInGame()) then
				request_guest_leave(menu, controller)
				return
			end
			return remove_client(menu, controller)
		end
		LobbyRemoveLocalClientFromLobby = wrapped_remove
	end
	if LUI.createMenu ~= nil and LUI.createMenu.Lobby ~= nil and LUI.createMenu.Lobby ~= wrapped_create_lobby then
		local create_lobby = LUI.createMenu.Lobby
		wrapped_create_lobby = function(controller)
			local menu = create_lobby(controller)
			install_lobby_join_handler(menu, controller)
			install_lobby_a_handlers(menu)
			install_lobby_leave_handlers(menu)
			return menu
		end
		LUI.createMenu.Lobby = wrapped_create_lobby
	end
	-- Controller names: whatever text reads "XINPUT Controller N" when the menu localizes it
	-- gets Steam's name for that pad.
	if Engine.Localize ~= nil and Engine.Localize ~= wrapped_localize then
		local stock_localize = Engine.Localize
		wrapped_localize = function(...)
			local text = stock_localize(...)
			local renamed = rename_pad_text(text)
			if renamed ~= nil then
				return renamed
			end
			return text
		end
		Engine.Localize = wrapped_localize
	end

	local gamepad_list = DataSources ~= nil and DataSources.OptionGamepadSettingsPC or nil
	if gamepad_list ~= nil and gamepad_list.prepare ~= nil and gamepad_list.prepare ~= wrapped_gamepad_prepare then
		local stock_prepare = gamepad_list.prepare
		wrapped_gamepad_prepare = function(controller, list, filter)
			stock_prepare(controller, list, filter)
			add_controller_entries(list)
		end
		gamepad_list.prepare = wrapped_gamepad_prepare
		log("OptionGamepadSettingsPC: a controller entry per player")
	end

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
		wrapped_toggle = function(menu, controller)
			-- ezz's splitscreen_join calls this on A RELEASE with the guest's ID.
			-- This is not a host/mouse click: never choose some other free pad and
			-- never turn a guest's repeated A into DEACTIVATE ALL.
			if type(controller) == "number" and controller > 0 and controller < SUPPORTED
				and controller % 1 == 0
				and (Engine.GetPrimaryController == nil or controller ~= Engine.GetPrimaryController()) then
				-- Our model subscription owns release, even if ezz runs first, the
				-- stock add is refused, or its reservation expires while A is held.
				if menu ~= nil and menu.__zz_guest_join_models ~= nil
					and menu.__zz_guest_join_models[controller] ~= nil then return end
				if not join_call_in_progress and LobbyAddLocalClient ~= nil then
					LobbyAddLocalClient(menu, controller)
				end
				return
			end
			-- A join can synchronously publish a seat and then reach a focused
			-- toggle callback in another root. It must not also add a different pad.
			if join_call_in_progress then
				return
			end
			for c = 0, SUPPORTED - 1 do
				if (join_pending[c] ~= nil and join_pending[c].expires > join_epoch)
					or leave_pending[c] ~= nil then
					return
				end
			end
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
							request_guest_leave(menu, c)
						end
					end
				end
				return
			end
			-- Never call the PC stock toggle: it hard-codes controller 1 and
			-- bypasses our per-controller pending reservation on a fresh rejoin.
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
			-- A model refresh inside stock is not a second physical join request.
			if join_call_in_progress then return end
			local request = reserve_join(menu, controller, stock_add)
			if not request then
				return
			end
			local max = Dvar.lobby_maxLocalPlayers ~= nil and Dvar.lobby_maxLocalPlayers:get() or -1
			log("LobbyAddLocalClient c" .. tostring(controller) .. " used="
				.. tostring(Engine.GetUsedControllerCount()) .. " max=" .. tostring(max))
			-- Preserve a physical release while another lobby operation is busy.
			-- First attempt must still call stock to acquire local profile files.
			if not join_call_in_progress and lobby_queue_ready() and lobby_accepts_input(menu) then
				return attempt_join(request, controller)
			end
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
		join_epoch = join_epoch + 1
		install()
		-- Also cover a Lobby that existed before this script/constructor wrapper.
		local function visit(node)
			install_lobby_a_handlers(node)
			install_lobby_leave_handlers(node)
			if node.getFirstChild ~= nil then
				local child = node:getFirstChild()
				while child ~= nil do
					visit(child)
					child = child:getNextSibling()
				end
			end
		end
		visit(root)
		for c = 1, SUPPORTED - 1 do
			if guest_a[c] ~= nil and not lobby_accepts_input(guest_a[c].menu) then guest_a[c] = nil end
		end
		process_pending_joins()
		process_pending_leaves()
		for widget, menu in pairs(prompt_widgets) do
			if menu.updateElementState ~= nil and (menu.getParent == nil or menu:getParent() ~= nil) then
				menu:updateElementState(widget, {name = "model_validation", menu = menu})
			end
		end
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
