-- Offline per-slot preferences; persistence belongs exclusively to the native bridge.
-- Stock API references: KingslayerKyle/T7LuaRepo, main, Dumps/PC_Ship_2025-05-31:
-- ui/uieditor/actions.lua (ShowKeyboard), menus/PC/KeyboardInputOverlay.lua,
-- widgets/PC/Utility/InputTextField.lua, ui/T6/lobby/lobbymenus.lua.
-- lua/Shared/LuaEnums.lua does NOT define KeyboardType: the engine registers it.
-- Never invent an enum, assume a direct Engine.ShowKeyboard signature, or write
-- names before the native load has published splitscreen_names_status.
if rawget(_G, "__zz_splitscreen_settings_loaded") then return end
if not Engine or not LUI or not LUI.UIElement or not LUI.UITimer
    or not LUI.UITimer.newElementTimer or not LUI.roots then return end
rawset(_G, "__zz_splitscreen_settings_loaded", true)

local MENU = "SplitscreenSettings"
local SOURCE = "SplitscreenSettingsNames"
local BUTTON = "zz_splitscreen_settings"
local function read(key)
    return Engine.DvarString and Engine.DvarString(nil, key) or ""
end
local function offline()
    if not Engine.GetCurrentMap or Engine.GetCurrentMap() ~= "core_frontend"
        or not Engine.GetLobbyNetworkMode or (Engine.IsInGame and Engine.IsInGame()) then return false end
    local mode = Engine.GetLobbyNetworkMode()
    -- Same verified LOCAL/LAN values as the native bridge and join script.
    -- Do not require optional engine enums just to display a frontend button.
    return mode == 0 or mode == 1
end
local function writable()
    local status = read("splitscreen_names_status")
    return offline() and (status == "ready" or status == "save_error") and Engine.SetDvar ~= nil
end

-- Strict UTF-8 validation, including overlongs, surrogates, and U+10FFFF.
-- Reject rather than silently changing the submitted name. Empty is a reset.
local function validName(value)
    if type(value) ~= "string" or #value > 31 then return false end
    local i = 1
    while i <= #value do
        local b = string.byte(value, i)
        if b < 128 then
            if b < 32 or b == 127 or b == 34 or b == 47 or b == 92 or b == 59 then return false end
            i = i + 1
        else
            local count = b >= 194 and b <= 223 and 2 or b >= 224 and b <= 239 and 3
                or b >= 240 and b <= 244 and 4 or 0
            if count == 0 or i + count - 1 > #value then return false end
            for offset = 1, count - 1 do
                local c = string.byte(value, i + offset)
                if c < 128 or c > 191 then return false end
            end
            local second = string.byte(value, i + 1)
            if (b == 224 and second < 160) or (b == 237 and second >= 160)
                or (b == 240 and second < 144) or (b == 244 and second >= 144) then return false end
            if (b == 194 and second <= 159) or (b == 226 and second == 128
                and (string.byte(value, i + 2) == 168 or string.byte(value, i + 2) == 169)) then return false end
            i = i + count
        end
    end
    return true
end

local messages = {
    ready = "READY | Native name poll enabled; this UI cannot confirm disk saves.",
    save_error = "SAVE ERROR | Names may work this session but are NOT saved. Native retry is automatic.",
    load_error = "LOAD ERROR | Saved file protected; editing disabled. Check the native log.",
    dvar_error = "DVAR ERROR | Name bridge unavailable; editing disabled.",
    hook_error = "HOOK ERROR | Name overrides unavailable; editing disabled."
}
local pending = nil -- one stock keyboard across all settings menus/controllers
local function keyboardType()
    return Enum and Enum.KeyboardType and Enum.KeyboardType.KEYBOARD_TYPE_SERVER_FILTER_KEYWORDS
end
local function edit(menu, element, controller, slot)
    if not writable() or pending or menu.zzSettingsClosed then return end
    if keyboardType() == nil or not ShowKeyboard then
        menu.notice = "Stock text keyboard unavailable; no names changed."
        return
    end
    pending = {menu = menu, controller = controller, slot = slot,
        prefill = read("splitscreen_name_" .. slot), pcKeyboard = CoD and CoD.isPC}
    -- Only the stock keyboard scratch buffer is used, never a name dvar. Stock
    -- ShowKeyboard takes exactly (menu, element, controller, enum-name).
    Engine.SetDvar("ui_keyboard_dvar_edit", pending.prefill)
    ShowKeyboard(menu, element, controller, "KEYBOARD_TYPE_SERVER_FILTER_KEYWORDS")
end

local function preset(reset)
    return function(menu)
        if not writable() or pending or menu.zzSettingsClosed then return end
        menu.notice = "Changes requested; waiting for the native save poll."
        for slot = 1, 4 do
            Engine.SetDvar("splitscreen_name_" .. slot, reset and "" or "Player " .. slot)
        end
    end
end

local function rows()
    local result = {}
    for slot = 1, 4 do
        local index = slot
        local value = read("splitscreen_name_" .. slot)
        result[#result + 1] = {
            models = {displayText = "Player " .. slot .. ": " .. (value == "" and "<engine default>" or value),
                description = "Edit this slot even when its guest is absent. Empty input restores the engine name.",
                slotId = slot},
            properties = {action = function(menu, element, controller) edit(menu, element, controller, index) end}
        }
    end
    result[5] = {models = {displayText = "PRESET: PLAYER 1 - PLAYER 4", slotId = 5,
        description = "Set all four slots to Player 1, Player 2, Player 3, Player 4."}, properties = {action = preset(false)}}
    result[6] = {models = {displayText = "RESET: ENGINE ORIGINAL NAMES", slotId = 6,
        description = "Clear every override. The engine supplies original names, including absent guests when they join."},
        properties = {action = preset(true)}}
    return result
end

local installKeyboard
local function createMenu(controller)
    if not offline() then return nil end
    require("ui.uieditor.widgets.BackgroundFrames.GenericMenuFrame")
    require("ui.uieditor.widgets.Lobby.Common.List1ButtonLarge_PH")
    require("ui.uieditor.widgets.PC.Utility.InputTextField")
    require("ui.uieditor.menus.PC.KeyboardInputOverlay")
    installKeyboard()
    local self = CoD.Menu.NewForUIEditor(MENU)
    self.soundSet = "FrontendMain"
    self:setOwner(controller)
    self:setLeftRight(true, true, 0, 0)
    self:setTopBottom(true, true, 0, 0)
    self.buttonModel = Engine.CreateModel(Engine.GetModelForController(controller), MENU .. ".buttonPrompts")
    self.anyChildUsesUpdateState = true
    local owned = {}
    local function add(element)
        owned[#owned + 1] = element
        self:addElement(element)
        return element
    end
    local background = add(LUI.UIImage.new())
    background:setLeftRight(true, true, 0, 0)
    background:setTopBottom(true, true, 0, 0)
    background:setRGB(0.025, 0.035, 0.045)
    background:setAlpha(1)
    local frame = add(CoD.GenericMenuFrame.new(self, controller))
    frame:setLeftRight(true, true, 0, 0)
    frame:setTopBottom(true, true, 0, 0)
    frame:setModel(self.buttonModel, controller)
    if frame.titleLabel then frame.titleLabel:setText("SPLITSCREEN SETTINGS") end
    local title = frame.cac3dTitleIntermediary0
    title = title and title.FE3dTitleContainer0
    title = title and title.MenuTitle
    title = title and title.TextBox1
    -- The stock frame's animated 3-D title can inherit the lobby camera. Use
    -- a flat title inside this popup's safe area instead.
    if frame.titleLabel then frame.titleLabel:setAlpha(0) end
    if frame.cac3dTitleIntermediary0 then frame.cac3dTitleIntermediary0:setAlpha(0) end
    local function text(value, left, right, top, size, accent)
        local label = add(LUI.UIText.new())
        label:setLeftRight(true, false, left, right)
        -- UIText height is the FONT SIZE, not the available paragraph height.
        -- Stock MOTD body text uses a single-line height even for many lines.
        label:setTopBottom(true, false, top, top + size)
        label:setTTF("fonts/RefrigeratorDeluxe-Regular.ttf")
        label:setAlignment(Enum.LUIAlignment.LUI_ALIGNMENT_LEFT)
        label:setAlignment(Enum.LUIAlignment.LUI_ALIGNMENT_TOP)
        label:setLineSpacing(2)
        if accent then label:setRGB(1, 0.55, 0.18) else label:setRGB(0.8, 0.85, 0.9) end
        label:setText(value)
        return label
    end
    text("SPLITSCREEN SETTINGS", 64, 1216, 48, 36, true)
    text("LOCAL PLAY / PLAYER NAMES", 64, 1180, 112, 22, true)
    local list = add(LUI.UIList.new(self, controller, 8, 0, nil, false, false, 0, 0, false, false))
    self.Options = list
    list.id = "Options"
    list:makeFocusable()
    list:setLeftRight(true, false, 64, 664)
    list:setTopBottom(true, false, 166, 470)
    list:setWidgetType(CoD.List1ButtonLarge_PH)
    list:setVerticalCount(6)
    list:setDataSource(SOURCE)
    text("YOUR COUCH. YOUR NAMES.", 730, 1216, 166, 24, true)
    text("Four permanent slots.\nGuests need not be signed in.\n\n31 UTF-8 bytes maximum. No quotes,\nslashes, semicolons or control characters.\n\nEmpty name = engine original.\nPresets replace all four overrides.", 730, 1216, 218, 22)
    local description = text("", 64, 1216, 492, 22)
    description:linkToElementModel(list, "description", true, function(model)
        description:setText(Engine.GetModelValue(model) or "")
    end)
    self.Status = text("", 64, 1216, 564, 20, true)
    self.Notice = text("", 64, 1216, 616, 18)
    local signature = nil
    local noticeTicks = 0
    local function refresh()
        if self.zzSettingsClosed then return end
        if not offline() then
            self.Status:setText("OFFLINE FRONTEND ONLY | Editing disabled.")
            if pending and pending.menu == self then pending = nil end
            return
        end
        local status = read("splitscreen_names_status")
        self.Status:setText(messages[status] or (status == "" and "WAITING | Native names are still loading; editing disabled."
            or "UNKNOWN NATIVE STATUS | Editing disabled."))
        if self.notice == "Changes requested; waiting for the native save poll." then
            noticeTicks = noticeTicks + 1
            if noticeTicks >= 3 then self.notice = nil; noticeTicks = 0 end
        else noticeTicks = 0 end
        self.Notice:setText(self.notice or "")
        local nextSignature = status
        for slot = 1, 4 do nextSignature = nextSignature .. "\0" .. read("splitscreen_name_" .. slot) end
        if nextSignature ~= signature then
            local active = list.activeWidget
            local idModel = active and active:getModel(controller, "slotId")
            local id = idModel and Engine.GetModelValue(idModel)
            list:updateDataSource(false, false)
            if id then list:findItem({slotId = id}, nil, true, 0) end
            signature = nextSignature
        end
    end
    self:registerEventHandler("ui_keyboard_input", function(element, event)
        local request = pending
        if not request or request.menu ~= self or self.zzSettingsClosed or event.type ~= keyboardType()
            or (event.controller ~= nil and event.controller ~= request.controller) then return false end
        -- PC accept queues an engine command and closes the overlay immediately.
        -- Its later event has no request ID: it must not accept a newly opened
        -- slot (or turn cancellation into a reset). Only our explicit helper
        -- may submit an overlay-owned request. Event-only keyboards retain the
        -- stock input/cancel contract when no PC overlay was constructed.
        if (request.pcKeyboard or request.overlay) and event.zzSettingsRequest ~= request then return false end
        pending = nil
        -- nil input is cancellation; an explicitly accepted empty string is reset.
        if event.input ~= nil and event.cancelled ~= true and event.canceled ~= true and writable() then
            if validName(event.input) then
                Engine.SetDvar("splitscreen_name_" .. request.slot, event.input)
                self.notice = "Changes requested; waiting for the native save poll."
            else self.notice = "Invalid name. Use at most 31 UTF-8 bytes; no quotes, slashes, semicolons or controls." end
        end
        refresh()
        return true
    end)
    self:AddButtonCallbackFunction(list, controller, Enum.LUIButton.LUI_KEY_XBA_PSCROSS, "ENTER",
        function(element, menu, actionController)
            if writable() then ProcessListAction(self, element, actionController) end
            refresh()
            return true
        end, function(element, menu)
            CoD.Menu.SetButtonLabel(menu, Enum.LUIButton.LUI_KEY_XBA_PSCROSS, "MENU_SELECT")
            return true
        end, false)
    self:AddButtonCallbackFunction(self, controller, Enum.LUIButton.LUI_KEY_XBB_PSCIRCLE, nil,
        function(element, menu, actionController) GoBack(self, actionController); return true end,
        function(element, menu)
            CoD.Menu.SetButtonLabel(menu, Enum.LUIButton.LUI_KEY_XBB_PSCIRCLE, "MENU_BACK")
            return true
        end, false)
    add(LUI.UITimer.newElementTimer(250, false, refresh))
    LUI.OverrideFunction_CallOriginalSecond(self, "close", function()
        if self.zzSettingsClosed then return end
        self.zzSettingsClosed = true
        if pending and pending.menu == self then pending = nil end
        for _, element in ipairs(owned) do element:close() end
        Engine.UnsubscribeAndFreeModel(self.buttonModel)
        local refreshNames = rawget(_G, "__zz_splitscreen_refresh_names")
        if type(refreshNames) == "function" then refreshNames() end
    end)
    refresh()
    self:processEvent({name = "menu_loaded", controller = controller})
    self:processEvent({name = "update_state", menu = self})
    if not self:restoreState() then list:processEvent({name = "gain_focus", controller = controller}) end
    return self
end

local function targetAllowed(id)
    if not LobbyData or not LobbyData.UITargets then return false end
    -- Access known targets directly, just as ezz's own menu integrations do.
    -- Engine/client-backed tables need not expose their fields to pairs().
    for _, key in ipairs({"UI_MODESELECT", "UI_ZMLOBBYLANGAME", "UI_MPLOBBYLANGAME",
        "UI_CPLOBBYLANGAME", "UI_CPLOBBYLANCUSTOMGAME"}) do
        local target = LobbyData.UITargets[key]
        if target ~= nil and (target == id or (type(target) == "table" and target.id == id)) then return true end
    end
    return false
end

local installed = false
local wrapped = setmetatable({}, {__mode = "k"})
local keyboardWrapped = setmetatable({}, {__mode = "k"})
installKeyboard = function()
    -- Stock initializes its scratch dvar before building the input field. Set
    -- the prefill at the field's construction boundary (as ezz's password menu
    -- does), not just before ui_keyboard_new, which can overwrite it.
    -- Stock InputTextField.new/PostLoadFunc only READS scratch; it does not
    -- clear it. Its inputText/dummyText therefore see this restored value.
    if CoD and CoD.InputTextField then
        local original = CoD.InputTextField.new
        if type(original) == "function" and not keyboardWrapped[original] then
            local wrapper = function(menu, controller, ...)
                local request = pending
                if request and request.controller == controller then
                    Engine.SetDvar("ui_keyboard_dvar_edit", request.prefill)
                end
                return original(menu, controller, ...)
            end
            keyboardWrapped[wrapper] = true
            CoD.InputTextField.new = wrapper
        end
    end
    -- Some engine keyboard types complete by writing the scratch dvar without
    -- ui_keyboard_input. Stock PC's explicit accept helper is authoritative;
    -- never interpret a scratch change, overlay close, or cancel as acceptance.
    local originalComplete = UIKeyboardComplete
    if type(originalComplete) == "function" and not keyboardWrapped[originalComplete] then
        local wrapper = function(element, controller, ...)
            local request = pending
            local value = request and read("ui_keyboard_dvar_edit")
            local result = originalComplete(element, controller, ...)
            if request and pending == request and request.controller == controller
                and request.overlay == element then
                request.menu:processEvent({name = "ui_keyboard_input", type = keyboardType(),
                    controller = controller, input = value, zzSettingsRequest = request})
            end
            return result
        end
        keyboardWrapped[wrapper] = true
        UIKeyboardComplete = wrapper
    end
    local originalKeyboard = LUI.createMenu.KeyboardInputOverlay
    if type(originalKeyboard) == "function" and not keyboardWrapped[originalKeyboard] then
        local wrapper = function(controller, ...)
            local request = pending
            local overlay = originalKeyboard(controller, ...)
            if overlay and request and request.controller == controller then
                request.overlay = overlay
                LUI.OverrideFunction_CallOriginalSecond(overlay, "close", function()
                    if pending == request then pending = nil end
                end)
            end
            return overlay
        end
        keyboardWrapped[wrapper] = true
        LUI.createMenu.KeyboardInputOverlay = wrapper
    end
end
local function install()
    if not CoD or not CoD.Menu or not CoD.LobbyMenus or not LUI.createMenu or not DataSources
        or not ListHelper_SetupDataSource or not LUI.UITimer.newElementTimer then return end
    if not installed then
        DataSources[SOURCE] = ListHelper_SetupDataSource(SOURCE, rows, true)
        LUI.createMenu[MENU] = createMenu
        installed = true
    end
    installKeyboard()
    local original = CoD.LobbyMenus.AddButtonsForTarget
    if type(original) ~= "function" or wrapped[original] then return end
    local wrapper = function(controller, id, ...)
        local result = original(controller, id, ...)
        if type(result) ~= "table" or not offline() or not targetAllowed(id) then return result end
        for _, entry in ipairs(result) do
            if entry.customId == BUTTON or entry.optionDisplay == "SPLITSCREEN SETTINGS" then return result end
        end
        -- Put this in the first visible row, not below Quit in an overflowing
        -- frontend list where it can be off-screen without an obvious scrollbar.
        table.insert(result, 1, {optionDisplay = "SPLITSCREEN SETTINGS", customId = BUTTON,
            isLargeButton = false, isLastButtonInGroup = true, disabled = false, selected = false,
            action = function(menu, element, actionController, param, parent)
                if offline() then OpenPopup(parent or menu, MENU, actionController) end
            end})
        return result
    end
    wrapped[wrapper] = true
    CoD.LobbyMenus.AddButtonsForTarget = wrapper
    -- The frontend may already have built LobbyButtons before this late install.
    if offline() and ForceLobbyButtonUpdate and Engine.GetPrimaryController then
        ForceLobbyButtonUpdate(Engine.GetPrimaryController())
    end
end

-- One watcher per UI VM; late stock/ezz callbacks are captured, not overwritten.
-- Each wrapper captures its own predecessor (no mutable-original recursion).
local watchedRoots = setmetatable({}, {__mode = "k"})
local watchRoots
watchRoots = function()
    local function attach(root)
        if not root or watchedRoots[root] or not root.addElement then return end
        watchedRoots[root] = true
        local watcher = LUI.UIElement.new()
        watcher:addElement(LUI.UITimer.newElementTimer(500, false, function()
            install()
            watchRoots()
        end))
        root:addElement(watcher)
    end
    attach(LUI.roots.UIRootFull)
    attach(LUI.roots.UIRoot0)
end
watchRoots()
install()
