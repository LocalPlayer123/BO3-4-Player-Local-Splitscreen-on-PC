-- Execute the complete production script with stock-shaped UI doubles, no game,
-- filesystem, pcall, io, os, or native name persistence implementation.
local tests = {}
-- ezz src/client/game/structs/lua/core.hpp: KeyboardType::SERVER_FILTER_KEYWORDS
-- is 40 (7 is TWITCH_USER). Production resolves the engine enum, never a custom
-- keyboard type or a hard-coded numeric fallback.
local KEYBOARD_TYPE = 40
local function fixture(production, options)
    options = options or {}
    local s = {dvars = {splitscreen_names_status = "ready"}, writes = {}, timers = {},
        network = 0, map = "core_frontend", inGame = false, freed = 0, original = 0,
        subscriptions = 0, menus = {}, commands = {}, popups = {}, queued = {}, buttonUpdates = {}}
    for i = 1, 4 do s.dvars["splitscreen_name_" .. i] = "Saved" .. i end
    local function noop() end
    local function node()
        local n = {children = {}, handlers = {}, callbacks = {}, closes = 0, refreshes = 0}
        for _, name in ipairs({"setOwner", "setRGB", "setTTF", "setLineSpacing",
            "setAlignment", "makeFocusable", "setWidgetType", "setVerticalCount", "setModel"}) do n[name] = noop end
        function n:setLeftRight(a, b, left, right) self.horizontal = {a, b, left, right} end
        function n:setTopBottom(a, b, top, bottom) self.vertical = {a, b, top, bottom} end
        function n:setAlpha(value) self.alpha = value end
        function n:setText(value) self.text = value end
        function n:addElement(child) self.children[#self.children + 1] = child end
        function n:close()
            self.closes = self.closes + 1
            for _, subscription in ipairs(self.links or {}) do
                if not subscription.closed then subscription.closed = true; s.subscriptions = s.subscriptions - 1 end
            end
        end
        function n:registerEventHandler(name, handler) self.handlers[name] = handler end
        function n:processEvent(event)
            if event.name == "gain_focus" then self.focused = true end
            if self.handlers[event.name] then return self.handlers[event.name](self, event) end
        end
        function n:AddButtonCallbackFunction(element, controller, button, key, action, label)
            self.callbacks[button] = {element = element, action = action, label = label}
        end
        function n:restoreState() return false end
        function n:getModel(controller, key) return self.models[key] end
        function n:linkToElementModel(list, key, initial, callback)
            self.links = self.links or {}
            self.links[#self.links + 1] = {callback = callback}
            s.subscriptions = s.subscriptions + 1
        end
        function n:setDataSource(source) self.source = source; self:updateDataSource() end
        function n:updateDataSource()
            self.refreshes = self.refreshes + 1
            self.rows = DataSources[self.source].build(0)
            self.items = {}
            for i, row in ipairs(self.rows) do
                local item = node()
                item.models = {}
                for key, value in pairs(row.models) do item.models[key] = {value = value} end
                item.action = row.properties.action
                self.items[i] = item
            end
            self.activeWidget = self.items[1]
        end
        function n:findItem(values, properties, select)
            for _, item in ipairs(self.items) do
                if item.models.slotId.value == values.slotId then
                    if select then self.activeWidget = item end
                    return item
                end
            end
        end
        return n
    end
    Engine = {
        GetCurrentMap = function() return s.map end,
        GetLobbyNetworkMode = function() return s.network end,
        GetPrimaryController = function() return 0 end,
        IsInGame = function() return s.inGame end,
        DvarString = function(controller, key) return s.dvars[key] end,
        SetDvar = function(key, value)
            assert(type(value) == "string", "all names and resets must remain strings")
            s.writes[#s.writes + 1] = {key = key, value = value}
            s.dvars[key] = value
        end,
        GetModelForController = function(controller) return controller end,
        CreateModel = function(parent, key) return {parent = parent, key = key} end,
        GetModelValue = function(model) return model.value end,
        UnsubscribeAndFreeModel = function(model)
            assert(not model.freed, "buttonModel double free")
            model.freed = true; s.freed = s.freed + 1
        end,
        Exec = function(controller, command)
            assert(command == "ui_keyboard_new 40" or command == "ui_keyboard_complete"
                or command == "ui_keyboard_cancel", "only verified stock command shapes are supported")
            s.commands[#s.commands + 1] = {controller = controller, command = command}
            if command == "ui_keyboard_new 40" and s.nativeKeyboard then
                if s.deferOverlay then
                    s.queued[#s.queued + 1] = {controller = controller, open = true}
                else
                    -- Engine initialization may replace scratch before constructing stock UI.
                    s.dvars.ui_keyboard_dvar_edit = ""
                    s.overlay = LUI.createMenu.KeyboardInputOverlay(controller)
                end
            elseif command == "ui_keyboard_complete" or command == "ui_keyboard_cancel" then
                -- Exec queues commands, not a synchronous ui_keyboard_input.
                -- Snapshot here models an eventual engine event. Late events
                -- intentionally carry no settings request ID.
                s.queued[#s.queued + 1] = {controller = controller,
                    input = s.dvars.ui_keyboard_dvar_edit,
                    cancelled = command == "ui_keyboard_cancel"}
            end
        end
    }
    Enum = {LobbyNetworkMode = {LOBBY_NETWORKMODE_LAN = 0, LOBBY_NETWORKMODE_LOCAL = 1,
        LOBBY_NETWORKMODE_LIVE = 2}, KeyboardType = {KEYBOARD_TYPE_SERVER_FILTER_KEYWORDS = KEYBOARD_TYPE},
        LUIButton = {LUI_KEY_XBA_PSCROSS = "A", LUI_KEY_XBB_PSCIRCLE = "B"},
        LUIAlignment = {LUI_ALIGNMENT_LEFT = 1, LUI_ALIGNMENT_TOP = 2}}
    LUI = {roots = {UIRootFull = node(), UIRoot0 = node()}, createMenu = {}, UIElement = {new = node},
        UIImage = {new = node}, UIText = {new = node}, UIList = {new = node}, UITimer = {}}
    LUI.UITimer.newElementTimer = function(interval, once, callback)
        assert(not once)
        local timer = node()
        timer.interval = interval; timer.callback = callback
        s.timers[#s.timers + 1] = timer
        return timer
    end
    LUI.OverrideFunction_CallOriginalSecond = function(element, key, callback)
        local original = element[key]
        element[key] = function(self, ...) original(self, ...); callback(self, ...) end
    end
    CoD = {Menu = {NewForUIEditor = function(name)
        local menu = node(); menu.menuName = name
        s.menus[#s.menus + 1] = menu
        return menu
    end, SetButtonLabel = noop}, GenericMenuFrame = {new = function()
        local frame = node()
        if not options.noTitle then frame.titleLabel = node() end
        return frame
    end}, List1ButtonLarge_PH = {}, LobbyMenus = {}}
    CoD.InputTextField = {new = function()
        local field = node()
        field.prefill = s.dvars.ui_keyboard_dvar_edit
        return field
    end}
    LUI.createMenu.KeyboardInputOverlay = function(controller)
        local overlay = node()
        overlay.field = CoD.InputTextField.new(overlay, controller)
        return overlay
    end
    UIKeyboardComplete = function(element, controller)
        Engine.Exec(controller, "ui_keyboard_complete")
        return "stock-complete"
    end
    UIKeyboardCancel = function(element, controller)
        Engine.Exec(controller, "ui_keyboard_cancel")
    end
    require = function(module)
        assert(module == "ui.uieditor.widgets.BackgroundFrames.GenericMenuFrame"
            or module == "ui.uieditor.widgets.Lobby.Common.List1ButtonLarge_PH"
            or module == "ui.uieditor.widgets.PC.Utility.InputTextField"
            or module == "ui.uieditor.menus.PC.KeyboardInputOverlay", "unexpected require")
    end
    CoD.LobbyMenus.AddButtonsForTarget = function(controller, id)
        s.original = s.original + 1
        assert(s.original < 50, "recursive wrapper")
        return s.result or {{optionDisplay = "STOCK"}}
    end
    LobbyData = {UITargets = {UI_MODESELECT = {id = 1, name = "ModeSelect"},
        UI_ZMLOBBYLANGAME = {id = 410, name = "ZMLobbyLANGame"},
        UI_MPLOBBYLANGAME = {id = 310, name = "MPLobbyLANGame"},
        UI_CPLOBBYLANGAME = {id = 110, name = "CPLobbyLANGame"},
        UI_CPLOBBYLANCUSTOMGAME = {id = 111, name = "CPLobbyLANCustomGame"},
        UI_ZMLOBBYONLINE = {id = 400, name = "ZMLobbyOnline"}}}
    DataSources = {}
    ListHelper_SetupDataSource = function(name, build) return {build = build} end
    -- Verbatim contract of stock actions.lua ShowKeyboard, not a guessed engine API.
    ShowKeyboard = function(menu, element, controller, keyboardName)
        Engine.Exec(controller, "ui_keyboard_new " .. Enum.KeyboardType[keyboardName])
    end
    ProcessListAction = function(menu, item, controller) item.action(menu, item, controller) end
    OpenPopup = function(parent, name, controller)
        s.popups[#s.popups + 1] = {parent = parent, name = name, controller = controller}
    end
    GoBack = function(menu) menu:close() end
    ForceLobbyButtonUpdate = function(controller)
        assert(controller == 0)
        s.buttonUpdates[#s.buttonUpdates + 1] = controller
    end
    __zz_splitscreen_settings_loaded = nil
    __zz_splitscreen_refresh_names = function() s.nameRefreshes = (s.nameRefreshes or 0) + 1 end
    pcall = nil; io = nil; os = nil
    if options.late then s.lobbyMenus = CoD.LobbyMenus; CoD.LobbyMenus = nil end
    if options.noIsInGame then Engine.IsInGame = nil end
    if options.noNetworkEnum then Enum.LobbyNetworkMode = nil end
    if options.lateFullRoot then LUI.roots.UIRootFull = nil end
    production()
    function s:addFullRoot()
        assert(LUI.roots.UIRootFull == nil)
        LUI.roots.UIRootFull = node()
        return LUI.roots.UIRootFull
    end
    function s:tick(interval)
        for _, timer in ipairs(self.timers) do
            if timer.interval == interval and timer.closes == 0 then timer.callback() end
        end
    end
    function s:open()
        self.menu = LUI.createMenu.SplitscreenSettings(0)
        return self.menu
    end
    function s:choose(index, controller)
        local menu = self.menu
        CoD.isPC = self.nativeKeyboard == true
        menu.Options.activeWidget = menu.Options.items[index]
        local callback = menu.callbacks.A
        return callback.action(menu.Options.activeWidget, menu, controller or 0)
    end
    function s:input(value, extra)
        local event = extra or {}
        event.name = "ui_keyboard_input"; event.input = value
        if event.type == nil then event.type = KEYBOARD_TYPE end
        return self.menu:processEvent(event)
    end
    function s:drain()
        local queued = self.queued
        self.queued = {}
        for _, event in ipairs(queued) do
            if event.open then
                self.dvars.ui_keyboard_dvar_edit = ""
                self.overlay = LUI.createMenu.KeyboardInputOverlay(event.controller)
            else
                self:input(event.input, event)
            end
        end
    end
    function s:nameWrites()
        local count = 0
        for _, write in ipairs(self.writes) do if string.match(write.key, "^splitscreen_name_%d$") then count = count + 1 end end
        return count
    end
    return s
end

function tests.readable_text_and_close_refresh(production)
    local s = fixture(production); local menu = s:open()
    assert(menu.children[1].alpha == 1, "settings background must be opaque")
    local textCount = 0
    for _, child in ipairs(menu.children) do
        if child.text and child.vertical then
            local height = child.vertical[4] - child.vertical[3]
            assert(height >= 18 and height <= 36, "UIText height is font size, not paragraph height")
            assert(child.vertical[3] >= 48 and child.vertical[4] <= 640, "text outside safe area")
            textCount = textCount + 1
        end
    end
    assert(textCount >= 7)
    menu:close(); assert(s.nameRefreshes == 1)
    menu:close(); assert(s.nameRefreshes == 1, "duplicate close must not schedule another refresh")
    s:open(); __zz_splitscreen_refresh_names = nil; s.menu:close()
end

function tests.four_slots_prefill_and_labels(production)
    local s = fixture(production); local menu = s:open()
    assert(#menu.Options.rows == 6 and menu.Options.focused)
    for slot = 1, 4 do
        assert(menu.Options.rows[slot].models.displayText == "Player " .. slot .. ": Saved" .. slot)
        s:choose(slot)
        assert(s.dvars.ui_keyboard_dvar_edit == "Saved" .. slot)
        s:input("New" .. slot)
        assert(s.dvars["splitscreen_name_" .. slot] == "New" .. slot)
        assert(menu.Options.rows[slot].models.displayText == "Player " .. slot .. ": New" .. slot)
        assert(menu.Options.activeWidget.models.slotId.value == slot)
    end
    assert(s:nameWrites() == 4 and #s.commands == 4)
end

function tests.wait_for_native_publish(production)
    local s = fixture(production); s.dvars.splitscreen_names_status = nil; s:open()
    for _, status in ipairs({"", "load_error", "dvar_error", "hook_error", "unknown"}) do
        s.dvars.splitscreen_names_status = status
        s:tick(250)
        for i = 1, 6 do s:choose(i) end
    end
    assert(#s.writes == 0 and #s.commands == 0)
    s.dvars.splitscreen_name_4 = "Loaded Guest"
    s.dvars.splitscreen_names_status = "ready"; s:tick(250)
    assert(s.menu.Options.rows[4].models.displayText == "Player 4: Loaded Guest")
    s:choose(4); s:input("Guest"); assert(s:nameWrites() == 1)
end

function tests.presets_and_reset(production)
    local s = fixture(production); s:open(); s:choose(5)
    for i = 1, 4 do assert(s.dvars["splitscreen_name_" .. i] == "Player " .. i) end
    s:choose(6)
    for i = 1, 4 do
        assert(s.dvars["splitscreen_name_" .. i] == "")
        assert(s.menu.Options.rows[i].models.displayText == "Player " .. i .. ": <engine default>")
    end
    assert(s:nameWrites() == 8)
end

function tests.cancel_is_not_empty(production)
    local s = fixture(production); s:open()
    for _, event in ipairs({{}, {cancelled = true}, {canceled = true}}) do
        s:choose(2)
        local value = nil
        if event.cancelled or event.canceled then value = "" end
        s:input(value, event)
    end
    assert(s:nameWrites() == 0 and s.dvars.splitscreen_name_2 == "Saved2")
    s:choose(2); s:input("")
    assert(s:nameWrites() == 1 and s.dvars.splitscreen_name_2 == "")
    s:input("duplicate"); assert(s:nameWrites() == 1)
end

function tests.invalid_and_utf8_boundaries(production)
    local s = fixture(production); s:open()
    for _, value in ipairs({'bad"name', "bad;name", "bad\\name", "bad/name", "line\nfeed", "tab\tname",
        string.char(0), string.char(127), string.rep("a", 32), string.rep("é", 16),
        string.char(192, 175), string.char(128), string.char(237, 160, 128),
        string.char(244, 144, 128, 128), string.char(226, 130), "\194\128", "\226\128\168", "\226\128\169"}) do
        s:choose(3); s:input(value)
        assert(s:nameWrites() == 0, "accepted unsafe name")
        assert(string.find(s.menu.Notice.text, "Invalid name", 1, true))
    end
    for _, value in ipairs({string.rep("a", 31), string.rep("é", 15) .. "a", "Couch 🎮", "O'Brien"}) do
        s:choose(3); s:input(value); assert(s.dvars.splitscreen_name_3 == value)
    end
    assert(s:nameWrites() == 4)
end

function tests.keyboard_ownership_and_stale_events(production)
    local s = fixture(production); s:open(); s:choose(4, 2)
    s:choose(1); s:choose(5); s:choose(6)
    assert(#s.commands == 1 and s:nameWrites() == 0)
    assert(not s:input("Wrong", {controller = 0}))
    assert(not s:input("Wrong", {controller = 2, type = 123}))
    s:input("Right", {controller = 2})
    assert(s.dvars.splitscreen_name_4 == "Right" and s:nameWrites() == 1)
    s:choose(1); s.menu:close(); s:input("Late")
    assert(s:nameWrites() == 1)
    s:open(); s:choose(1); assert(#s.commands == 3)
end

function tests.offline_frontend_guards(production)
    local s = fixture(production); s:open()
    for _, mode in ipairs({2, 99}) do
        s.network = mode
        assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 1) == 1)
        s:choose(5); s:choose(1); assert(LUI.createMenu.SplitscreenSettings(0) == nil)
    end
    s.network = 0; s:choose(1); s.network = 2; s:input("Nope")
    s.network = 0; s.map = "zm_test"; s:choose(5)
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 410) == 1)
    s.map = "core_frontend"; s.inGame = true; s:choose(5); s:tick(250)
    assert(string.find(s.menu.Status.text, "OFFLINE FRONTEND ONLY", 1, true))
    assert(s:nameWrites() == 0)
end

function tests.missing_optional_is_in_game_visible_and_editable(production)
    local s = fixture(production, {noIsInGame = true})
    for _, mode in ipairs({0, 1}) do
        s.network = mode
        assert(s.map == "core_frontend" and Engine.IsInGame == nil)
        assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 1) == 2)
        s:open(); s:choose(1); s:input("Offline " .. mode); s.menu:close()
        assert(s.dvars.splitscreen_name_1 == "Offline " .. mode)
    end
    assert(s:nameWrites() == 2)
    s.map = "zm_test"
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 410) == 1)
    assert(LUI.createMenu.SplitscreenSettings(0) == nil)
end

function tests.numeric_offline_modes_independent_of_network_enum(production)
    local s = fixture(production, {noNetworkEnum = true})
    for _, enums in ipairs({false, {LOBBY_NETWORKMODE_LAN = 99, LOBBY_NETWORKMODE_LOCAL = 98}}) do
        Enum.LobbyNetworkMode = enums or nil
        for _, mode in ipairs({0, 1}) do
            s.network = mode
            assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 410) == 2)
            s:open(); s:choose(5); s.menu:close()
        end
        for _, mode in ipairs({2, 98, 99, "0", "1"}) do
            s.network = mode
            assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 410) == 1)
            assert(LUI.createMenu.SplitscreenSettings(0) == nil)
        end
    end
    assert(s:nameWrites() == 16)
end

function tests.required_offline_apis_fail_closed(production)
    local s = fixture(production); s:open()
    for _, key in ipairs({"GetCurrentMap", "GetLobbyNetworkMode"}) do
        local original = Engine[key]; Engine[key] = nil
        assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 1) == 1)
        assert(LUI.createMenu.SplitscreenSettings(0) == nil)
        s:choose(1); s:choose(5); s:choose(6)
        Engine[key] = original
    end
    s.network = nil
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 1) == 1)
    s:choose(5)
    assert(s:nameWrites() == 0 and #s.commands == 0)
end

function tests.buttons_targets_no_duplicates(production)
    local s = fixture(production)
    for _, id in ipairs({1, 410, 310, 110, 111}) do
        local result = CoD.LobbyMenus.AddButtonsForTarget(0, id)
        assert(#result == 2 and result[1].optionDisplay == "SPLITSCREEN SETTINGS")
        assert(result[2].optionDisplay == "STOCK" and result[1].customId)
        result[1].action({}, nil, 0, nil, "parent")
        assert(s.popups[#s.popups].parent == "parent")
        s.result = result
        assert(CoD.LobbyMenus.AddButtonsForTarget(0, id) == result and #result == 2)
        s.result = nil
    end
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 400) == 1)
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 999) == 1)
    -- Resolve changed IDs only through known keys, never by scanning names.
    LobbyData.UITargets = {UI_ZMLOBBYLANGAME = {id = 987, name = "RenamedLobby"},
        renamed = {id = 986, name = "ZMLobbyLANGame"}}
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 987) == 2)
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 986) == 1)
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 410) == 1)
    local result = CoD.LobbyMenus.AddButtonsForTarget(0, 987)
    s.network = 2; result[1].action({}, nil, 0)
    assert(#s.popups == 5)
end

function tests.client_backed_targets_without_enumerated_keys(production)
    fixture(production)
    local keys = {"UI_MODESELECT", "UI_ZMLOBBYLANGAME", "UI_MPLOBBYLANGAME",
        "UI_CPLOBBYLANGAME", "UI_CPLOBBYLANCUSTOMGAME"}
    for _, numeric in ipairs({false, true}) do
        local values, lookups = {}, {}
        for i, key in ipairs(keys) do
            values[key] = numeric and (900 + i) or {id = 900 + i}
        end
        values.UI_ZMLOBBYONLINE = {id = 990}
        values.renamed = {id = 991, name = "ZMLobbyLANGame"}
        LobbyData.UITargets = setmetatable({}, {__index = function(_, key)
            lookups[key] = (lookups[key] or 0) + 1
            return values[key]
        end})
        assert(next(LobbyData.UITargets) == nil)
        for _ in pairs(LobbyData.UITargets) do error("proxy exposes enumerated keys") end
        for i, key in ipairs(keys) do
            local result = CoD.LobbyMenus.AddButtonsForTarget(0, 900 + i)
            assert(#result == 2 and result[1].optionDisplay == "SPLITSCREEN SETTINGS")
            assert(result[2].optionDisplay == "STOCK" and lookups[key] > 0)
        end
        for _, id in ipairs({1, 410, 990, 991, 999}) do
            assert(#CoD.LobbyMenus.AddButtonsForTarget(0, id) == 1)
        end
        assert(lookups.UI_ZMLOBBYONLINE == nil and lookups.renamed == nil)
        assert(next(LobbyData.UITargets) == nil, "lookup must not populate the proxy")
    end
end

function tests.settings_first_before_ten_stock_rows_preserves_rows_on_reuse(production)
    local s = fixture(production)
    local original, snapshots = {}, {}
    s.result = {}
    for i = 1, 10 do
        local row = {optionDisplay = i == 10 and "QUIT" or "STOCK " .. i,
            customId = "stock_" .. i, action = function() return i end,
            disabled = i % 2 == 0, properties = {stockIndex = i}}
        s.result[i] = row
        original[i] = row
        snapshots[i] = {}
        for key, value in pairs(row) do snapshots[i][key] = value end
    end
    local settings
    for _ = 1, 3 do
        local result = CoD.LobbyMenus.AddButtonsForTarget(0, 1)
        -- In-place insertion is intentional; only the stock row objects and
        -- their relative order must remain unchanged, not their array indices.
        assert(result == s.result and #result == 11)
        assert(result[1].optionDisplay == "SPLITSCREEN SETTINGS")
        assert(result[1].customId == "zz_splitscreen_settings")
        if settings then assert(result[1] == settings) else settings = result[1] end
        local count = 0
        for _, row in ipairs(result) do
            if row.customId == "zz_splitscreen_settings" or row.optionDisplay == "SPLITSCREEN SETTINGS" then
                count = count + 1
            end
        end
        assert(count == 1)
        for i, row in ipairs(original) do
            assert(result[i + 1] == row and row.action() == i and row.properties.stockIndex == i)
            for key, value in pairs(snapshots[i]) do assert(row[key] == value) end
            for key, value in pairs(row) do assert(snapshots[i][key] == value) end
        end
        assert(result[11].optionDisplay == "QUIT")
    end
end

function tests.existing_settings_either_identity_is_not_duplicated(production)
    local s = fixture(production)
    for _, existing in ipairs({{customId = "zz_splitscreen_settings", optionDisplay = "CUSTOM LABEL"},
        {optionDisplay = "SPLITSCREEN SETTINGS"}}) do
        local stock = {optionDisplay = "STOCK"}
        s.result = {stock, existing}
        for _ = 1, 3 do
            local result = CoD.LobbyMenus.AddButtonsForTarget(0, 1)
            assert(result == s.result and #result == 2)
            assert(result[1] == stock and result[2] == existing)
        end
    end
end

function tests.late_install_and_callback_replacement(production)
    local s = fixture(production, {late = true})
    assert(not LUI.createMenu.SplitscreenSettings)
    assert(#s.timers == 2 and #s.buttonUpdates == 0)
    CoD.LobbyMenus = s.lobbyMenus; s:tick(500)
    assert(#s.buttonUpdates == 1)
    local first = CoD.LobbyMenus.AddButtonsForTarget
    for i = 1, 20 do s:tick(500) end
    assert(first == CoD.LobbyMenus.AddButtonsForTarget)
    local external = 0
    CoD.LobbyMenus.AddButtonsForTarget = function(...)
        external = external + 1
        local result = first(...); result[#result + 1] = {optionDisplay = "EZZ"}; return result
    end
    s:tick(500)
    local replacement = CoD.LobbyMenus.AddButtonsForTarget
    assert(#s.buttonUpdates == 2)
    for i = 1, 20 do s:tick(500) end
    assert(replacement == CoD.LobbyMenus.AddButtonsForTarget)
    local result = replacement(0, 1)
    assert(#result == 3 and external == 1 and s.original == 1)
    assert(result[3].optionDisplay == "EZZ")
    production(); assert(#s.timers == 2 and #s.buttonUpdates == 2, "duplicate watcher or refresh")
    assert(#LUI.roots.UIRoot0.children == 1 and #LUI.roots.UIRootFull.children == 1)
end

function tests.late_full_root_attaches_once_and_rewraps(production)
    local s = fixture(production, {lateFullRoot = true})
    assert(#s.timers == 1 and #LUI.roots.UIRoot0.children == 1 and #s.buttonUpdates == 1)
    local full = s:addFullRoot()
    s:tick(500)
    assert(#full.children == 1 and #s.timers == 2 and #s.buttonUpdates == 1)
    local previous = CoD.LobbyMenus.AddButtonsForTarget
    CoD.LobbyMenus.AddButtonsForTarget = function(...) return previous(...) end
    -- Drive only the new full-root watcher, proving it independently installs.
    full.children[1].children[1].callback()
    local wrapped = CoD.LobbyMenus.AddButtonsForTarget
    assert(#s.buttonUpdates == 2 and wrapped ~= previous)
    assert(#wrapped(0, 1) == 2)
    for _ = 1, 20 do s:tick(500) end
    production()
    assert(CoD.LobbyMenus.AddButtonsForTarget == wrapped and #s.buttonUpdates == 2)
    assert(#s.timers == 2 and #full.children == 1 and #LUI.roots.UIRoot0.children == 1)
end

function tests.online_wrap_does_not_force_lobby_refresh(production)
    local s = fixture(production, {late = true})
    s.network = 2; CoD.LobbyMenus = s.lobbyMenus; s:tick(500)
    assert(#s.buttonUpdates == 0)
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 1) == 1)
    s.network = 1
    assert(#CoD.LobbyMenus.AddButtonsForTarget(0, 1) == 2)
end

function tests.status_focus_and_cleanup(production)
    local s = fixture(production, {noTitle = true}); local menu = s:open()
    menu.Options.activeWidget = menu.Options.items[4]
    local refreshes = menu.Options.refreshes
    for i = 1, 30 do s:tick(250) end
    assert(menu.Options.refreshes == refreshes and s.subscriptions == 1)
    for _, status in ipairs({"save_error", "load_error", "dvar_error", "hook_error"}) do
        s.dvars.splitscreen_names_status = status; s:tick(250)
        assert(string.find(menu.Status.text, "ERROR", 1, true))
        assert(menu.Options.activeWidget.models.slotId.value == 4)
    end
    s.dvars.splitscreen_names_status = "save_error"; s:choose(4); s:input("Unsaved")
    assert(s.dvars.splitscreen_name_4 == "Unsaved")
    assert(string.find(menu.Status.text, "NOT saved", 1, true))
    menu:close()
    assert(s.freed == 1 and s.subscriptions == 0)
    for _, timer in ipairs(s.timers) do if timer.interval == 250 then assert(timer.closes == 1) end end
    s:tick(250); menu:close(); assert(s.freed == 1)
    s:open(); assert(s.subscriptions == 1)
    s.menu.callbacks.B.action(nil, s.menu, 0)
    assert(s.freed == 2 and s.subscriptions == 0)
end

function tests.missing_keyboard_fails_closed(production)
    local s = fixture(production); s:open()
    Enum.KeyboardType.KEYBOARD_TYPE_SERVER_FILTER_KEYWORDS = nil
    Enum.KeyboardType.KEYBOARD_TYPE_EDIT_DVAR = 7 -- never fall back to a made-up enum
    s:choose(1)
    assert(#s.writes == 0 and #s.commands == 0)
    assert(string.find(s.menu.Notice.text, "unavailable", 1, true))
    Enum.KeyboardType = nil
    s:choose(2); assert(#s.writes == 0 and #s.commands == 0)
    Enum.KeyboardType = {KEYBOARD_TYPE_SERVER_FILTER_KEYWORDS = KEYBOARD_TYPE}; ShowKeyboard = nil
    s:choose(1); assert(#s.writes == 0)
    -- Presets do not require a keyboard.
    s:choose(5); assert(s:nameWrites() == 4)
end

function tests.stock_accept_cancel_without_input_event(production)
    local s = fixture(production); s.nativeKeyboard = true; s:open()
    for slot = 1, 4 do
        local controller = slot - 1
        s:choose(slot, controller)
        assert(s.commands[#s.commands].command == "ui_keyboard_new 40")
        assert(s.commands[#s.commands].controller == controller)
        assert(s.overlay.field.prefill == "Saved" .. slot, "engine initialization clobbered prefill")
        s.dvars.ui_keyboard_dvar_edit = "Abandoned"
        UIKeyboardCancel(s.overlay, controller); s.overlay:close()
        assert(s:nameWrites() == (slot - 1) * 2)
        s:choose(slot, controller)
        s:drain() -- prior cancel arrives after a NEW overlay was opened
        assert(s.overlay.field.prefill == "Saved" .. slot)
        s.dvars.ui_keyboard_dvar_edit = "Accepted " .. slot
        assert(UIKeyboardComplete(s.overlay, controller) == "stock-complete")
        s.overlay:close()
        assert(s:nameWrites() == (slot - 1) * 2 + 1)
        assert(s.dvars["splitscreen_name_" .. slot] == "Accepted " .. slot)
        s:choose(slot, controller)
        assert(s.overlay.field.prefill == "Accepted " .. slot)
        s:drain() -- prior accept must NOT write into this new request
        assert(s:nameWrites() == (slot - 1) * 2 + 1)
        s.dvars.ui_keyboard_dvar_edit = ""
        UIKeyboardComplete(s.overlay, controller); s.overlay:close(); s:drain()
        assert(s:nameWrites() == slot * 2 and s.dvars["splitscreen_name_" .. slot] == "")
    end
    s:choose(4); s.dvars.ui_keyboard_dvar_edit = "Closed only"
    s.overlay:close() -- close without either stock helper is cancellation too
    assert(s:nameWrites() == 8)
end

function tests.request_status_does_not_acknowledge_save(production)
    local s = fixture(production); s:open(); s:choose(5)
    assert(string.find(s.menu.Notice.text, "Changes requested", 1, true))
    assert(string.find(s.menu.Status.text, "cannot confirm disk saves", 1, true))
    -- The dvar writes are visible immediately, but ready is not a save ACK.
    for i = 1, 4 do s:tick(250) end
    assert(not string.find(s.menu.Status.text, "saved automatically", 1, true))
    assert(s.menu.Notice.text == "")
    s:choose(2); s:input("Requested name")
    assert(string.find(s.menu.Notice.text, "waiting for the native save poll", 1, true))
    s.dvars.splitscreen_names_status = "save_error"; s:tick(250)
    assert(string.find(s.menu.Status.text, "NOT saved", 1, true))
end

function tests.queued_open_ignores_prior_completion_before_overlay(production)
    local s = fixture(production); s.nativeKeyboard = true; s.deferOverlay = true
    s:open(); s:choose(1)
    assert(s.overlay == nil and s:nameWrites() == 0)
    s:drain()
    assert(s.overlay.field.prefill == "Saved1")
    s.dvars.ui_keyboard_dvar_edit = "First"
    UIKeyboardComplete(s.overlay, 0); s.overlay:close()
    s:choose(4)
    -- Both complete and new are queued: the old event arrives while the new
    -- request exists but its overlay has not yet been constructed.
    s:drain()
    assert(s:nameWrites() == 1 and s.dvars.splitscreen_name_4 == "Saved4")
    assert(s.overlay.field.prefill == "Saved4")
    s.dvars.ui_keyboard_dvar_edit = "Fourth"
    UIKeyboardComplete(s.overlay, 0); s.overlay:close(); s:drain()
    assert(s:nameWrites() == 2 and s.dvars.splitscreen_name_4 == "Fourth")
end

function tests.stock_keyboard_other_menus_unaffected(production)
    local s = fixture(production); s:open()
    local field = CoD.InputTextField.new({}, 0)
    assert(field.prefill == nil and #s.writes == 0)
    UIKeyboardComplete({}, 0)
    assert(s:nameWrites() == 0)
    s:choose(1)
    UIKeyboardComplete({}, 2)
    assert(s:nameWrites() == 0)
    s.dvars.ui_keyboard_dvar_edit = "Guest Edit"
    s:input("Host Edit")
    assert(s:nameWrites() == 1)
end

return tests
