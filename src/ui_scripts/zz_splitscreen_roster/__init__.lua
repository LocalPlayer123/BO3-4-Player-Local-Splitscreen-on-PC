-- Presentation only: never change session membership, join order, or native names.
-- Stock read directly at KingslayerKyle/T7LuaRepo, revision
-- 5f45af23887a1d850fba748c45a16ab746da3600:
-- Dumps/PC_Ship_2025-05-31/ui/T7/Utility/LobbyUtility.lua (not ui/T6/lobby).
-- GetClientList returns sessionClients; UpdateLobbyList reads gamertag/xuid/isLocal
-- and publishes controllerNum = row.localController. No name/XUID parsing.
-- UpdateLobbyList:280-463 publishes GLOBAL lobbyRoot.clientList.<1-based index>
-- and count; clanTagAndGamertag is "[" .. clantag .. "]" .. gamertag (no space).
-- ui/uieditor/datasources.lua:3224-3256 reads that same global tree. Refresh only
-- existing name models, not list owners or membership/model update notifications.
if rawget(_G, "__zz_splitscreen_roster_loaded") then return end
if not LUI or not LUI.UIElement or not LUI.UIElement.new or not LUI.UITimer
    or not LUI.UITimer.newElementTimer or not LUI.roots then return end
rawset(_G, "__zz_splitscreen_roster_loaded", true)

local function copy(source)
    local result = {}
    for key, value in pairs(source) do result[key] = value end
    return result
end

local function slot(row)
    -- Lua considers numeric zero true. Stock flags must be explicitly true/1.
    if type(row) ~= "table" or (row.isLocal ~= true and row.isLocal ~= 1) then return nil end
    local controller = row.localController
    if type(controller) ~= "number" or controller < 0 or controller > 3
        or controller % 1 ~= 0 then return nil end
    return controller
end

local function offline()
    if not Engine or type(Engine.GetLobbyNetworkMode) ~= "function"
        or type(Engine.IsInGame) ~= "function" then return false end
    if Engine.GetCurrentMap and Engine.GetCurrentMap() ~= "core_frontend" then return false end
    local inGame = Engine.IsInGame()
    if inGame ~= false and inGame ~= 0 then return false end
    local mode = Engine.GetLobbyNetworkMode()
    return mode == 0 or mode == 1
end

local function sanitize(input)
    -- Same UTF-8/control/config-character policy and 31-byte cap as native names.
    if type(input) ~= "string" then return nil end
    local result, bytes, i = {}, 0, 1
    while i <= #input do
        local c = string.byte(input, i)
        local n, cp = 1, c
        if c >= 194 and c <= 223 then n, cp = 2, c - 192
        elseif c >= 224 and c <= 239 then n, cp = 3, c - 224
        elseif c >= 240 and c <= 244 then n, cp = 4, c - 240
        elseif c >= 128 then n = 0 end
        local valid = n > 0 and i + n - 1 <= #input
        for j = 1, n - 1 do
            local nextByte = string.byte(input, i + j)
            if not nextByte or nextByte < 128 or nextByte > 191 then valid = false; break end
            cp = cp * 64 + nextByte - 128
        end
        valid = valid and not (n == 2 and cp < 128) and not (n == 3 and cp < 2048)
            and not (n == 4 and cp < 65536) and cp <= 1114111
            and not (cp >= 55296 and cp <= 57343)
        if not valid then i = i + 1
        else
            if cp >= 32 and not (cp >= 127 and cp <= 159) and cp ~= 34 and cp ~= 59
                and cp ~= 92 and cp ~= 8232 and cp ~= 8233 then
                if bytes + n > 31 then break end
                result[#result + 1] = string.sub(input, i, i + n - 1)
                bytes = bytes + n
            end
            i = i + n
        end
    end
    return table.concat(result)
end

local function present(source)
    if type(source) ~= "table" or not offline() then return source end

    local result, counts = copy(source), {}
    for key, row in pairs(source) do
        if type(row) == "table" then result[key] = copy(row) end
    end
    for _, row in ipairs(source) do
        local controller = slot(row)
        if controller ~= nil then counts[controller] = (counts[controller] or 0) + 1 end
    end
    local positions, locals = {}, {}
    local status = Engine.DvarString and Engine.DvarString(nil, "splitscreen_names_status")
    local namesReady = status == "ready" or status == "save_error"
    for index, row in ipairs(result) do
        local controller = slot(row)
        -- All occurrences of ambiguous slots stay fixed and keep stock names.
        if controller ~= nil and counts[controller] == 1 then
            positions[#positions + 1] = index
            locals[#locals + 1] = row
            if namesReady then
                local name = sanitize(Engine.DvarString(nil, "splitscreen_name_" .. (controller + 1)))
                if type(name) == "string" and name ~= "" then row.gamertag = name end
            end
        end
    end
    table.sort(locals, function(a, b) return a.localController < b.localController end)
    for index, position in ipairs(positions) do result[position] = locals[index] end
    return result
end

local wrapped = setmetatable({}, {__mode = "k"})
local function install()
    local utility = CoD and CoD.LobbyUtility
    local original = utility and utility.GetClientList
    if not utility then return end
    -- Immutable predecessor: an ezz/stock late wrapper may call an older wrapper.
    -- Never point an older closure at the newly captured function (recursion).
    if type(original) == "function" and not wrapped[original] then
        local wrapper = function(...) return present(original(...)) end
        wrapped[wrapper] = true
        utility.GetClientList = wrapper
    end
end

local function report(value)
    if Engine and type(Engine.SetDvar) == "function" then
        Engine.SetDvar("splitscreen_names_refresh_status",
            type(value) == "number" and ("updated:" .. value) or value)
    end
    return value
end

local function refresh()
    if not offline() then return report("offline_only") end
    if type(Engine.GetGlobalModel) ~= "function" or type(Engine.GetModel) ~= "function"
        or type(Engine.GetModelValue) ~= "function" or type(Engine.SetModelValue) ~= "function"
        or type(Engine.DvarString) ~= "function" then return report("native_unavailable") end
    local status = Engine.DvarString(nil, "splitscreen_names_status")
    if status ~= "ready" and status ~= "save_error" then return report("native_unavailable") end
    local global = Engine.GetGlobalModel()
    local list = global and Engine.GetModel(global, "lobbyRoot.clientList")
    local function value(parent, key)
        local model = parent and Engine.GetModel(parent, key)
        return model and Engine.GetModelValue(model)
    end
    local count = value(list, "count")
    -- Stock lobbies are small. Fail closed on corrupt/unbounded counts.
    if type(count) ~= "number" or count < 1 or count > 64 or count % 1 ~= 0 then
        return report("no_roster")
    end
    local rows, counts = {}, {}
    for index = 1, count do
        local model = Engine.GetModel(list, tostring(index))
        local valid = value(model, "validClient")
        local controller = slot({isLocal = value(model, "isLocal"),
            localController = value(model, "controllerNum")})
        if (valid == true or valid == 1) and controller ~= nil then
            counts[controller] = (counts[controller] or 0) + 1
            rows[#rows + 1] = {model = model, controller = controller}
        end
    end
    local fresh, freshCounts
    local function stockName(controller)
        if not freshCounts then
            fresh, freshCounts = {}, {}
            local utility = CoD and CoD.LobbyUtility
            local source = utility and type(utility.GetClientList) == "function" and utility.GetClientList()
            if type(source) == "table" then
                for _, row in ipairs(source) do
                    local c = slot(row)
                    if c ~= nil then
                        fresh[c] = row.gamertag
                        freshCounts[c] = (freshCounts[c] or 0) + 1
                    end
                end
            end
        end
        if freshCounts[controller] == 1 and type(fresh[controller]) == "string" then
            return fresh[controller]
        end
    end
    local updated = 0
    for _, row in ipairs(rows) do
        if counts[row.controller] == 1 then
            local model = row.model
            local tag = Engine.GetModel(model, "gamertag")
            local combined = Engine.GetModel(model, "clanTagAndGamertag")
            local clan = value(model, "clantag")
            if tag and combined and type(Engine.GetModelValue(tag)) == "string"
                and type(Engine.GetModelValue(combined)) == "string" and type(clan) == "string" then
                local name = sanitize(Engine.DvarString(nil, "splitscreen_name_" .. (row.controller + 1)))
                if name == "" then name = stockName(row.controller) end
                if name ~= nil then
                    Engine.SetModelValue(tag, name)
                    Engine.SetModelValue(combined, (clan ~= "" and ("[" .. clan .. "]") or "") .. name)
                    updated = updated + 1
                end
            end
        end
    end
    return report(updated)
end

local pending
local function cancel()
    if pending then pending:close(); pending = nil end
end

-- Lazy stock/ezz dependency installation. No permanent name/model polling.
local roots = setmetatable({}, {__mode = "kv"})
local watchRoots
watchRoots = function()
    local function attach(root)
        if not root or roots[root] or not root.addElement then return end
        local watcher = LUI.UIElement.new()
        roots[root] = watcher
        watcher.id = "zz_splitscreen_roster"
        watcher:addElement(LUI.UITimer.newElementTimer(500, false, function()
            install()
            watchRoots()
        end))
        root:addElement(watcher)
    end
    attach(LUI.roots.UIRootFull)
    attach(LUI.roots.UIRoot0)
end

-- Settings calls after close. Always refresh the existing global tree, even if
-- no UpdateLobbyList call was observed. Retry at 250/500ms for the native poll.
rawset(_G, "__zz_splitscreen_refresh_names", function()
    cancel()
    install()
    watchRoots()
    local result = refresh()
    local root = LUI.roots.UIRootFull or LUI.roots.UIRoot0
    local watcher = root and roots[root]
    if result ~= "offline_only" and watcher then
        local remaining, timer = 2
        timer = LUI.UITimer.newElementTimer(250, false, function()
            if pending ~= timer then return end
            if root ~= (LUI.roots.UIRootFull or LUI.roots.UIRoot0) or not offline() then
                cancel()
                report("offline_only")
                return
            end
            remaining = remaining - 1
            refresh()
            if remaining == 0 then cancel() end
        end)
        pending = timer
        watcher:addElement(timer)
    end
    return result
end)

install()
watchRoots()