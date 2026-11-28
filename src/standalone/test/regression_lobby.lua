-- Test doubles only: the runner compiles and passes the FULL production script.
-- No private production helpers are copied or exposed by this harness.
local tests = {}
local sandbox_pcall = pcall

local function fixture(production, without_pcall, options)
	options = options or {}
	local state = {
		used = {[0] = true}, ports = {[0] = 0, [1] = 1, [2] = 2, [3] = 3},
		max = 4, network = 0, queue_empty = true, calls = {}, models = {},
		child_events = 0, events = 0, stock_buttons = 0, pad_queries = 0,
		leaves = {}, toggles = {}, active = {}, in_game = false,
		controller_models = {}, notifications = 0, notification_depth = 0,
		global_model = {children = {['lobbyRoot.lobbyNav'] = {value = 100, children = {}, subscribers = {}}}},
		max_notification_depth = 0, stock_dispatches = {}, ezz_releases = {},
		ezz_removes = 0, primary_gamepad = true,
	}
	local function element()
		local node = {handlers = {}, children = {}}
		function node:registerEventHandler(name, handler)
			self.handlers[name] = handler
		end
		function node:addElement(child)
			table.insert(self.children, child)
			child.parent = self
		end
		function node:getParent() return self.parent end
		function node:getFirstChild() return self.children[1] end
		function node:getNextSibling()
			if self.parent == nil then return nil end
			for i, child in ipairs(self.parent.children) do
				if child == self then return self.parent.children[i + 1] end
			end
		end
		function node:subscribeToModel(model, callback, initial)
			assert(model ~= nil and type(callback) == "function")
			table.insert(model.subscribers, {owner = self, callback = callback})
			if initial ~= false then callback(model) end
		end
		function node:processEvent(event)
			local handler = self.handlers[event.name]
			if handler ~= nil and handler(self, event) then
				return true
			end
			for _, child in ipairs(self.children) do
				if child:processEvent(event) then return true end
			end
		end
		return node
	end
	Engine = {
		IsControllerBeingUsed = function(c) return state.used[c] == true end,
		GetUsedControllerCount = function()
			local count = 0
			for c = 0, 3 do if state.used[c] then count = count + 1 end end
			return count
		end,
		GetLobbyNetworkMode = function() return state.network end,
		GetCurrentMap = function() return "core_frontend" end,
		GetPrimaryController = function() return 0 end,
		GetMaxLocalControllers = function() return state.stock_bound or 4 end,
		GetModelForController = function(c) return state.controller_models[c] end,
		GetGlobalModel = function() return state.global_model end,
		GetModel = function(parent, name) return parent.children[name] end,
		CreateModel = function(parent, name)
			if parent.children[name] == nil then
				parent.children[name] = {value = 0, children = {}, subscribers = {}}
			end
			return parent.children[name]
		end,
		GetModelValue = function(model) return model.value end,
		IsInGame = function() return state.in_game end,
		GamepadsConnectedIsActive = function(c) return state.active[c] == true end,
		GamepadsConnectedPort = function(c) return state.ports[c] end,
		GamepadsConnectedValidPort = function(port)
			return type(port) == "number" and port >= 0 and port < 4
		end,
		SetModelValue = function(model, value)
			local changed = model.value ~= value
			model.value = value
			table.insert(state.models, {model = model, value = value})
			if not changed then return end -- Engine does not notify unchanged writes.
			state.notification_depth = state.notification_depth + 1
			state.max_notification_depth = math.max(state.max_notification_depth, state.notification_depth)
			-- Capture the subscription list, but read live values during callbacks.
			-- In particular, SetModelValue(0) re-enters synchronously before returning.
			local subscribers = {}
			for i, sub in ipairs(model.subscribers or {}) do subscribers[i] = sub end
			for _, sub in ipairs(subscribers) do
				state.notifications = state.notifications + 1
				sub.callback(model)
			end
			state.notification_depth = state.notification_depth - 1
		end,
		Localize = function(text, ...) return text end,
		GamepadType = function(slot)
			state.pad_queries = state.pad_queries + 1
			assert(slot == 10)
			return 105 -- Steam's PS4 type for the third displayed pad.
		end,
	}
	Dvar = {lobby_maxLocalPlayers = {get = function() return state.max end}}
	LuaUtils = {LobbyProcessQueueEmpty = function() return state.queue_empty end}
	Enum = {LUIButton = {LUI_KEY_XBA_PSCROSS = "A", LUI_KEY_XBB_PSCIRCLE = "B"},
		LUIButtonFlags = {FLAG_DOWN = 1}}
	CoD = {isPC = true, perController = {[0] = {}},
		BitUtility = {IsBitwiseAndNonZero = function(value, flag)
			-- Arithmetic flag double works with Lua 5.1 as well as lupa's Lua.
			return math.floor(value / flag) % 2 == 1
		end}, Menu = {HandleButtonPress = function()
		state.stock_buttons = state.stock_buttons + 1
		error("production replacement was not installed")
	end}}
	if options.additional_widget then
		state.additional_calls, state.additional_actions, state.ezz_additional_calls = 0, 0, 0
		local function control()
			local button = element()
			button.alpha, button.alpha_writes, button.focusable = 1, 0, true
			function button:setAlpha(value)
				self.alpha = value
				self.alpha_writes = self.alpha_writes + 1
			end
			function button:setMouseDisabled(value) self.mouseDisabled = value end
			function button:setText(value) self.text = value end
			function button:makeNotFocusable() self.focusable = false end
			button:registerEventHandler("button_action", function()
				state.additional_actions = state.additional_actions + 1
				LobbySplitscreenToggle({}, 0)
				return true
			end)
			button:registerEventHandler("mouse_left_click", button.handlers.button_action)
			return button
		end
		CoD.FE_ListAdditonal = {new = function(menu, controller)
			state.additional_calls = state.additional_calls + 1
			local widget = element()
			widget.menu, widget.controller = menu, controller
			widget.SplitscreenLobbyButtonPC, widget.StringA, widget.OtherButton = control(), control(), control()
			widget.StringA.text = "PLATFORM_FEEDER_SECONDARY_CONTROLLER_PLUGIN"
			widget.hint_action = widget.StringA.handlers.button_action
			widget.other_click = widget.OtherButton.handlers.mouse_left_click
			widget.stock_pc_condition = function() return true end
			widget.other_condition = function(_, _, event) return event.allowed end
			widget.stateConditions = {
				-- PC ship 2025-05-31 has Play, AddController, PC in this order.
				-- Include adversarial earlier integration states too: evaluation is
				-- first-match, not an isolated call of the Play predicate.
				{stateName = "Hide", condition = function() return state.hide_prompt == true end},
				{stateName = "MapController", condition = function() return state.map_prompt == true end},
				{stateName = "AddControllerState_Play", condition = function() return false end},
				{stateName = "AddControllerState", condition = function() return state.plugin_prompt == true end},
				{stateName = "SplitscreenButtonPC", condition = widget.stock_pc_condition},
				{stateName = "OtherState", condition = widget.other_condition},
			}
			-- Capture stock parent clips: they look up the child's live setAlpha.
			widget.clipsPerState = {SplitscreenButtonPC = {DefaultClip = function()
				widget.SplitscreenLobbyButtonPC:setAlpha(1)
				widget.StringA:setAlpha(0)
				widget.OtherButton:setAlpha(1)
			end}}
			for _, name in ipairs({"DefaultState", "Hide", "MapController", "OtherState",
				"AddControllerState", "AddControllerState_Play"}) do
				local clip_state = name
				widget.clipsPerState[name] = {DefaultClip = function()
					local play = clip_state == "AddControllerState_Play"
					widget.StringA:setAlpha((play or clip_state == "AddControllerState") and 1 or 0)
					widget.StringA:setText(play and "PLATFORM_FEEDER_SECONDARY_CONTROLLER_PLAY"
						or "PLATFORM_FEEDER_SECONDARY_CONTROLLER_PLUGIN")
					widget.SplitscreenLobbyButtonPC:setAlpha(0)
				end}
			end
			widget.StringA:setAlpha(0)
			return widget
		end}
		function state:install_ezz_additional()
			local create = CoD.FE_ListAdditonal.new
			CoD.FE_ListAdditonal.new = function(menu, controller)
				self.ezz_additional_calls = self.ezz_additional_calls + 1
				local widget = create(menu, controller)
				-- Actual installed ezz replaces (does not append) this condition:
				-- IsWaitingForSignInToPlaySplitscreen() and ShouldShowAddController(...).
				widget.ezz_condition = function()
					return state.stock_hint_waiting == true and state.stock_hint_allowed == true
				end
				widget.stateConditions[3].condition = widget.ezz_condition
				return widget
			end
		end
		if options.additional_ezz then state:install_ezz_additional() end
	end
	LUI = {
		roots = {UIRoot0 = element(), UIRootFull = element()},
		UIElement = {new = element},
		UITimer = {new = function(interval, event, one_shot, target)
			assert(interval == 500 and event == "zz_splitscreen_tick" and one_shot == false)
			assert(target.handlers[event] ~= nil)
			return element()
		end},
	}
	LobbyAddLocalClient = function(menu, controller)
		table.insert(state.calls, {menu = menu, controller = controller})
		-- Asynchronous by default; selected tests model synchronous publication/reentry.
		if state.seat_on_add then state.used[controller] = true end
		if state.on_add then state.on_add(menu, controller) end
		return "stock-add-result"
	end
	LobbyRemoveLocalClientFromLobby = function(menu, controller)
		table.insert(state.leaves, {menu = menu, controller = controller})
		-- Independent of joining: tests explicitly publish completed removals.
		if state.unseat_on_remove then state.used[controller] = false end
		if state.on_remove then state.on_remove(menu, controller) end
	end
	CoD.LobbyBase = {RemoveLocalPlayerFromTheLobby = LobbyRemoveLocalClientFromLobby}
	IsGamepad = function(c) assert(c == 0); return state.primary_gamepad end
	state.button_updates = 0
	ForceLobbyButtonUpdate = function(c)
		assert(c >= 1 and c <= 3)
		state.button_updates = state.button_updates + 1
	end
	for c = 0, 3 do
		state.controller_models[c] = {children = {}}
		for _, button in ipairs({"A", "B", "DPAD_DOWN"}) do
			Engine.CreateModel(state.controller_models[c], "ButtonBits." .. button)
		end
	end
	function state:button_model(c, button)
		return Engine.GetModel(Engine.GetModelForController(c), "ButtonBits." .. button)
	end
	function state:notify(c, button, value)
		local model = self:button_model(c, button)
		Engine.SetModelValue(model, value)
		return model
	end
	local function make_menu(controller, stock_subscriptions)
		local menu = element()
		menu.menuName, menu.m_ownerController = "Lobby", controller or 0
		LUI.roots.UIRootFull:addElement(menu)
		function menu:updateElementState(widget, event)
			local selected = "DefaultState"
			for _, entry in ipairs(widget.stateConditions) do
				if entry.condition == nil or entry.condition(self, widget, event) then
					selected = entry.stateName
					break
				end
			end
			-- CoD.Menu.updateElementState only plays the clip on state change.
			if widget.currentState ~= selected then
				widget.currentState = selected
				widget.clipsPerState[selected].DefaultClip()
			end
		end
		menu.lobbyMode = options.zombies and "Zombies" or "Multiplayer"
		function menu:GetElementAndFunctionTableForButton(button, name)
			assert(name == "buttonFunctions")
			return self.button_entries or {}
		end
		menu:registerEventHandler("unused_gamepad_button", function()
			state.events = state.events + 1
			return true -- Stock PC handler; production must replace this lazily.
		end)
		local child = element()
		child:registerEventHandler("unused_gamepad_button", function(_, event)
			state.child_events = state.child_events + 1
			LobbyAddLocalClient(menu, event.controller)
			return true
		end)
		menu:addElement(child)
		if stock_subscriptions then
			-- Stock anyControllerAllowed subscriptions capture the bound at build time.
			-- Zombies can build with only the host bound, before native widening.
			for c = 0, Engine.GetMaxLocalControllers() - 1 do
				local guest = c
				for _, key in ipairs({"A", "B", "DPAD_DOWN"}) do
					local button = key
					menu:subscribeToModel(state:button_model(guest, button), function(model)
						if not menu.occludedBy and not menu.m_inputDisabled and not menu.m_disableAllButtonActions
							and CoD.BitUtility.IsBitwiseAndNonZero(Engine.GetModelValue(model) or 0,
								Enum.LUIButtonFlags.FLAG_DOWN) then
							table.insert(state.stock_dispatches, {controller = guest, button = button})
							CoD.Menu.HandleButtonPress(menu, guest, button, model)
						end
					end, false)
				end
			end
		end
		return menu
	end
	if options.create_lobby then
		LUI.createMenu = {Lobby = function(controller)
			local menu = make_menu(controller, true)
			-- The native bound becomes available after stock built its subscriptions,
			-- before ezz's post-construction loop queries it again.
			state.stock_bound = 4
			return menu
		end}
	end
	function state:install_ezz()
		-- Behavioral reproduction of the installed splitscreen_join integration:
		-- A arms on DOWN and calls the toggle with the SAME guest on RELEASE.
		-- No external files are executed or modified; FE-list UI has a separate double.
		local create = assert(LUI.createMenu.Lobby)
		LUI.createMenu.Lobby = function(controller)
			local menu, pressed = create(controller), {}
			for c = 1, Engine.GetMaxLocalControllers() - 1 do
				local joining = c
				local model = Engine.CreateModel(Engine.GetModelForController(joining), "ButtonBits." .. Enum.LUIButton.LUI_KEY_XBA_PSCROSS)
				menu:subscribeToModel(model, function(button_model)
					local value = Engine.GetModelValue(button_model) or 0
					if CoD.BitUtility.IsBitwiseAndNonZero(value, Enum.LUIButtonFlags.FLAG_DOWN) then
						pressed[joining] = not Engine.IsControllerBeingUsed(joining)
						return
					end
					local was_pressed = pressed[joining]
					pressed[joining] = false
					if was_pressed and not menu.m_inputDisabled and not menu.occludedBy
						and not Engine.IsControllerBeingUsed(joining) and LuaUtils.LobbyProcessQueueEmpty() then
						table.insert(state.ezz_releases, joining)
						LobbySplitscreenToggle(menu, joining)
					end
				end, false)
			end
			return menu
		end
		local remove = LobbyRemoveLocalClientFromLobby
		LobbyRemoveLocalClientFromLobby = function(menu, controller)
			state.ezz_removes = state.ezz_removes + 1
			local primary = Engine.GetPrimaryController()
			if IsGamepad(primary) or controller == primary or not Engine.IsControllerBeingUsed(controller) then
				return remove(menu, controller)
			end
			CoD.LobbyBase.RemoveLocalPlayerFromTheLobby(menu, controller)
			CoD.perController[primary].localClientChanged = true
			ForceLobbyButtonUpdate(controller)
		end
	end
	-- Optional unrelated integrations are intentionally absent.
	DataSources, ListHelper_GetListHelperModel, ListHelper_CreateModelsFromTable = nil, nil, nil
	IsSplitscreenLobbyRoomAvailable, LobbySplitscreenToggle, ShouldOpenMessageDialog = nil, nil, nil
	IsSplitscreenPlayAvailable, GamepadsConnectedAny = nil, nil
	if options.toggle then
		-- Must exist before production installs wrappers, not after production().
		IsSplitscreenLobbyRoomAvailable = function()
			return Engine.GetUsedControllerCount() < state.max
		end
		IsSplitscreenPlayAvailable = function() return true end
		GamepadsConnectedAny = function()
			for c = 1, 3 do if state.active[c] then return true end end
			return false
		end
		LobbySplitscreenToggle = function(menu, controller)
			table.insert(state.toggles, {menu = menu, controller = controller})
			-- Detect the stock PC path even if it bypasses the guarded add function.
			if state.malicious_toggle then state.used[1] = true end
		end
	end
	IsGameTypeDOA = nil
	if options.ezz then state:install_ezz() end
	pcall = sandbox_pcall
	if without_pcall then pcall = nil end
	_G.__zz_splitscreen_loaded = nil
	production()
	assert(#LUI.roots.UIRoot0.children == 1 and #LUI.roots.UIRootFull.children == 1)
	local watcher = LUI.roots.UIRootFull.children[1]
	assert(watcher.handlers.zz_splitscreen_tick and watcher.handlers.splitscreen_update)
	function state:tick()
		-- Pick ONE registered watcher: ticking both would advance two epochs.
		assert(watcher:processEvent({name = "zz_splitscreen_tick"}) == true)
	end
	function state:menu()
		return make_menu(0, false)
	end
	function state:lobby(stock_bound)
		self.stock_bound = stock_bound or 4
		local menu = LUI.createMenu.Lobby(0)
		self.stock_bound = 4 -- Engine bound is widened after the menu was built.
		return menu
	end
	function state:press(menu, controller, button)
		local model = {value = 1}
		CoD.Menu.HandleButtonPress(menu, controller, button or "A", model)
		return model
	end
	return state
end

local function assert_additional_widget(s, widget, menu, controller, ezz)
	assert(widget.menu == menu and widget.controller == controller)
	local button, hint, other = widget.SplitscreenLobbyButtonPC, widget.StringA, widget.OtherButton
	assert(button.alpha == 0 and button.mouseDisabled == true and button.focusable == false)
	local writes = button.alpha_writes
	button:setAlpha(1)
	assert(button.alpha == 0 and button.alpha_writes == writes + 1)
	widget.clipsPerState.SplitscreenButtonPC.DefaultClip()
	assert(button.alpha == 0 and button.alpha_writes == writes + 2)
	assert(button:processEvent({name = "button_action"}) == true)
	assert(button:processEvent({name = "mouse_left_click"}) == true)
	assert(s.additional_actions == 0 and #s.toggles == 0 and #s.calls == 0)
	assert(hint.focusable == true)
	assert(hint.mouseDisabled == nil and other.alpha == 1 and other.mouseDisabled == nil and other.focusable == true)
	assert(hint.handlers.button_action == widget.hint_action)
	assert(other.handlers.mouse_left_click == widget.other_click)
	assert(#widget.stateConditions == 6)
	assert(widget.stateConditions[5].stateName == "SplitscreenButtonPC")
	assert(widget.stateConditions[5].condition ~= widget.stock_pc_condition)
	assert(widget.stateConditions[5].condition(menu, widget, {allowed = true, play = true}) == false)
	s.network = 2
	assert(widget.stateConditions[6].condition(menu, widget, {allowed = true}) == true)
	assert(widget.stateConditions[6].condition(menu, widget, {allowed = false}) == false)
	s.network = 0
	if ezz then
		local state = widget.stateConditions[3]
		assert(state.stateName == "AddControllerState_Play" and state.condition ~= widget.ezz_condition)
		s.network = 2 -- Online still delegates to ezz's original condition.
		s.stock_hint_waiting, s.stock_hint_allowed = true, true
		assert(state.condition(widget, menu, {play = true}) == true)
		s.stock_hint_allowed = false
		assert(state.condition(widget, menu, {play = false}) == false)
		s.network = 0
	end
end

function tests.additional_widget_hides_only_host_pc_toggle(production)
	local s = fixture(production, false, {toggle = true, additional_widget = true, additional_ezz = true})
	s.active[1], s.malicious_toggle = true, true
	local menu = s:menu()
	local constructor = CoD.FE_ListAdditonal.new
	assert_additional_widget(s, constructor(menu, 0), menu, 0, true)
	for _ = 1, 20 do s:tick() end
	assert(CoD.FE_ListAdditonal.new == constructor)
	assert_additional_widget(s, constructor(menu, 2), menu, 2, true)
	assert(s.additional_calls == 2 and s.ezz_additional_calls == 2 and not s.used[1])
end

function tests.additional_widget_rewraps_late_ezz_once(production)
	local s = fixture(production, false, {toggle = true, additional_widget = true})
	local menu = s:menu()
	local first = CoD.FE_ListAdditonal.new
	assert_additional_widget(s, first(menu, 0), menu, 0, false)
	s:install_ezz_additional()
	local external = CoD.FE_ListAdditonal.new
	s:tick()
	local wrapped = CoD.FE_ListAdditonal.new
	assert(wrapped ~= external and wrapped ~= first)
	assert_additional_widget(s, wrapped(menu, 0), menu, 0, true)
	for _ = 1, 20 do s:tick() end
	production()
	assert(CoD.FE_ListAdditonal.new == wrapped)
	assert_additional_widget(s, wrapped(menu, 1), menu, 1, true)
	assert(s.additional_calls == 3 and s.ezz_additional_calls == 2)
end

function tests.delayed_seat_deduplicates_across_menus(production)
	local s = fixture(production)
	local first, second = s:menu(), s:menu()
	assert(LobbyAddLocalClient(first, 1) == "stock-add-result")
	assert(LobbyAddLocalClient(second, 1) == nil)
	for _ = 1, 3 do
		assert(s:press(first, 1).value == 0)
		assert(s:press(second, 1).value == 0)
	end
	assert(#s.calls == 1 and s.calls[1].controller == 1 and s.calls[1].menu == first)
	assert(not s.used[1] and s.child_events == 0 and s.events == 0)
end

function tests.separate_pads_can_join_while_pending(production)
	local s = fixture(production)
	local menu = s:menu()
	for c = 1, 3 do assert(s:press(menu, c).value == 0) end
	assert(#s.calls == 3)
	for c = 1, 3 do assert(s.calls[c].controller == c and s.calls[c].menu == menu) end
end

function tests.known_port_pending_and_seated_rejected(production)
	local s = fixture(production)
	local menu = s:menu()
	s.ports[2] = s.ports[1]
	LobbyAddLocalClient(menu, 1)
	LobbyAddLocalClient(menu, 2)
	assert(#s.calls == 1)
	s.used[1] = true -- Real seat publication also retires its pending entry.
	for _ = 1, 6 do s:tick() end
	LobbyAddLocalClient(menu, 2)
	assert(#s.calls == 1)
	s.ports[2] = s.ports[0] -- Host port is protected without a pending join too.
	LobbyAddLocalClient(menu, 2)
	assert(#s.calls == 1)
	s.ports[2] = 2
	LobbyAddLocalClient(menu, 2)
	assert(#s.calls == 2 and s.calls[2].controller == 2)
end

function tests.unknown_ports_do_not_alias(production)
	local s = fixture(production)
	s.ports[1], s.ports[2] = -1, -1
	local menu = s:menu()
	LobbyAddLocalClient(menu, 1)
	LobbyAddLocalClient(menu, 2)
	assert(#s.calls == 2)
end

function tests.invalid_controller_values_rejected(production)
	local s = fixture(production)
	local menu = s:menu()
	local invalid = {{}, {value = false}, {value = "1"}, {value = {}},
		{value = -1}, {value = 4}, {value = 100}, {value = 1.5},
		{value = math.huge}, {value = 0 / 0}}
	for _, entry in ipairs(invalid) do
		LobbyAddLocalClient(menu, entry.value)
		assert(#s.calls == 0)
	end
	LobbyAddLocalClient(menu, 3)
	assert(#s.calls == 1 and s.calls[1].controller == 3)
end

function tests.used_controller_rejected(production)
	local s = fixture(production)
	LobbyAddLocalClient(s:menu(), 0)
	assert(#s.calls == 0)
end

function tests.online_and_missing_network_rejected(production)
	local s = fixture(production)
	local menu = s:menu()
	s.network = 2
	LobbyAddLocalClient(menu, 1)
	assert(s:press(menu, 1).value == 1)
	Engine.GetLobbyNetworkMode = nil
	LobbyAddLocalClient(menu, 1)
	assert(s:press(menu, 1).value == 1)
	assert(#s.calls == 0 and #s.models == 0 and not menu.__zz_console_join)
	Engine.GetLobbyNetworkMode = function() return 1 end -- LAN is allowed.
	LobbyAddLocalClient(menu, 1)
	assert(#s.calls == 1)
end

function tests.full_lobby_rejected_without_reserving(production)
	local s = fixture(production)
	local menu = s:menu()
	s.max = 1
	LobbyAddLocalClient(menu, 1)
	assert(#s.calls == 0)
	s.max = 4
	LobbyAddLocalClient(menu, 1)
	assert(#s.calls == 1)
end

function tests.busy_queue_preserved_without_another_press(production)
	local s = fixture(production)
	local menu = s:menu()
	s.queue_empty = false
	LobbyAddLocalClient(menu, 1)
	s:press(menu, 1)
	assert(#s.calls == 0)
	s.queue_empty = true
	s:tick()
	assert(#s.calls == 1)
end

function tests.expiration_at_six_watcher_ticks(production)
	local s = fixture(production)
	local menu = s:menu()
	local wrapped_add, wrapped_press = LobbyAddLocalClient, CoD.Menu.HandleButtonPress
	LobbyAddLocalClient(menu, 1)
	-- Update events do not advance the reservation clock.
	for _ = 1, 6 do
		assert(LUI.roots.UIRoot0.children[1]:processEvent({name = "splitscreen_update"}) == true)
	end
	LobbyAddLocalClient(menu, 1)
	assert(#s.calls == 1)
	for _ = 1, 5 do
		s:tick()
		LobbyAddLocalClient(menu, 1)
		assert(#s.calls == 1)
	end
	s:tick()
	assert(LobbyAddLocalClient(menu, 1) == "stock-add-result")
	assert(#s.calls == 2 and not s.used[1])
	LobbyAddLocalClient(menu, 1)
	assert(#s.calls == 2)
	assert(LobbyAddLocalClient == wrapped_add and CoD.Menu.HandleButtonPress == wrapped_press)
end

function tests.join_event_consumed_before_children(production)
	local s = fixture(production)
	local menu = s:menu()
	assert(s:press(menu, 1).value == 0) -- Installs the actual lobby handler.
	assert(#s.calls == 1 and menu.__zz_console_join)
	assert(menu:processEvent({name = "unused_gamepad_button", controller = 2}) == true)
	assert(#s.calls == 2 and s.calls[2].controller == 2)
	assert(menu:processEvent({name = "unused_gamepad_button", controller = 2}) == true)
	LobbyAddLocalClient = nil
	assert(menu:processEvent({name = "unused_gamepad_button", controller = 3}) == true)
	assert(s.child_events == 0 and s.events == 0)
end

function tests.only_a_joins_and_resets_model(production)
	local s = fixture(production)
	local menu = s:menu()
	for _, button in ipairs({"B", "DPAD_UP", "START"}) do
		assert(s:press(menu, 1, button).value == 1)
	end
	assert(#s.calls == 0 and #s.models == 0 and not menu.__zz_console_join)
	local model = s:press(menu, 1, "A")
	assert(model.value == 0 and #s.models == 1 and s.models[1].model == model)
	assert(#s.calls == 1 and s.calls[1].controller == 1)
end

function tests.missing_a_enum_denies_join(production)
	local s = fixture(production)
	local menu = s:menu()
	for _, missing in ipairs({"Enum", "LUIButton", "LUI_KEY_XBA_PSCROSS"}) do
		Enum = {LUIButton = {LUI_KEY_XBA_PSCROSS = "A"}}
		if missing == "Enum" then Enum = nil
		elseif missing == "LUIButton" then Enum.LUIButton = nil
		else Enum.LUIButton.LUI_KEY_XBA_PSCROSS = nil end
		assert(s:press(menu, 1).value == 1)
	end
	assert(#s.calls == 0 and #s.models == 0 and not menu.__zz_console_join)
end

function tests.localization_without_pcall_is_safe(production)
	-- BOIII sandbox: absent during full source load AND subsequent callbacks.
	local s = fixture(production, true)
	assert(pcall == nil)
	assert(Engine.Localize("XINPUT Controller 3") == "XINPUT Controller 3")
	assert(Engine.Localize("Other text") == "Other text")
	assert(s.pad_queries == 0)
	s:tick() -- Reinstallation must preserve the guard too.
	assert(Engine.Localize("XINPUT Controller 3") == "XINPUT Controller 3")
	assert(s.pad_queries == 0)
	pcall = sandbox_pcall
	assert(Engine.Localize("XINPUT Controller 3") == "PS4 Controller (3)")
	assert(s.pad_queries == 1)
end

function tests.used_controller_keeps_stock_navigation(production)
	local s = fixture(production)
	local menu = s:menu()
	s.network = 2 -- Used controllers navigate even online and without A enum.
	Enum = nil
	local calls, target, expected_model = {}, {}, nil
	menu.button_entries = {
		{element = target, fn = function(el, actual_menu, c, model)
			assert(el == target and actual_menu == menu and c == 0 and model == expected_model)
			table.insert(calls, "first")
			return false
		end},
		{element = target, fn = function()
			table.insert(calls, "handled")
			return true
		end},
		{fn = function() error("navigation must stop after handled") end},
	}
	expected_model = {value = 1}
	CoD.Menu.HandleButtonPress(menu, 0, "DPAD_DOWN", expected_model)
	assert(#calls == 2 and calls[1] == "first" and calls[2] == "handled")
	assert(expected_model.value == 0 and #s.models == 2)
	menu.button_entries = {{fn = function() return false end}}
	assert(s:press(menu, 0, "B").value == 0) -- Any mapped stock entry consumes its model.
	menu.button_entries = {}
	assert(s:press(menu, 0, "B").value == 1) -- Unmapped navigation stays untouched.
	assert(#s.calls == 0 and s.child_events == 0 and not menu.__zz_console_join)
	assert(s.stock_buttons == 0) -- Exercises the replacement's literal stock branch.
end

for guest = 1, 3 do
	local target = guest
	tests["b_removes_only_guest_" .. target] = function(production)
		local s = fixture(production)
		local menu = s:menu()
		for c = 1, 3 do s.used[c] = true end
		menu.button_entries = {{fn = function() error("guest B reached stock navigation") end}}
		local model = s:press(menu, target, "B")
		assert(model.value == 0 and #s.models == 1 and s.models[1].model == model)
		assert(#s.leaves == 1 and s.leaves[1].controller == target and s.leaves[1].menu == menu)
		assert(#s.calls == 0 and s.child_events == 0 and s.events == 0)
		for c = 0, 3 do assert(s.used[c]) end -- Removal remains asynchronous.
	end
end

function tests.busy_leave_retries_on_tick_without_another_b(production)
	local s = fixture(production)
	local menu = s:menu()
	s.used[2], s.queue_empty = true, false
	assert(s:press(menu, 2, "B").value == 0)
	assert(#s.leaves == 0 and #s.calls == 0)
	s:tick()
	assert(#s.leaves == 0)
	s.queue_empty = true
	s:tick()
	assert(#s.leaves == 1 and s.leaves[1].controller == 2 and s.leaves[1].menu == menu)
	s:tick()
	assert(#s.leaves == 2 and s.leaves[2].controller == 2) -- Still seated: retry next epoch.
	s.used[2] = false -- Publish completion; a still-seated guest would be retried.
	s:tick()
	assert(#s.leaves == 2 and #s.calls == 0)
end

function tests.repeated_b_across_menus_and_reentry_removes_once(production)
	local s = fixture(production)
	local first, second = s:menu(), s:menu()
	s.used[3] = true
	s.on_remove = function(menu, c)
		assert(menu == first and c == 3)
		assert(s:press(second, c, "B").value == 0)
	end
	assert(s:press(first, 3, "B").value == 0)
	for tick = 1, 5 do
		assert(s:press(second, 3, "B").value == 0)
		assert(#s.leaves == tick) -- Reentry/repeated B cannot resend within this epoch.
		s:tick()
		assert(#s.leaves == tick + 1)
	end
	assert(#s.leaves == 6)
	for _, leave in ipairs(s.leaves) do assert(leave.controller == 3 and leave.menu == first) end
	s.used[3] = false
	s:tick() -- Completion retires the request, rather than retrying it.
	assert(#s.leaves == 6)
	assert(#s.calls == 0)
end

function tests.sent_leave_expires_at_six_ticks(production)
	local s = fixture(production)
	local menu = s:menu()
	s.used[1] = true
	s:press(menu, 1, "B")
	for tick = 1, 6 do
		local before = #s.leaves
		s:tick()
		assert(#s.leaves - before <= 1 and #s.leaves <= 6)
	end
	local attempts = #s.leaves
	assert(attempts >= 1 and attempts <= 6) -- At most once per epoch until timeout.
	for _ = 1, 3 do s:tick() end
	assert(#s.leaves == attempts) -- An expired request never sends a late retry.
	s:press(menu, 1, "B")
	assert(#s.leaves == attempts + 1 and s.leaves[attempts + 1].controller == 1 and #s.calls == 0)
end

function tests.unsent_busy_leave_expires_without_late_remove(production)
	local s = fixture(production)
	local menu = s:menu()
	s.used[2], s.queue_empty = true, false
	s:press(menu, 2, "B")
	for _ = 1, 6 do s:tick() end
	s.queue_empty = true
	s:tick()
	assert(#s.leaves == 0)
	s:press(menu, 2, "B")
	assert(#s.leaves == 1 and s.leaves[1].controller == 2 and #s.calls == 0)
end

function tests.b_outside_offline_guest_lobby_preserves_stock_navigation(production)
	for _, scenario in ipairs({"host", "submenu", "online", "in_game"}) do
		local s = fixture(production)
		local menu = s:menu()
		local controller = scenario == "host" and 0 or 2
		s.used[controller] = true
		if scenario == "submenu" then menu.menuName = "Options" end
		if scenario == "online" then s.network = 2 end
		if scenario == "in_game" then s.in_game = true end
		local navigated = 0
		local target = {}
		menu.button_entries = {{element = target, fn = function(el, actual_menu, c, model)
			assert(el == target and actual_menu == menu and c == controller and model.value == 1)
			navigated = navigated + 1
			return true
		end}}
		assert(s:press(menu, controller, "B").value == 0)
		assert(navigated == 1 and #s.models == 2 and #s.leaves == 0 and #s.calls == 0)
		assert(s.used[controller] and s.stock_buttons == 0)
	end
end

function tests.leave_clears_stale_join_and_allows_immediate_targeted_rejoin(production)
	local s = fixture(production)
	local menu = s:menu()
	s:press(menu, 2) -- Reserve, then publish the seat without a watcher tick.
	assert(#s.calls == 1 and s.calls[1].controller == 2)
	s.used[2] = true
	s:press(menu, 2, "B")
	assert(#s.leaves == 1 and s.leaves[1].controller == 2)
	s.used[2] = false -- Completion is visible before a watcher can clear leave_pending.
	assert(s:press(menu, 2).value == 0)
	assert(#s.calls == 2 and s.calls[2].controller == 2 and s.calls[2].menu == menu)
	assert(#s.leaves == 1 and not s.used[1] and not s.used[3])
	s:press(menu, 2)
	assert(#s.calls == 2)
end

function tests.synchronous_a_join_blocks_reentrant_and_subsequent_toggle(production)
	local s = fixture(production, false, {toggle = true})
	local menu, other = s:menu(), s:menu()
	s.active[2], s.active[3], s.seat_on_add, s.malicious_toggle = true, true, true, true
	s.on_add = function(actual_menu, c)
		assert(actual_menu == menu and c == 2 and s.used[2])
		LobbySplitscreenToggle(other, 0) -- Synchronous stock callback into another root.
	end
	assert(s:press(menu, 2).value == 0)
	LobbySplitscreenToggle(other, 0) -- Seat already visible, reservation still unexpired.
	assert(#s.calls == 1 and s.calls[1].controller == 2 and s.calls[1].menu == menu)
	assert(s.used[2] and not s.used[1] and not s.used[3])
	assert(#s.toggles == 0 and #s.leaves == 0)
end

function tests.fresh_toggle_selects_free_pad_through_guarded_add_never_stock(production)
	local s = fixture(production, false, {toggle = true})
	local menu = s:menu()
	s.active[2], s.active[3], s.malicious_toggle = true, true, true
	assert(not s.active[1] and not s.used[1])
	assert(IsSplitscreenLobbyRoomAvailable())
	LobbySplitscreenToggle(menu, 0)
	assert(#s.calls == 1 and s.calls[1].controller == 2 and s.calls[1].menu == menu)
	LobbyAddLocalClient(menu, 2) -- Same reservation must guard direct adds too.
	LobbySplitscreenToggle(menu, 0)
	assert(#s.calls == 1 and #s.toggles == 0 and #s.leaves == 0 and not s.used[1])
end

function tests.toggle_blocks_any_pending_join_until_expiration(production)
	local s = fixture(production, false, {toggle = true})
	local menu = s:menu()
	s.active[2], s.active[3] = true, true
	LobbyAddLocalClient(menu, 3) -- Pending pad is not the first free pad.
	for _ = 1, 5 do
		LobbySplitscreenToggle(menu, 0)
		assert(#s.calls == 1)
		s:tick()
	end
	LobbySplitscreenToggle(menu, 0)
	assert(#s.calls == 1)
	s:tick()
	LobbySplitscreenToggle(menu, 0)
	assert(#s.calls == 2 and s.calls[1].controller == 3 and s.calls[2].controller == 2)
	assert(#s.toggles == 0 and #s.leaves == 0)
end

function tests.toggle_blocks_pending_leave_even_with_another_free_pad(production)
	local s = fixture(production, false, {toggle = true})
	local menu = s:menu()
	s.used[2], s.active[3], s.queue_empty = true, true, false
	s:press(menu, 2, "B")
	s.queue_empty = true
	LobbySplitscreenToggle(menu, 0)
	assert(#s.calls == 0 and #s.leaves == 0)
	s:tick() -- Now sent, but not yet completed.
	LobbySplitscreenToggle(menu, 0)
	assert(#s.calls == 0 and #s.leaves == 1 and s.leaves[1].controller == 2)
	s.used[2] = false
	s:tick()
	LobbySplitscreenToggle(menu, 0)
	assert(#s.calls == 1 and s.calls[1].controller == 3 and #s.toggles == 0)
end

function tests.leave_two_guests_rejoin_one_without_adding_another(production)
	local s = fixture(production, false, {toggle = true})
	local menu = s:menu()
	s.used[2], s.used[3] = true, true
	s:press(menu, 2, "B")
	s:press(menu, 3, "B")
	assert(#s.leaves == 2 and s.leaves[1].controller == 2 and s.leaves[2].controller == 3)
	assert(s.leaves[1].menu == menu and s.leaves[2].menu == menu)
	s.used[2], s.used[3] = false, false
	s.active[2], s.active[3], s.seat_on_add, s.malicious_toggle = true, true, true, true
	assert(s:press(menu, 3).value == 0) -- No tick before the targeted rejoin.
	LobbySplitscreenToggle(menu, 0)
	s:press(menu, 3, "A")
	assert(#s.calls == 1 and s.calls[1].controller == 3 and s.calls[1].menu == menu)
	assert(s.used[0] and s.used[3] and not s.used[1] and not s.used[2])
	assert(#s.leaves == 2 and #s.toggles == 0)
end

-- Exercise real subscriptions, not direct calls to HandleButtonPress. A stock
-- bound of one reproduces Zombies building before extra-controller widening;
-- ezz still queries the widened bound when its outer constructor returns.
for _, bound in ipairs({1, 4}) do
	for guest = 1, 3 do
		local stock_bound, target = bound, guest
		tests["model_a_press_release_bound_" .. stock_bound .. "_guest_" .. target] = function(production)
			local s = fixture(production, false, {toggle = true, create_lobby = true, ezz = true, zombies = true})
			local menu = s:lobby(stock_bound)
			for c = 1, 3 do s.active[c] = true end -- Other free pads must never be selected.
			s.malicious_toggle = true
			local model = s:button_model(target, "A")
			assert(model == Engine.CreateModel(Engine.GetModelForController(target), "ButtonBits.A"))
			assert(#model.subscribers == (stock_bound == 1 and 2 or 3))
			s:notify(target, "A", 2) -- A non-DOWN flag is not an A press/release pair.
			s:notify(target, "A", 0)
			assert(#s.calls == 0 and #s.ezz_releases == 0)
			s:notify(target, "A", 3) -- DOWN plus an unrelated bit.
			assert(#s.calls == 0 and model.value == 3)
			assert(#s.stock_dispatches == (stock_bound == 1 and 0 or 1))
			for _ = 1, 6 do s:tick() end
			s:notify(target, "A", 0)
			local expected = 1
			assert(#s.calls == expected and s.calls[1].controller == target)
			LobbySplitscreenToggle(menu, 0) -- Host UI callback remains guarded.
			assert(#s.calls == expected)
			for _, call in ipairs(s.calls) do assert(call.controller == target and call.menu == menu) end
			-- With a seat visible, a late guest toggle is NOT a generic host click,
			-- even after every join reservation has expired.
			s.used[target] = true
			for _ = 1, 6 do s:tick() end
			LobbySplitscreenToggle(menu, target)
			s:notify(target, "A", 1)
			s:notify(target, "A", 0)
			assert(#s.calls == expected and #s.leaves == 0 and #s.toggles == 0)
			for c = 1, 3 do assert(Engine.IsControllerBeingUsed(c) == (c == target)) end
			assert(s.used[0] and s.notification_depth == 0)
		end
	end
end

for guest = 1, 3 do
	local target = guest
	tests["zombies_model_b_without_stock_guest_subscription_" .. target] = function(production)
		local s = fixture(production, false, {create_lobby = true, ezz = true, zombies = true})
		local menu = s:lobby(1)
		assert(menu.lobbyMode == "Zombies" and menu.__zz_console_join)
		for c = 1, 3 do
			s.used[c] = true
			assert(#s:button_model(c, "B").subscribers == 1) -- Production only, no stock guest callback.
		end
		s:notify(target, "B", 2)
		assert(#s.leaves == 0)
		local model = s:notify(target, "B", 1)
		assert(model.value == 0 and #s.leaves == 1 and s.max_notification_depth >= 2)
		assert(s.leaves[1].menu == menu and s.leaves[1].controller == target)
		for _ = 1, 4 do s:notify(target, "B", 1) end
		assert(#s.leaves == 1 and #s.stock_dispatches == 0 and #s.calls == 0)
		s.used[target] = false
		for _ = 1, 7 do s:tick() end
		assert(#s.leaves == 1 and s.used[0] and s.notification_depth == 0)
		for c = 1, 3 do assert(s.used[c] == (c ~= target)) end
	end
end

function tests.ezz_release_alone_joins_target_and_guards_reentrant_host_click(production)
	local s = fixture(production, false, {toggle = true, create_lobby = true, ezz = true, zombies = true})
	local menu = s:lobby(1)
	s.active[1], s.active[2], s.active[3], s.seat_on_add = true, true, true, true
	s.on_add = function(actual_menu, c)
		assert(actual_menu == menu and c == 3 and s.used[c])
		LobbySplitscreenToggle(menu, 0)
	end
	s:notify(3, "A", 1)
	assert(#s.calls == 0)
	for _ = 1, 6 do s:tick() end -- No synthetic direct press or reservation is required.
	s:notify(3, "A", 0)
	-- Ezz runs first in this construction order, but its toggle is suppressed;
	-- production then owns the sole stock add on the very same release.
	assert(#s.ezz_releases == 1 and s.ezz_releases[1] == 3)
	assert(#s.calls == 1 and s.calls[1].controller == 3 and s.used[3])
	assert(not s.used[1] and not s.used[2] and #s.leaves == 0 and #s.toggles == 0)
end

function tests.model_b_busy_repetition_does_not_extend_timeout(production)
	local s = fixture(production, false, {create_lobby = true, zombies = true})
	local menu = s:lobby(1)
	s.used[2], s.queue_empty = true, false
	s:notify(2, "B", 1)
	for _ = 1, 5 do
		s:tick()
		s:notify(2, "B", 1)
		LobbyRemoveLocalClientFromLobby(menu, 2)
		assert(#s.leaves == 0)
	end
	s:tick() -- Original epoch-six deadline, not six ticks after the last B.
	s.queue_empty = true
	s:tick()
	assert(#s.leaves == 0)
	s:notify(2, "B", 1) -- A genuinely new request after timeout is allowed.
	assert(#s.leaves == 1 and s.leaves[1].controller == 2 and #s.calls == 0)
end

function tests.model_b_repeated_and_global_remove_defer_busy_queue(production)
	local s = fixture(production, false, {create_lobby = true, ezz = true, zombies = true})
	local first, second = s:lobby(1), s:lobby(1)
	s.used[2], s.queue_empty = true, false
	for _ = 1, 4 do
		s:notify(2, "B", 1) -- Both menus see the same model, including reset reentry.
		LobbyRemoveLocalClientFromLobby(second, 2) -- Stock action bypassing button dispatch.
	end
	assert(#s.leaves == 0 and s.ezz_removes == 0)
	for _ = 1, 2 do s:tick(); assert(#s.leaves == 0) end
	s.queue_empty = true
	s:tick()
	assert(#s.leaves == 1 and s.ezz_removes == 1)
	assert(s.leaves[1].controller == 2 and s.leaves[1].menu == first)
	for _ = 1, 3 do s:notify(2, "B", 1); LobbyRemoveLocalClientFromLobby(second, 2) end
	assert(#s.leaves == 1) -- Busy retries and repeated B do not reset the epoch guard.
	s.used[2] = false
	s:tick()
	assert(#s.leaves == 1 and #s.calls == 0 and s.notification_depth == 0)
end

function tests.global_remove_refused_once_retries_next_epoch(production)
	local s = fixture(production, false, {create_lobby = true, zombies = true})
	local menu = s:lobby(1)
	s.used[3] = true
	s.on_remove = function(actual_menu, c)
		assert(actual_menu == menu and c == 3)
		-- A refusal leaves the seat intact. Synchronous model callbacks must not
		-- retry in this epoch, nor extend the six-tick deadline.
		s:notify(c, "B", 1)
		if #s.leaves == 2 then s.used[c] = false end
	end
	LobbyRemoveLocalClientFromLobby(menu, 3)
	for _ = 1, 5 do LobbyRemoveLocalClientFromLobby(menu, 3) end
	assert(#s.leaves == 1 and s.used[3])
	s:tick()
	assert(#s.leaves == 2 and not s.used[3])
	for _ = 1, 7 do s:tick() end
	assert(#s.leaves == 2 and #s.calls == 0 and s.used[0])
end

for _, mode in ipairs({"occluded", "input_disabled", "actions_disabled", "online", "in_game"}) do
	local scenario = mode
	tests["model_b_" .. scenario .. "_preserves_navigation"] = function(production)
		local s = fixture(production, false, {create_lobby = true, ezz = true, zombies = true})
		local menu = s:lobby(4)
		s.used[2] = true
		local navigated = 0
		menu.button_entries = {{fn = function(_, actual_menu, c, model)
			assert(actual_menu == menu and c == 2 and model == s:button_model(2, "B"))
			navigated = navigated + 1
			return true
		end}}
		if scenario == "occluded" then menu.occludedBy = {} end
		if scenario == "input_disabled" then menu.m_inputDisabled = true end
		if scenario == "actions_disabled" then menu.m_disableAllButtonActions = true end
		if scenario == "online" then s.network = 2 end
		if scenario == "in_game" then s.in_game = true end
		local navigation_allowed = scenario == "online" or scenario == "in_game"
		s:notify(2, "B", 1)
		assert(#s.leaves == 0 and #s.calls == 0 and navigated == (navigation_allowed and 1 or 0))
		s:tick()
		assert(#s.leaves == 0)
		-- When an overlay/disable flag clears, stock mapped navigation still works.
		menu.occludedBy, menu.m_inputDisabled, menu.m_disableAllButtonActions = nil, false, false
		s.network = 2
		s:notify(2, "B", 0)
		s:notify(2, "B", 1)
		assert(navigated == (navigation_allowed and 2 or 1))
		assert(#s.leaves == 0 and #s.calls == 0 and s.used[2] and s.used[0])
	end
end

for _, gamepad in ipairs({true, false}) do
	local primary_gamepad = gamepad
	tests["ezz_remove_installed_after_production_" .. (gamepad and "chain" or "keyboard_host")] = function(production)
		local s = fixture(production, false, {create_lobby = true, zombies = true})
		local menu = s:lobby(1)
		s.used[2], s.queue_empty, s.primary_gamepad = true, false, primary_gamepad
		LobbyRemoveLocalClientFromLobby(menu, 2) -- Already queued before the external wrapper arrives.
		assert(#s.leaves == 0)
		s:install_ezz() -- Captures production's first wrapper, just like installed ezz.
		s:tick() -- Production now wraps ezz, which still points back to its OLD wrapper.
		local wrapped = LobbyRemoveLocalClientFromLobby
		assert(#s.leaves == 0)
		LobbyRemoveLocalClientFromLobby(menu, 2)
		assert(#s.leaves == 0 and s.ezz_removes == 0)
		s.unseat_on_remove, s.queue_empty = true, true
		s:tick()
		assert(#s.leaves == 1 and s.ezz_removes == 1 and not s.used[2])
		assert(s.leaves[1].controller == 2 and s.leaves[1].menu == menu)
		if not primary_gamepad then assert(CoD.perController[0].localClientChanged == true) end
		for _ = 1, 7 do s:tick() end
		assert(LobbyRemoveLocalClientFromLobby == wrapped and #s.leaves == 1)
		-- A second removal proves the late wrapper was not discarded by ticking.
		s.used[2] = true
		LobbyRemoveLocalClientFromLobby(menu, 2)
		assert(#s.leaves == 2 and s.ezz_removes == 2 and not s.used[2])
		assert(#s.calls == 0 and s.used[0])
	end
end

function tests.additional_prompt_offline_four_seat_capacity(production)
	local s = fixture(production, false, {additional_widget = true, additional_ezz = true})
	local menu = s:menu()
	local widget = CoD.FE_ListAdditonal.new(menu, 0)
	local condition = widget.stateConditions[3].condition
	-- The stock two-client controller-allowance predicate must not be consulted.
	CoD.LobbyBase.SplitscreenControllersAllowed = function() error("stock two-seat cap") end
	s.used[1], s.active[3] = true, true
	assert(condition(widget, menu, {play = false}) == true)
	s.network = 1
	assert(condition(widget, menu, {play = false}) == true)
	s.used[2], s.used[3] = true, true
	assert(condition(widget, menu, {play = true}) == false)
	s.used[3], s.active[3] = false, false
	assert(condition(widget, menu, {play = true}) == false)
	Engine.GetLobbyNetworkMode = nil
	s.stock_hint_waiting, s.stock_hint_allowed = true, true
	assert(condition(widget, menu, {play = true}) == true)
end

function tests.prompt_first_match_renders_stock_string_for_every_free_seat(production)
	local s = fixture(production, true, {additional_widget = true, additional_ezz = true})
	s.active[1], s.active[2], s.active[3] = true, true, true
	s.hide_prompt, s.map_prompt, s.plugin_prompt = true, true, true
	local menu = s:menu()
	local widget = CoD.FE_ListAdditonal.new(menu, 0)
	for c = 1, 3 do
		assert(widget.currentState == "AddControllerState_Play")
		assert(widget.StringA.alpha == 1 and widget.StringA.text == "PLATFORM_FEEDER_SECONDARY_CONTROLLER_PLAY")
		assert(widget.SplitscreenLobbyButtonPC.alpha == 0)
		s.used[c] = true
		s:tick() -- No artificial predicate invocation or external model refresh.
	end
	assert(widget.currentState == "Hide" and widget.StringA.alpha == 0)
	s.used[2], s.active[2] = false, false
	s:tick()
	assert(widget.currentState == "Hide") -- Empty seat without a connected pad.
	s.active[2] = true
	s:tick()
	assert(widget.currentState == "AddControllerState_Play" and widget.StringA.alpha == 1)
	s.network = 2
	s:tick()
	assert(widget.currentState == "Hide" and widget.StringA.alpha == 0) -- Original competing condition online.
	s.network, s.in_game = 0, true
	s:tick()
	assert(widget.currentState == "Hide")
	pcall = sandbox_pcall
end

for _, late_ezz in ipairs({false, true}) do
	for _, bound in ipairs({1, 4}) do
		local late, stock_bound = late_ezz, bound
		tests["four_players_one_a_each_" .. tostring(late) .. "_bound_" .. stock_bound] = function(production)
			local s = fixture(production, true, {toggle = true, create_lobby = true, ezz = not late,
				additional_widget = true, additional_ezz = true})
			if late then s:install_ezz(); s:tick() end
			local menu = s:lobby(stock_bound)
			local other = s:lobby(stock_bound) -- Duplicate active root subscribers.
			for c = 1, 3 do s.active[c] = true end
			local widget = CoD.FE_ListAdditonal.new(menu, 0)
			local selected = 0
			menu.button_entries = {{fn = function() selected = selected + 1; return true end}}
			other.button_entries = menu.button_entries
			s.seat_on_add = true
			s.on_add = function(actual, c)
				assert(actual == menu)
				-- A synchronous model write is a real nested release notification.
				Engine.SetModelValue(s:button_model(c, "A"), 0)
				LobbySplitscreenToggle(other, c)
				LobbySplitscreenToggle(other, 0)
			end
			for c = 1, 3 do
				assert(widget.StringA.alpha == 1)
				s:notify(c, "A", 3)
				assert(#s.calls == c - 1 and not s.used[c] and selected == 0)
				s:notify(c, "A", 1) -- Held DOWN flag transition still cannot seat/select.
				for _ = 1, 7 do s:tick() end -- Ownership outlives reservation timeout.
				s:notify(c, "A", 2) -- Release can carry non-DOWN flags.
				assert(#s.calls == c and s.calls[c].controller == c and s.used[c])
				s:notify(c, "A", 0)
				s:tick()
				assert(#s.calls == c and selected == 0 and #s.leaves == 0 and #s.toggles == 0)
			end
			assert(Engine.GetUsedControllerCount() == 4 and widget.StringA.alpha == 0)
			assert(s.notification_depth == 0)
			pcall = sandbox_pcall
		end
	end
end

function tests.existing_lobby_gets_all_guest_a_subscriptions_on_watcher(production)
	local s = fixture(production, false, {toggle = true, create_lobby = true})
	-- Simulate a menu built before the constructor could be wrapped: no stock
	-- or ezz subscriptions for guests, no HandleButtonPress to lazily attach us.
	local menu = s:menu()
	assert(#s:button_model(3, "A").subscribers == 0)
	s:tick()
	for c = 1, 3 do assert(#s:button_model(c, "A").subscribers == 1) end
	s.seat_on_add = true
	s:notify(3, "A", 1)
	assert(#s.calls == 0)
	s:notify(3, "A", 0)
	assert(#s.calls == 1 and s.calls[1].menu == menu and s.used[3])
	s:tick()
	assert(#s:button_model(3, "A").subscribers == 1)
end

function tests.physical_release_busy_queue_acquires_files_then_retries_exact_guest(production)
	local s = fixture(production, false, {toggle = true, create_lobby = true, ezz = true})
	local menu = s:lobby(1)
	s.queue_empty = false
	local ready = false
	Engine.AreLocalFilesReady = function(c) assert(c == 2); return ready end
	s:notify(2, "A", 1)
	s:notify(2, "A", 0)
	assert(#s.calls == 0 and #s.ezz_releases == 0)
	s:tick()
	assert(#s.calls == 0)
	s.queue_empty = true
	s:tick()
	assert(#s.calls == 1 and s.calls[1].controller == 2 and s.calls[1].menu == menu)
	s:tick()
	assert(#s.calls == 1) -- Do not waste retry budget until files are ready.
	ready, s.seat_on_add = true, true
	s:tick()
	assert(#s.calls == 2 and s.used[2] and not s.used[1] and not s.used[3])
end

function tests.busy_physical_join_is_context_bound_and_expires(production)
	for _, scenario in ipairs({"closed", "online", "map", "in_game", "expiry", "nav", "pad", "lan"}) do
		local s = fixture(production, false, {toggle = true, create_lobby = true, ezz = true})
		local menu = s:lobby(4)
		s.queue_empty = false
		s:notify(3, "A", 1)
		s:notify(3, "A", 0)
		if scenario == "closed" then menu.parent = nil end
		if scenario == "online" then s.network = 2 end
		if scenario == "map" then Engine.GetCurrentMap = function() return "zm_test" end end
		if scenario == "in_game" then s.in_game = true end
		if scenario == "nav" then s.global_model.children['lobbyRoot.lobbyNav'].value = 200 end
		if scenario == "pad" then s.ports[3] = 2 end
		if scenario == "lan" then s.network = 1 end
		for _ = 1, 6 do s:tick() end
		s.queue_empty, s.network, s.in_game, menu.parent = true, 0, false, LUI.roots.UIRootFull
		Engine.GetCurrentMap = function() return "core_frontend" end
		s:tick()
		assert(#s.calls == 0, scenario)
	end
end

function tests.held_a_cannot_join_after_context_change_or_select_after_seating(production)
	for _, scenario in ipairs({"nav", "pad", "online", "occluded", "input", "actions", "closed"}) do
		local s = fixture(production, false, {toggle = true, create_lobby = true, ezz = true})
		local menu = s:lobby(4)
		s:notify(2, "A", 1)
		if scenario == "nav" then s.global_model.children['lobbyRoot.lobbyNav'].value = 200 end
		if scenario == "pad" then s.ports[2] = 3 end
		if scenario == "online" then s.network = 2 end
		if scenario == "occluded" then menu.occludedBy = {} end
		if scenario == "input" then menu.m_inputDisabled = true end
		if scenario == "actions" then menu.m_disableAllButtonActions = true end
		if scenario == "closed" then menu.parent = nil end
		s:notify(2, "A", 0)
		assert(#s.calls == 0, scenario)
	end
	local s = fixture(production, false, {toggle = true, create_lobby = true, ezz = true})
	local menu = s:lobby(4)
	local selects = 0
	menu.button_entries = {{fn = function() selects = selects + 1; return true end}}
	s.seat_on_add = true
	s:notify(2, "A", 1)
	assert(#s.calls == 0 and selects == 0)
	s:notify(2, "A", 0)
	assert(s.used[2] and #s.calls == 1 and selects == 0)
	s:notify(2, "A", 1) -- A genuinely new seated press must retain stock select.
	assert(selects == 1 and #s.calls == 1 and s.notification_depth == 0)
end

function tests.join_retries_exact_request_after_files_and_loading_overlay(production)
	local s = fixture(production, false, {toggle = true})
	local menu, other = s:menu(), s:menu()
	local ready = false
	Engine.AreLocalFilesReady = function(c) assert(c == 3); return ready end
	assert(LobbyAddLocalClient(menu, 3) == "stock-add-result")
	assert(#s.calls == 1) -- Initial stock call must acquire files even while unready.
	s:tick()
	assert(#s.calls == 1)
	ready, menu.occludedBy, menu.m_inputDisabled = true, {}, true
	s:tick()
	assert(#s.calls == 1)
	menu.occludedBy, menu.m_inputDisabled, s.queue_empty = nil, false, false
	s:tick()
	assert(#s.calls == 1)
	s.queue_empty, s.seat_on_add, s.active[1] = true, true, true
	s.on_add = function(actual, c)
		assert(actual == menu and c == 3)
		LobbyAddLocalClient(other, 2)
		LobbySplitscreenToggle(other, 0)
	end
	s:tick()
	assert(#s.calls == 2 and s.used[3] and not s.used[1] and not s.used[2])
	assert(s.button_updates >= 2)
	LobbySplitscreenToggle(other, 0) -- Keep reservation after synchronous seating.
	s:tick()
	assert(#s.calls == 2)
	for _, call in ipairs(s.calls) do assert(call.menu == menu and call.controller == 3) end
end

function tests.join_retry_budget_and_immutable_stock_callback(production)
	local s = fixture(production)
	local menu = s:menu()
	Engine.AreLocalFilesReady = function(c) assert(c == 2); return true end
	LobbyAddLocalClient(menu, 2)
	LobbyAddLocalClient = function() error("retry must use captured stock callback") end
	for _ = 1, 8 do s:tick() end
	assert(#s.calls == 3)
	for _, call in ipairs(s.calls) do assert(call.menu == menu and call.controller == 2) end
end

function tests.join_retry_waits_do_not_extend_deadline(production)
	for _, blocker in ipairs({"files", "queue", "overlay", "input", "actions"}) do
		local s = fixture(production)
		local menu = s:menu()
		Engine.AreLocalFilesReady = function() return blocker ~= "files" end
		LobbyAddLocalClient(menu, 1)
		if blocker == "queue" then s.queue_empty = false end
		if blocker == "overlay" then menu.occludedBy = {} end
		if blocker == "input" then menu.m_inputDisabled = true end
		if blocker == "actions" then menu.m_disableAllButtonActions = true end
		for _ = 1, 5 do
			s:tick()
			LobbyAddLocalClient(s:menu(), 1) -- Dedup must not replace menu/deadline.
			assert(#s.calls == 1)
		end
		Engine.AreLocalFilesReady = function() return true end
		s.queue_empty, menu.occludedBy = true, nil
		menu.m_inputDisabled, menu.m_disableAllButtonActions = false, false
		for _ = 1, 3 do s:tick() end
		assert(#s.calls == 1)
	end
end

function tests.join_retry_cancels_unsafe_context_and_leave(production)
	for _, scenario in ipairs({"capacity", "online", "unknown_network", "missing_network",
		"in_game", "map", "unknown_menu", "closed", "leave", "seated"}) do
		local s = fixture(production)
		local menu = s:menu()
		Engine.AreLocalFilesReady = function() return true end
		LobbyAddLocalClient(menu, 2)
		if scenario == "capacity" then s.max = 1 end
		if scenario == "online" then s.network = 2 end
		if scenario == "unknown_network" then s.network = 7 end
		if scenario == "missing_network" then Engine.GetLobbyNetworkMode = nil end
		if scenario == "in_game" then s.in_game = true end
		if scenario == "map" then Engine.GetCurrentMap = function() return "zm_test" end end
		if scenario == "unknown_menu" then menu.menuName = nil end
		if scenario == "closed" then menu.getParent = function() return nil end end
		if scenario == "leave" or scenario == "seated" then s.used[2] = true end
		if scenario == "leave" then LobbyRemoveLocalClientFromLobby(menu, 2) end
		s:tick()
		s.max, s.network, s.in_game, s.used[2] = 4, 0, false, false
		Engine.GetLobbyNetworkMode = function() return s.network end
		Engine.GetCurrentMap = function() return "core_frontend" end
		menu.menuName, menu.getParent = "Lobby", function() return LUI.roots.UIRootFull end
		for _ = 1, 7 do s:tick() end
		assert(#s.calls == 1, scenario)
	end
end

function tests.join_retry_optional_map_and_attached_menu(production)
	local s = fixture(production)
	local menu = s:menu()
	menu.getParent = function() return LUI.roots.UIRootFull end
	Engine.GetCurrentMap = nil
	Engine.AreLocalFilesReady = function() return true end
	LobbyAddLocalClient(menu, 1)
	s.seat_on_add = true
	s:tick()
	assert(#s.calls == 2 and s.used[1])
end

return tests