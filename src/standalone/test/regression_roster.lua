-- Full production chunk; offline harness, NOT stock source evidence.
-- Contract checked against pinned raw LobbyUtility.lua:280-463 and
-- datasources.lua:3224-3256 (5f45af23887a1d850fba748c45a16ab746da3600).
-- Model handles are opaque numbers, not Lua row tables or owner.display.
local tests = {}
local function row(controller, flag, name)
    return {localController = controller, isLocal = flag, gamertag = name or "stock",
        clantag = "", xuid = "xuid-" .. tostring(controller), nested = {keep = true}}
end
local function fixture(production, late, existing)
    local s = {network = 0, dvars = {splitscreen_names_status = "ready"}, timers = {}, calls = 0,
        now = 0, updates = 0, map = "core_frontend", inGame = false, writes = {}, models = {}}
    s.active, s.clients = {[0] = true}, {[0] = 7, [1] = 2, [2] = 11, [3] = 4}
    Enum = {scoreBoardColumns_e = {SCOREBOARD_COLUMN_CLIENTNUM = 99}}
    GetClientName = function(controller, client, extra) return "stock:" .. tostring(client), extra end
    GetClientClantag = function(controller, client) return "[clan]" end
    GetClientNameAndClanTag = function(controller, client)
        return GetClientClantag(controller, client) .. GetClientName(controller, client)
    end
    GetScoreboardPlayerName = function(controller, info, extra) return "stock-score:" .. tostring(info), extra end
    local function node()
        local n = {children = {}, closed = false}
        function n:addElement(child)
            self.children[#self.children + 1] = child
            child.parent = self
        end
        function n:getParent() return self.parent end
        function n:close()
            self.closed = true
            self.parent = nil
            for _, child in ipairs(self.children) do child:close() end
        end
        return n
    end
    local function model(path, create)
        local current = 1
        for key in string.gmatch(path, "[^.]+") do
            local parent = s.models[current]
            if not parent.children[key] and create then
                local handle = #s.models + 1
                parent.children[key] = handle
                s.models[handle] = {children = {}, path = parent.path == "" and key or parent.path .. "." .. key}
            end
            current = parent.children[key]
            if not current then return nil end
        end
        return current
    end
    s.models[1] = {children = {}, path = ""}
    function s:put(path, value) self.models[model(path, true)].value = value end
    function s:value(path)
        local handle = model(path)
        return handle and self.models[handle].value
    end
    function s:remove(path)
        local parent, key = string.match(path, "^(.*)%.([^.]*)$")
        self.models[model(parent)].children[key] = nil
    end
    -- Only the harness publishes stock-shaped rows. Production must never call
    -- CreateModel, UpdateLobbyList, session APIs, or membership notifications.
    function s:publish(source)
        self:put("lobbyRoot.clientList.count", #source)
        for index, r in ipairs(source) do
            local base = "lobbyRoot.clientList." .. index .. "."
            self:put(base .. "validClient", true)
            self:put(base .. "isLocal", r.isLocal)
            self:put(base .. "controllerNum", r.localController)
            self:put(base .. "gamertag", r.gamertag)
            self:put(base .. "clantag", r.clantag)
            self:put(base .. "clanTagAndGamertag", (r.clantag ~= "" and ("[" .. r.clantag .. "]") or "") .. r.gamertag)
            self:put(base .. "xuid", r.xuid)
            self:put(base .. "rank", 42)
        end
    end
    Engine = {
        GetLobbyNetworkMode = function() return s.network end,
        GetCurrentMap = function() return s.map end,
        IsInGame = function() return s.inGame end,
        IsControllerBeingUsed = function(controller) return s.active[controller] or false end,
        GetClientNum = function(controller) return s.clients[controller] end,
        GetScoreboardTeamClientCount = function(team) assert(team == 0); return 4 end,
        GetScoreboardPlayerData = function(index, team, column)
            assert(team == 0 and column == 99)
            return ({[0] = 11, [1] = 7, [2] = 4, [3] = 2})[index]
        end,
        GetGlobalModel = function() return 1 end,
        GetModel = function(parent, path)
            assert(type(parent) == "number" and s.models[parent], "opaque existing model expected")
            local prefix = s.models[parent].path
            return model((prefix == "" and "" or prefix .. ".") .. path)
        end,
        GetModelValue = function(handle) return s.models[handle].value end,
        SetModelValue = function(handle, value)
            local m = assert(s.models[handle])
            assert(string.match(m.path, "^lobbyRoot%.clientList%.%d+%.gamertag$")
                or string.match(m.path, "^lobbyRoot%.clientList%.%d+%.clanTagAndGamertag$"), "non-name write")
            assert(type(value) == "string")
            s.writes[#s.writes + 1] = {handle = handle, path = m.path, value = value}
            m.value = value
        end,
        CreateModel = function() error("production must not create models") end,
        SetDvar = function(key, value)
            assert(key == "splitscreen_names_refresh_status", "no name/session dvar writes")
            s.dvars[key] = value
        end,
        DvarString = function(controller, key)
            assert(controller == nil)
            s.reads = (s.reads or 0) + 1
            return s.dvars[key] or ""
        end
    }
    s.original = function(...)
        s.calls = s.calls + 1
        assert(s.calls < 100, "recursive wrapper")
        s.argument = ...
        return s.source
    end
    s.update = function()
        s.updates = s.updates + 1
        error("refresh must not call UpdateLobbyList")
    end
    CoD = late and nil or {LobbyUtility = {GetClientList = s.original, UpdateLobbyList = s.update}}
    LUI = {roots = {UIRootFull = node(), UIRoot0 = node()}, UIElement = {new = node}, UITimer = {}}
    LUI.UITimer.newElementTimer = function(interval, once, callback)
        assert((interval == 500 or interval == 250) and once == false)
        local timer = node(); timer.callback = callback
        timer.interval, timer.due = interval, s.now + interval
        s.timers[#s.timers + 1] = timer
        return timer
    end
    __zz_splitscreen_roster_loaded = nil
    __zz_splitscreen_refresh_names = nil
    pcall = nil; io = nil; os = nil
    if existing then s:publish(existing) end
    production()
    function s:advance(ms)
        local finish = self.now + ms
        while true do
            local nextDue
            for _, timer in ipairs(self.timers) do
                if not timer.closed and timer.due <= finish then
                    nextDue = math.min(nextDue or timer.due, timer.due)
                end
            end
            if not nextDue then break end
            self.now = nextDue
            if self.poll then self:poll() end
            local count = #self.timers
            for i = 1, count do
                local timer = self.timers[i]
                if not timer.closed and timer.due == nextDue then
                    timer.due = timer.due + timer.interval
                    timer.callback()
                end
            end
        end
        self.now = finish
    end
    function s:tick() self:advance(500) end
    function s:get() return CoD.LobbyUtility.GetClientList("forwarded") end
    function s:pending()
        local count = 0
        for _, timer in ipairs(self.timers) do
            if timer.interval == 250 and not timer.closed then count = count + 1 end
        end
        return count
    end
    return s
end
local function order(list, expected)
    assert(#list == #expected)
    for i, controller in ipairs(expected) do assert(list[i].localController == controller, "wrong slot order") end
end

local function matchFixture(production)
    local s = fixture(production)
    s.inGame, s.map = true, "zm_test"
    for controller = 0, 3 do
        s.active[controller] = true
        s.dvars["splitscreen_name_" .. (controller + 1)] = "Custom" .. controller
    end
    return s
end

function tests.match_names_resolve_client_not_row_or_viewer(production)
    local s = matchFixture(production)
    for viewer = 0, 3 do
        for controller = 0, 3 do
            assert(GetClientName(viewer, s.clients[controller]) == "Custom" .. controller)
            assert(GetClientNameAndClanTag(viewer, s.clients[controller]) == "[clan]Custom" .. controller)
        end
    end
    assert(#s.writes == 0)
end

function tests.match_scoreboard_resolves_sorted_team_index(production)
    matchFixture(production)
    for index, controller in ipairs({2, 0, 3, 1}) do
        assert(GetScoreboardPlayerName(3, "team: 0 client: " .. (index - 1) .. " 12345")
            == "[clan]Custom" .. controller)
    end
end

function tests.match_online_and_frontend_keep_stock(production)
    local s = matchFixture(production)
    for _, mode in ipairs({2, 3, -1, "0"}) do
        s.network = mode
        local name, extra = GetClientName(0, 7, "forwarded")
        assert(name == "stock:7" and extra == "forwarded")
        assert(GetScoreboardPlayerName(0, "team: 0 client: 1 5") == "stock-score:team: 0 client: 1 5")
    end
    s.network, s.inGame = 0, false
    assert(GetClientName(0, 7) == "stock:7")
end

function tests.match_remote_bot_and_ambiguous_names_unchanged(production)
    local s = matchFixture(production)
    assert(GetClientName(0, 38) == "stock:38")
    s.clients[1] = s.clients[0]
    assert(GetClientName(0, 7) == "stock:7")
    s.active[1] = false
    assert(GetClientName(0, 7) == "Custom0")
    s.active[0] = false
    assert(GetClientName(0, 7) == "stock:7")
end

function tests.match_reset_invalid_values_and_bridge_errors(production)
    local s = matchFixture(production)
    for _, value in ipairs({-1, 64, 1.5, "7", false}) do
        assert(GetClientName(0, value) == "stock:" .. tostring(value))
    end
    assert(GetClientName(0, nil) == "stock:nil")
    s.dvars.splitscreen_name_1 = ""
    assert(GetClientName(0, 7) == "stock:7")
    s.dvars.splitscreen_name_1 = "Fresh"
    assert(GetClientName(0, 7) == "Fresh")
    for _, status in ipairs({"load_error", "hook_error", "dvar_error", ""}) do
        s.dvars.splitscreen_names_status = status
        assert(GetClientName(0, 7) == "stock:7")
    end
    s.dvars.splitscreen_names_status = "save_error"
    assert(GetClientName(0, 7) == "Fresh")
end

function tests.match_malformed_scoreboard_rows_fall_back(production)
    matchFixture(production)
    for _, info in ipairs({"", "team: 0 client: -1 5", "team: 0 client: 4 5",
        "team: 0 client: 1", "team: 0 client: 1 garbage", "team: 999 client: 1 5", 7}) do
        local name, extra = GetScoreboardPlayerName(0, info, "forwarded")
        assert(name == "stock-score:" .. tostring(info) and extra == "forwarded")
    end
end

function tests.match_late_install_and_chained_wrapper_no_recursion(production)
    local s = matchFixture(production)
    local predecessor = GetClientName
    GetClientName = function(...) return predecessor(...) end
    s:tick()
    for i = 1, 4 do s:tick(); assert(GetClientName(0, 7) == "Custom0") end
    GetClientName = nil
    s:tick()
    GetClientName = function() return "late-stock" end
    s:tick()
    assert(GetClientName(0, 7) == "Custom0")
    assert(GetClientName(0, 38) == "late-stock")
end

function tests.match_missing_engine_api_preserves_stock(production)
    matchFixture(production)
    Engine.GetClientNum = nil
    assert(GetClientName(0, 7) == "stock:7")
end

function tests.sorting_three_host_two(production)
    local s = fixture(production)
    s.source = {row(2, true), row(3, 1), row(0, true)}
    s.source[1].isHost = true
    local out = s:get()
    order(out, {0, 2, 3})
    assert(out[2].isHost == true and s.argument == "forwarded" and s.calls == 1)
    order(s.source, {2, 3, 0}) -- No session/join reordering.
end

function tests.names_by_slot_not_text_or_host(production)
    local s = fixture(production)
    s.source = {row(3, 1, "Player1"), row(2, true, "Player1"), row(0, 1, "unrelated"), row(1, true, "host")}
    s.dvars.splitscreen_name_1 = "Host"
    s.dvars.splitscreen_name_2 = "Same"
    s.dvars.splitscreen_name_3 = "Same"
    s.dvars.splitscreen_name_4 = "Fourth"
    local out = s:get()
    order(out, {0, 1, 2, 3})
    for i, name in ipairs({"Host", "Same", "Same", "Fourth"}) do assert(out[i].gamertag == name) end
    assert(out[1].xuid == s.source[3].xuid and out[4].xuid == s.source[1].xuid)
end

function tests.shallow_copy_source_immutable(production)
    local s = fixture(production)
    s.source = {row(1, true, "one"), row(0, true, "zero"), metadata = {keep = 1}, scalar = 9}
    s.dvars.splitscreen_name_1 = "override"
    local out = s:get()
    assert(out ~= s.source and out[1] ~= s.source[2] and out[2] ~= s.source[1])
    assert(out[1].nested == s.source[2].nested and out.scalar == 9)
    assert(out.metadata ~= s.source.metadata and out.metadata.keep == 1)
    out[1].rankIcon = "changed"; out[2].gamertag = "changed"; out.scalar = 10
    assert(s.source[1].gamertag == "one" and s.source[2].gamertag == "zero")
    assert(s.source[2].rankIcon == nil and s.source.scalar == 9)
end

function tests.remote_positions_order_and_numeric_zero(production)
    local s = fixture(production)
    s.source = {row(3, true), row(0, 0, "remoteA"), row(2, 1), row(1, false, "remoteB"), row(0, true)}
    s.dvars.splitscreen_name_1 = "local0"; s.dvars.splitscreen_name_2 = "local1"
    local out = s:get()
    order(out, {0, 0, 2, 1, 3})
    assert(out[2].gamertag == "remoteA" and out[4].gamertag == "remoteB")
    assert(out[2].xuid == s.source[2].xuid and out[4].xuid == s.source[4].xuid)
end

function tests.online_and_unknown_modes_unchanged(production)
    local s = fixture(production)
    s.source = {row(3, true), row(0, 1)}
    s.dvars.splitscreen_name_1 = "override"
    for _, mode in ipairs({2, 3, -1, "0", false}) do
        s.network = mode
        assert(s:get() == s.source and s.reads == nil)
    end
    s.network = nil; assert(s:get() == s.source)
    Engine.GetLobbyNetworkMode = nil; assert(s:get() == s.source)
    Engine = nil; assert(s:get() == s.source)
end

function tests.both_offline_modes_and_name_status(production)
    local s = fixture(production)
    s.source = {row(1, true, "one"), row(0, 1, "zero")}
    s.dvars.splitscreen_name_1 = "override"; s.dvars.splitscreen_name_2 = ""
    for _, mode in ipairs({0, 1}) do
        s.network = mode
        for _, status in ipairs({"ready", "save_error", "", "loading", "load_error", "dvar_error", "hook_error"}) do
            s.dvars.splitscreen_names_status = status
            local out = s:get()
            order(out, {0, 1})
            assert(out[1].gamertag == ((status == "ready" or status == "save_error") and "override" or "zero"))
            assert(out[2].gamertag == "one")
        end
    end
    Engine.DvarString = nil
    local out = s:get(); order(out, {0, 1}); assert(out[1].gamertag == "zero")
end

function tests.duplicate_and_malformed_rows_stay_fixed(production)
    local s = fixture(production)
    s.source = {row(3, true), row(1, true, "duplicateA"), row("0", true), row(1, 1, "duplicateB"),
        row(-1, true), row(4, true), row(0.5, true), row(nil, true), row(0, "1"),
        false, "bad", row(0 / 0, true), row(0, true)}
    s.dvars.splitscreen_name_2 = "must not apply"
    local out = s:get()
    assert(out[1].localController == 0 and out[13].localController == 3)
    for i = 2, 12 do
        local original = s.source[i]
        if type(original) == "table" then
            assert(out[i].gamertag == original.gamertag and out[i].xuid == original.xuid)
        else assert(out[i] == original) end
    end
    assert(out[2].gamertag == "duplicateA" and out[4].gamertag == "duplicateB")
end

function tests.next_stock_update_reads_current_names(production)
    local s = fixture(production)
    s.source = {row(0, true, "stock")}
    local before = s:get()
    s.dvars.splitscreen_name_1 = "edited"
    s:tick()
    assert(s.calls == 1 and before[1].gamertag == "stock", "no speculative timer roster refresh")
    assert(s:get()[1].gamertag == "edited")
    s.dvars.splitscreen_name_1 = ""; assert(s:get()[1].gamertag == "stock")
end

function tests.late_dependencies_and_wrapper_chain(production)
    local s = fixture(production, true)
    s.source = {row(2, true), row(0, true)}
    s:tick(); CoD = {}; s:tick(); CoD.LobbyUtility = {}; s:tick()
    CoD.LobbyUtility.GetClientList = s.original; s:tick()
    local first = CoD.LobbyUtility.GetClientList
    local lateCalls = 0
    local late = function(...)
        lateCalls = lateCalls + 1
        local out = first(...); out.late = true; return out
    end
    CoD.LobbyUtility.GetClientList = late
    s:tick()
    local second = CoD.LobbyUtility.GetClientList
    assert(second ~= late)
    for i = 1, 5 do
        s:tick(); assert(CoD.LobbyUtility.GetClientList == second)
        local out = s:get(); order(out, {0, 2}); assert(out.late == true)
    end
    assert(s.calls == 5 and lateCalls == 5)
    production(); assert(CoD.LobbyUtility.GetClientList == second and #s.timers == 2)
    CoD.LobbyUtility.GetClientList = first; s:tick()
    assert(CoD.LobbyUtility.GetClientList == first, "do not wrap a known predecessor again")
end

function tests.root_lifetime_and_late_full_root(production)
    local s = fixture(production)
    assert(#LUI.roots.UIRootFull.children == 1 and #LUI.roots.UIRoot0.children == 1)
    local old = LUI.roots.UIRootFull
    LUI.roots.UIRootFull = nil; old:close()
    s:tick(); assert(#s.timers == 2)
    local new = {children = {}}
    function new:addElement(child) self.children[#self.children + 1] = child end
    LUI.roots.UIRootFull = new; s:tick()
    assert(#new.children == 1 and #s.timers == 3)
    s:tick(); assert(#new.children == 1)
    LUI.roots.UIRoot0:close(); new.children[1]:close()
    CoD.LobbyUtility.GetClientList = s.original
    s:tick(); assert(CoD.LobbyUtility.GetClientList == s.original, "closed watchers must stop")
end

function tests.non_table_results_preserved(production)
    local s = fixture(production)
    assert(s:get() == nil)
    s.source = false; assert(s:get() == false)
    s.source = "bad"; assert(s:get() == "bad")
end

local base = "lobbyRoot.clientList."
local function name(s, index) return s:value(base .. index .. ".gamertag") end

function tests.existing_global_tree_no_owner_or_update_ever_seen(production)
    local source = {row(2, true, "two"), row(0, 1, "zero"), row(3, 0, "remote"), row(1, 1, "one")}
    source[1].clantag = "CLAN"
    local s = fixture(production, false, source) -- Built before production loads.
    s:put(base .. "2.validClient", 1)
    s.dvars.splitscreen_name_1 = "Host"
    s.dvars.splitscreen_name_2 = "Second"
    s.dvars.splitscreen_name_3 = "Third"
    local handles = #s.models
    local update = CoD.LobbyUtility.UpdateLobbyList
    assert(__zz_splitscreen_refresh_names() == 3)
    assert(s.dvars.splitscreen_names_refresh_status == "updated:3")
    assert(name(s, 1) == "Third" and name(s, 2) == "Host" and name(s, 3) == "remote" and name(s, 4) == "Second")
    assert(s:value(base .. "1.clanTagAndGamertag") == "[CLAN]Third")
    assert(s:value(base .. "2.clanTagAndGamertag") == "Host")
    assert(s:value(base .. "1.clantag") == "CLAN" and s:value(base .. "1.rank") == 42)
    for i, r in ipairs(source) do
        assert(s:value(base .. i .. ".xuid") == r.xuid)
        assert(s:value(base .. i .. ".controllerNum") == r.localController)
    end
    assert(#s.models == handles and s:value("lobbyRoot.clientList.count") == 4)
    assert(s.updates == 0 and s.calls == 0 and CoD.LobbyUtility.UpdateLobbyList == update)
    assert(s:pending() == 1, "one root-owned retry, not one per root/list")
    s:advance(249); assert(#s.writes == 6)
    s:advance(1); assert(#s.writes == 12)
    s:advance(250); assert(#s.writes == 18 and s:pending() == 0)
    s:advance(2000); assert(#s.writes == 18, "no continuous model polling")
end

function tests.reset_uses_fresh_unique_local_controller_not_index_or_xuid(production)
    local s = fixture(production)
    s:publish({row(1, 1, "custom1"), row(0, true, "custom0")})
    s:put(base .. "1.clantag", "KEEP")
    s.source = {row(0, 1, "custom0"), row(1, 1, "custom1")}
    s.poll = function(self)
        if self.now >= 500 then
            self.source = {row(0, true, "restored0"), row(1, true, "restored1")}
            self.source[1].xuid = "not parsed or matched"
        end
    end
    assert(__zz_splitscreen_refresh_names() == 2)
    s:advance(250); assert(name(s, 1) == "custom1")
    s:advance(250)
    assert(name(s, 1) == "restored1" and name(s, 2) == "restored0")
    assert(s:value(base .. "1.clanTagAndGamertag") == "[KEEP]restored1")
    assert(s:pending() == 0 and s.calls == 3 and s.source[1].gamertag == "restored0")
end

function tests.reset_ambiguous_remote_missing_or_malformed_source_no_guess(production)
    local s = fixture(production)
    s:publish({row(0, true, "keep0"), row(1, true, "keep1"), row(2, true, "keep2"), row(3, true, "keep3")})
    s.source = {row(0, true, "dupA"), row(0, 1, "dupB"), row(1, 0, "remote"), row(2, true)}
    s.source[5] = row(3, true); s.source[5].gamertag = 42
    s.source[4].gamertag = false
    assert(__zz_splitscreen_refresh_names() == 0 and #s.writes == 0)
    assert(name(s, 1) == "keep0" and name(s, 2) == "keep1")
    for _, source in ipairs({false, "bad", {row("0", true)}}) do
        s.source = source; assert(__zz_splitscreen_refresh_names() == 0)
    end
    s.source = nil; assert(__zz_splitscreen_refresh_names() == 0)
    CoD = nil; assert(__zz_splitscreen_refresh_names() == 0)
end

function tests.invalid_duplicate_and_missing_model_fields_are_untouched(production)
    local s = fixture(production)
    local source = {}
    for i = 1, 17 do source[i] = row(0, true, "keep" .. i) end
    s:publish(source)
    -- One valid unique slot (3); duplicate 0 includes a malformed-name row.
    local controllers = {3, 0, 0, "1", -1, 4, 0.5, false, 0 / 0, 1, 1, 1, 1, 2, 2, 2, 2}
    for i, c in ipairs(controllers) do s:put(base .. i .. ".controllerNum", c) end
    s:remove(base .. "3.gamertag")
    s:put(base .. "10.isLocal", 0); s:put(base .. "11.isLocal", false)
    s:put(base .. "12.isLocal", "1"); s:remove(base .. "13.isLocal")
    s:put(base .. "14.validClient", false); s:put(base .. "15.validClient", 0)
    s:put(base .. "16.validClient", "1"); s:remove(base .. "17.validClient")
    for i = 1, 4 do s.dvars["splitscreen_name_" .. i] = "new" .. i end
    assert(__zz_splitscreen_refresh_names() == 1 and #s.writes == 2)
    assert(name(s, 1) == "new4")
    for i = 2, 17 do if i ~= 3 then assert(name(s, i) == "keep" .. i) end end
    assert(name(s, 3) == nil)
end

function tests.missing_name_models_or_nonstring_values_not_created(production)
    local s = fixture(production)
    for _, field in ipairs({"gamertag", "clanTagAndGamertag", "clantag"}) do
        s:publish({row(0, true, "keep")})
        s.dvars.splitscreen_name_1 = "new"
        s:put(base .. "1." .. field, 99)
        assert(__zz_splitscreen_refresh_names() == 0)
        s:remove(base .. "1." .. field)
        local count = #s.models
        assert(__zz_splitscreen_refresh_names() == 0 and #s.models == count)
    end
    assert(#s.writes == 0)
end

function tests.sanitized_dvar_utf8_byte_cap_and_clan_prefix(production)
    local s = fixture(production)
    s:publish({row(0, 1), row(1, true), row(2, true), row(3, true)})
    s.dvars.splitscreen_name_1 = 'A\n\r\t";\\B' .. string.char(127, 194, 128, 255) .. "é" .. "\226\128\168"
    s.dvars.splitscreen_name_2 = string.rep("é", 16)
    s.dvars.splitscreen_name_3 = string.rep("x", 30) .. "é"
    s.dvars.splitscreen_name_4 = string.char(192, 175, 237, 160, 128) .. "safe"
    assert(__zz_splitscreen_refresh_names() == 4)
    assert(name(s, 1) == "ABé" and name(s, 2) == string.rep("é", 15))
    assert(name(s, 3) == string.rep("x", 30) and name(s, 4) == "safe")
    s.source = {row(0, 1, "stock")}
    s.dvars.splitscreen_name_1 = "\n;"
    s.dvars.splitscreen_name_2 = false -- malformed dvar must not concatenate/write.
    assert(__zz_splitscreen_refresh_names() == 3 and name(s, 1) == "stock")
end

function tests.offline_frontend_and_bridge_status_guards(production)
    local s = fixture(production)
    s.source = {row(0, true)}; s:publish(s.source)
    s.dvars.splitscreen_name_1 = "new"
    for _, mode in ipairs({0, 1}) do
        s.network = mode
        for _, status in ipairs({"ready", "save_error"}) do
            s.dvars.splitscreen_names_status = status
            assert(__zz_splitscreen_refresh_names() == 1)
        end
    end
    for _, status in ipairs({"", "loading", "load_error", "hook_error", "dvar_error"}) do
        s.dvars.splitscreen_names_status = status
        local before = #s.writes
        assert(__zz_splitscreen_refresh_names() == "native_unavailable" and #s.writes == before)
    end
    s.dvars.splitscreen_names_status = "ready"
    for _, mode in ipairs({2, 3, -1, "0", false}) do
        s.network = mode
        assert(__zz_splitscreen_refresh_names() == "offline_only" and s:pending() == 0)
    end
    s.network = 0
    s.map = "zm_zod"; assert(__zz_splitscreen_refresh_names() == "offline_only")
    assert(s:get() == s.source, "presentation wrapper must also reject nonfrontend")
    s.map = "core_frontend"; s.inGame = true
    assert(__zz_splitscreen_refresh_names() == "offline_only" and s:get() == s.source)
    s.inGame = 1; assert(__zz_splitscreen_refresh_names() == "offline_only")
    s.inGame = false; Engine.GetCurrentMap = nil
    assert(__zz_splitscreen_refresh_names() == 1, "map API optional; enforce it when available")
    Engine.IsInGame = nil; assert(__zz_splitscreen_refresh_names() == "offline_only")
end

function tests.no_roster_bad_counts_missing_apis_and_optional_diagnostic(production)
    local s = fixture(production)
    assert(__zz_splitscreen_refresh_names() == "no_roster")
    assert(s.dvars.splitscreen_names_refresh_status == "no_roster")
    for _, count in ipairs({0, -1, 65, 1.5, "4", false, math.huge, 0 / 0}) do
        s:put("lobbyRoot.clientList.count", count)
        assert(__zz_splitscreen_refresh_names() == "no_roster" and #s.writes == 0)
    end
    s:publish({row(0, true)})
    s.dvars.splitscreen_name_1 = "new"
    for _, api in ipairs({"GetGlobalModel", "GetModel", "GetModelValue", "SetModelValue", "DvarString"}) do
        local previous = Engine[api]; Engine[api] = nil
        assert(__zz_splitscreen_refresh_names() == "native_unavailable")
        Engine[api] = previous
    end
    Engine.SetDvar = nil; assert(__zz_splitscreen_refresh_names() == 1)
    Engine = nil; assert(__zz_splitscreen_refresh_names() == "offline_only" and s:pending() == 0)
end

function tests.repeated_refresh_coalesces_root_owned_bounded_retries(production)
    local s = fixture(production)
    s:publish({row(0, true)})
    for i = 1, 8 do
        s.dvars.splitscreen_name_1 = "edit" .. i
        assert(__zz_splitscreen_refresh_names() == 1)
        assert(name(s, 1) == "edit" .. i and s:pending() == 1)
        local timer = s.timers[#s.timers]
        assert(timer.parent == LUI.roots.UIRootFull.children[1], "not list-owned")
        s:advance(100)
    end
    local before = #s.writes
    s:advance(500); assert(#s.writes == before + 4 and s:pending() == 0)
    s:advance(2000); assert(#s.writes == before + 4)
end

function tests.retry_guards_and_root_close_or_replacement(production)
    local s = fixture(production)
    s:publish({row(0, true)})
    s.dvars.splitscreen_name_1 = "new"
    __zz_splitscreen_refresh_names()
    local before = #s.writes
    s.network = 2; s:advance(250)
    assert(#s.writes == before and s:pending() == 0)
    s.network = 0; __zz_splitscreen_refresh_names(); before = #s.writes
    s.map = "zm_zod"; s:advance(250)
    assert(#s.writes == before and s:pending() == 0)
    s.map = "core_frontend"; __zz_splitscreen_refresh_names(); before = #s.writes
    s.inGame = true; s:advance(250)
    assert(#s.writes == before and s:pending() == 0)
    s.inGame = false; __zz_splitscreen_refresh_names(); before = #s.writes
    local old = LUI.roots.UIRootFull
    LUI.roots.UIRootFull = nil -- Detached old tree cannot continue retries.
    s:advance(250); assert(#s.writes == before and s:pending() == 0)
    LUI.roots.UIRootFull = old
    __zz_splitscreen_refresh_names(); before = #s.writes
    old:close(); s:advance(1000)
    assert(#s.writes == before and s:pending() == 0, "root closure owns timer lifetime")
end

function tests.lazy_roster_and_late_dependencies_need_no_capture(production)
    local s = fixture(production, true)
    assert(__zz_splitscreen_refresh_names() == "no_roster")
    s:advance(250)
    s:publish({row(0, true, "stale")})
    s.dvars.splitscreen_name_1 = "new"
    s:advance(250)
    assert(name(s, 1) == "new" and s:pending() == 0, "retry discovers newly built tree")
    assert(s.updates == 0 and s.calls == 0)
    CoD = {LobbyUtility = {GetClientList = s.original}}
    s.source = {row(0, true, "fresh")}; s.dvars.splitscreen_name_1 = ""
    assert(__zz_splitscreen_refresh_names() == 1 and name(s, 1) == "fresh")
end

function tests.sparse_rows_and_stale_rows_outside_count_not_touched(production)
    local s = fixture(production)
    s:publish({row(0, true, "zero"), row(1, true, "one"), row(2, true, "two")})
    s:remove(base .. "2")
    for i = 1, 4 do s.dvars["splitscreen_name_" .. i] = "new" .. i end
    assert(__zz_splitscreen_refresh_names() == 2)
    assert(name(s, 1) == "new1" and name(s, 3) == "new3")
    s:put("lobbyRoot.clientList.count", 1)
    s.dvars.splitscreen_name_3 = "do not apply"
    assert(__zz_splitscreen_refresh_names() == 1 and name(s, 3) == "new3")
end

function tests.no_initial_roots_callback_observes_late_root(production)
    local s = fixture(production)
    local old = LUI.roots.UIRootFull
    old:close(); LUI.roots.UIRoot0:close()
    LUI.roots = {}
    s:publish({row(0, true)})
    s.dvars.splitscreen_name_1 = "first"
    assert(__zz_splitscreen_refresh_names() == 1 and s:pending() == 0)
    local root = LUI.UIElement.new()
    LUI.roots.UIRootFull = root
    s.dvars.splitscreen_name_1 = "late"
    assert(__zz_splitscreen_refresh_names() == 1 and s:pending() == 1)
    assert(#root.children == 1 and name(s, 1) == "late")
    s:advance(500); assert(s:pending() == 0 and #root.children == 1)
end

return tests