# ezz BOIII and 4-player local splitscreen - what ezz could change natively

**None of the ezz-side changes below is required for the mod to work.** Every
point where ezz BOIII affects the mod is already worked around inside the mod
(`src/component/splitscreen_ezz.hpp`, and the plugin entry for ezz's plugin
loader, item 13). Doing them natively in ezz would make both sides cleaner:
most of the mod's workarounds - and much of its reference rewriting - would
go away. Each item says what ezz does, what the mod does today ("Mod
workaround") and what ezz could change instead ("Fix in ezz").

| item | topic | mod today |
|---|---|---|
| 1 | the two constants (`LOCAL_CLIENT_COUNT`, `CONTROLLER_INDEX_COUNT`) | background for items 2-5 |
| 2 | XUIDs for controllers 2/3 | worked around, verified |
| 3 | client name map read directly | worked around, verified |
| 4 | static cgame pools sized for 2 | worked around, verified |
| 5 | `clientUIActives[lc]` reads | the mod keeps slots 2/3 usable; 4-player matches work; ezz's own reads of slots 2/3 not checked |
| 6 | hooks on functions both patch | chained / stacked - notes for a merge |
| 7 | engine globals both name | notes for a merge |
| 8 | checked, not a problem | - |
| 9 | players 3/4 need PLAY OFFLINE | usage (engine rule, not ezz) |
| 10 | guest names | fixed in the mod (2.2.2) |
| 11 | observations | the ignored A press: fixed in the mod (2.2.3) |
| 12 | ClientCommand hook hangs the server (every ezz user) | worked around, verified |
| 13 | plugin loader: start point, `p_name`, unload at exit | worked around by the plugin |

Every item names the ezz source line (ezz commit `5aa7fac`, `src/client/...`)
and the game address in exe **0x06531394** (Steam build 24784313, the exe ezz
v3.0.0 runs). Game addresses are RVAs (image base 0x140000000 subtracted).
Items 6 and 7 come from a development tool that cross-checks every ezz hook
site and data symbol against every mod patch site.

Status in the headings says how the point would show WITHOUT the mod's
workaround: **BLOCKER** player 3/4 cannot join · **MATCH** breaks once 3-4
players are in a round · **CLEANUP** matters for a clean merge only.

**Where it stands (2026-09-29, mod 2.1 = ezz plugin, ezz v3.0.0):** with the
mod's workarounds, four local players play offline Zombies (Der Eisendrache,
two rounds to GAME OVER and back to the lobby) and offline Multiplayer (Rise
Team Deathmatch, all four players moving and firing until the score limit,
back to the lobby) under ezz BOIII, no crash.

## 1. The two constants - raise them for ezz-owned storage ONLY

`game/structs/core.hpp:278, 288, 311, 441`

```cpp
CONTROLLER_INDEX_COUNT = 0x2,
LOCAL_CLIENT_COUNT     = 0x2,   // twice
template <typename T> using LocalClientPool = array<T, LOCAL_CLIENT_COUNT>;
```

The PS4 build (full DWARF) sizes every per-client array `[4]`
(`MAX_LOCAL_CLIENTS` 4). **Do not simply raise `LOCAL_CLIENT_COUNT` to 4.**
(An earlier version of this file said to; that was wrong.) `LocalClientPool`
is used for two different kinds of memory:

* **ezz-owned storage** - arrays ezz defines itself. These must hold 4:
  `game/symbols/cg/core.cpp:8-15` (`cgArray`, `cgsArray`, `cg_entitiesArray`,
  `cg_viewModelArray`, `cg_weaponsArray`, `cg_destructibles`, `cg_ikBuf`),
  `component/currency.cpp:481-484`, and `guids` in item 2.
* **overlays of game globals** - `symbol<...>` types laid over the game's own
  `.data`, where the PC build has `[2]`: `cg_fakeEntitiesInuseBitArray`
  (0x04C98B80), `builtin_cg_entitiesArray` (0x04C98B60),
  `builtin_cg_weaponsArray` (0x0495A410), `builtin_cg_destructibles`
  (0x17E820C0), `builtin_cg_ikBuf` (0x049B25C0), `s_userDataForControllerMap`
  (0x03390190). A `[4]` type over them makes every access to `[2]`/`[3]`, and
  every loop to the constant, hit the next global.

Concrete case: `CG_ClearCGEnts_Impl` (`game/impl/cg/cg.cpp:27-32`, installed
at `component/client_patches.cpp:335`) loops
`localClientNum < LOCAL_CLIENT_COUNT` over `builtin_cg_entitiesArray->pools`,
two pointers at 0x04C98B60. With the constant at 4 it writes `nullptr` to
0x04C98B70 - the game's `cgsArray` pointer (ezz's own `builtin_cgsArray`) -
and 0x04C98B78.

**Native fix:** a separate constant for the new capacity (PS4 name
`MAX_LOCAL_CLIENTS = 4`) used by ezz-owned storage and loops over it; each
overlay either stays `[2]` with its accesses bounded to lc 0/1, or its game
array is moved to `[4]` first (what the mod does, see item 7) and the symbol
points at the new block. Also pre-existing: `game/impl/ugc/ugc.cpp:161` loops
`controllerIndex <= CONTROLLER_INDEX_COUNT` (one past the end).

## 2. XUIDs for controllers 2 and 3 - BLOCKER (worked around, verified)

`component/auth.cpp:539` `get_client_guid`:

```cpp
static const std::array<game::XUID, 2> guids = { get_key(0).get_hash(), get_key(1).get_hash() };
controllerIndex = valid_controller_index(controllerIndex) ? controllerIndex : CONTROLLER_INDEX_0;
```

Controllers 2 and 3 get **controller 0's XUID**. It is returned by three
detours that replace the engine functions completely (no `invoke`):

| ezz hook | game function | RVA |
|---|---|---|
| auth.cpp:734 `get_guid` | LiveUser_GetXuid | 0x01EBA880 |
| auth.cpp:731 `LiveUser_UserGetXuid_stub` | LiveUser_UserGetXuid | 0x01EBABC0 |
| live.cpp:214 `LiveUser_GetLocalXuid_UseAuthGuid` | LiveUser_GetLocalXuid | 0x020F2310 |

A guest with the host's XUID is the host as far as the lobby is concerned
(`LobbySession_GetClientByXUID`), so players 3/4 cannot be added.

**Fix in ezz:** size `guids` by `CONTROLLER_INDEX_COUNT`. `key_file_path()`
already builds `-N` key files for any index (auth.cpp:97).

**Mod workaround (port-2026 branch):** the mod chains after the first two
hooks: controllers 0/1 still go to ezz, controllers 2/3 run the engine's
original code (the code official BOIII runs), reached through the original
prologue bytes that ezz's 5-byte `jmp` replaced. LiveUser_GetLocalXuid is not
chained: the engine function at 0x020F2310 takes three arguments and is far
larger than the PS4 `Live_GetLocalXUID` (15 bytes), so ezz's one-argument
replacement is suspect in itself - whether the join path reaches it is not
measured yet.

## 3. Client name map read directly - BLOCKER (worked around, verified)

`component/live.cpp:54` `LiveUser_GetClientName_GetOrInit` (detour of
LiveUser_GetClientName 0x01EBA850, no `invoke`):

```cpp
const userDataRef data = s_userDataForControllerMap->data[controllerIndex];
```

`s_userDataForControllerMap` (0x03390190) is a game array of **2** pointers.
The mod moves it to a 4-entry array (the engine's accessor
LiveUser_GetUserDataForController 0x01EBA3F0 is rewritten to follow). ezz's
direct read of `data[2]`/`data[3]` lands on the next, unrelated global.

**Fix in ezz:** call `LiveUser_GetUserDataForController(controllerIndex)`
instead of reading the global - that is exactly what the engine's own
LiveUser_GetClientName does (`sub rsp,28h; call GetUserDataForController;
add rax,8; ...`).

**Mod workaround:** controllers 2/3 are answered with the engine's code
(`GetUserDataForController(ci) + 8`); 0/1 still go to ezz.

## 4. Static cgame pools sized for 2 - MATCH (worked around, verified)

`component/client_patches.cpp:299-336`, `game/symbols/cg/core.cpp:8-12`.
On the client ezz replaces five hunk allocations with its own static pools:

| call site | pool |
|---|---|
| CG_AllocateClientMemory+0x39 (0x00840929) | cgArray |
| CG_AllocateClientMemory+0x18D3 (0x008421C3) | cgsArray |
| CG_AllocateClientMemory+0x315F (0x00843A4F) | cg_viewModelArray |
| CG_AllocateClientMemory+0x3180 (0x00843A70) | cg_attachmentsArray |
| CG_InitAndAllocCGEntsArray+0x65 (0x0085B9F5) | cg_entitiesArray (`_FirstNull`) |

All are `LocalClientPool<>` = `[2]`. The engine asks for `maxLocalClients`
elements; with 4 local clients it writes clients 2/3 past the end of ezz's
static storage (the size `assert` is compiled out in release).
`Hunk_UserAlloc_ReturnStaticAllocation_FirstNull` cycles `next_alloc_index`
over 2 slots, so local client 2 would get client 0's entity pool.
The matching frees (`Hunk_UserFree_ResetGlobal`, CG_FreeCGEnts_Impl,
CG_ClearCGEnts_Impl in `game/impl/cg/cg.cpp:27-46`) loop to
`LOCAL_CLIENT_COUNT` too. `game/impl/scr/vm/op.cpp:27` reads
`cgArray[localClientNum].time` bounded by the game's `cl_maxLocalClients`.

**Fix in ezz:** grow these ezz-owned pools to 4 (item 1, first list) and
bound `Hunk_UserAlloc_ReturnStaticAllocation_FirstNull` by the new capacity.
The free/clear loops that write the game's `[2]` pointer array
(`builtin_cg_entitiesArray`) must stay at 2 unless that array moves (item 1).

**Measured without a workaround** (2026-09-29 11:39, 4-player ZM round, all
four clients reached `first_snapshot`): access violation at 0x00FEEE5F
(`mov [rax+40h],rbp`, rax = `[rbx+8A0h]` = 0x3F428F5C3F2B851F - two floats),
rbx = a **local client 0** entity at `boiii.exe+0x12C00B0`, i.e. inside ezz's
static entity pool. Client 2 had been handed client 0's pool.

**Mod workaround:** requests larger than ezz's pool (`cgArray` n x 0x342720,
`cgsArray` n x 0x1E940, `cg_viewModelArray` n x 0x3A0 - the `imul` before
each call is checked) get a zeroed buffer of their own; smaller ones still go
to ezz, so 1-2 player rounds are unchanged. Entity pools: the engine loop
index (rsi) is recorded before the call and clients 2/3 get pools of their
own. `cg_attachmentsArray` is a fixed 0x200 and is left alone. Side effect:
in a 3-4 player round ezz's `VM_OP_GetTime_Handler_Impl`
(`game/impl/scr/vm/op.cpp:27`) reads `time` from ezz's static `cgArray`,
which the engine no longer writes then - client-script GetTime under that
handler would see a stale value. Not observed to matter in the two test
rounds; item 1 removes it.

**Recognising ezz's redirect (fixed in mod 2.3.1):** `utils::hook::call`
(`common/utils/hook.cpp`) writes `call rel32` straight into boiii.exe when the
target is within 2 GB of the call site, and a call to a `jmp [rip+0]` thunk
otherwise, so the form depends on where ASLR loads boiii.exe (the same for
every launch until the next reboot). The mod up to 2.3 recognised only the
thunk; with boiii.exe 1.05 GB from the game (measured 2026-09-30) none of the
four hooks above was installed, clients 2/3 wrote into ezz's `[2]` pools, and
4-player loads crashed intermittently (client 2's `cgs_t` inside
`cg_entitiesArray`: AV at 0x02381906 on an entity field holding a `cgs_t`
value). 2.3.1 accepts both forms. Growing the pools in ezz (item 1) makes the
workaround unnecessary.

## 5. clientUIActives indexed by local client - MATCH (mod works; ezz's reads not checked)

ezz reads `cg::clientUIActives->actives[lc]` (0x05359BC0, game array
`[2]` x 0x1078) in `game/impl/cl/cl.cpp:56, 219, 225, 402`,
`component/auth.cpp:228, 590`, `component/name.cpp:214`. The most important
one is `CL_CheckForResend_Impl`, which replaces CL_CheckForResend everywhere
(client_patches.cpp:550-557, needed because Arxan mutates the original).

The mod does NOT move this array (moving it is a known dead end - a partial
reference rewrite blacks out the frontend). Flat `actives[2]` lands at
0x0535BCB0, which is exactly the old place of `voice_comm`, an array the mod
moves away; `actives[3]` starts at 0x0535CD28, its first 0x3F0 bytes are the
rest of that freed memory, and at +0x458 it runs into ezz's `clients` and at
+0x468 into `cls`. The fields ezz reads (`flags` +0x0, `connectionState` +0x8,
`migrationState` +0x10) are inside the freed part - whether the patched engine
keeps slots 2/3 there (and not only in the mod's sidecar) is **unverified**;
check in a 4-player match before relying on it.

**Mod workaround:** the array stays in place and slots 2/3 are made usable
around it: slot 2 and most of slot 3 are the block `voice_comm` vacated,
`CL_LocalClient_IsActive` (0x027C18E0) is replaced by a C++ function (entered
through a register-saving thunk: the stock code clobbers only rax/rcx) that
answers 0 for lc 2/3 while lc >= `cl_maxLocalClients` and otherwise reads the
flag, the twelve loops that end at `actives[2]`
and the end-of-match loops run to 4, and SwapClients guards slot 3's tail.
4-player Zombies and Multiplayer matches under ezz work with this.

**Fix in ezz (native):** own a `ClientUIActives` with 4 slots and point the
engine at it, the way ezz already owns the cgame pools; then the mod's
in-place extension and guards go away.

## 6. Hooks on functions the mod also patches - CLEANUP

ezz writes a 5-byte `E9` at the function start (MinHook); the rest of the
function is untouched. The mod checks original prologue bytes before it
patches or calls, and turned the affected feature off when it saw ezz's jump.

| game function | RVA | ezz | mod | today |
|---|---|---|---|---|
| LiveUser_GetXuid | 0x01EBA880 | auth.cpp:734 | calls it for the lobby join | chained (item 2) |
| LiveUser_UserGetXuid | 0x01EBABC0 | auth.cpp:731 | - | chained (item 2) |
| LiveUser_GetClientName | 0x01EBA850 | live.cpp:200 | - | chained (item 3) |
| Storage_Pump | 0x0221A680 | live.cpp:228 (lock + invoke) | detours it to pump guest storage | stacked on ezz's detour |
| Live_LocalClient_StorageAndStats_Ready | 0x01DFEA90 | live.cpp:233 (calls Storage_Pump when not ready) | - (a diagnostic probe that called it was removed in the cleanup) | no overlap. Note: ezz's hook gives this read-only predicate a side effect (Storage_Pump); calling it from a worker thread crashed in `va()` (TLS) |
| Com_FPSLimit | 0x00F7CFD0 | client_patches.cpp:565 (replaced) | moves one reference inside it | harmless (code no longer runs) |
| CL_CheckForResend | 0x0134B990 | client_patches.cpp:550-557 (replaced) | moves one clientUIActives reference inside it | see item 5 |
| CG_ClearCGEnts | 0x02CCDE90 | client_patches.cpp:334 (replaced) | widens its bit-array reference | see item 4 |
| ClientCommand | 0x0193DFC0 | client_command.cpp:46 (detour; `invoke` when no handler matches) | disables the 9 caller range tests inside it | **item 12 - hangs the server without the mod too** |
| UI_CoD_Init, CL_FirstSnapshot | 0x01F1C890, 0x01320E80 | ui_scripting.cpp | - (a diagnostic trace on their call sites was removed in the cleanup) | no overlap |
| Hunk_UserAlloc calls in CG_AllocateClientMemory (+0x39, +0x18D3, +0x315F) and CG_InitAndAllocCGEntsArray (+0x65) | 0x00840929, 0x008421C3, 0x00843A4F, 0x0085B9F5 | client_patches.cpp:299-316 (call to a static pool) | re-points each call to its own stub, which forwards to ezz what fits ezz's pool | item 4 |

For a merge: fold the mod's change into ezz's implementation of the same
function instead of stacking detours.

## 7. Engine globals the mod moves that ezz also names - CLEANUP

From the decoded pass of `ezz_overlap.py` (mod patch sites whose operand points
into an ezz data symbol). Moved by the mod, [2] -> [4] unless noted:

| ezz symbol | RVA | used by ezz on the client |
|---|---|---|
| s_userDataForControllerMap | 0x03390190 | live.cpp:55 (item 3) |
| cg_fakeEntitiesInuseBitArray | 0x04C98B80 | declared only |
| builtin_cgsArray (pointer pair; moved out of the way, same size, so cg_entitiesArray pools can grow into its place) | 0x04C98B70 | dedicated server only |
| builtin_cg_weaponsArray / builtin_cg_destructibles / builtin_cg_ikBuf | 0x0495A410 / 0x17E820C0 / 0x049B25C0 | CG_AllocateClientMemory_Impl - not hooked on the client |

The native design is the one ezz already uses for the cgame pools: ezz owns
the `[4]` storage and the engine is pointed at it. That replaces the mod's
reference rewriting for these arrays entirely.

## 8. Not a problem (checked)

* Loading: since 2.1 the mod is an ezz plugin (`boiii\plugins\`, item 13).
  (2.0 was an `XINPUT9_1_0.dll` in the game folder; that also loaded, because
  ezz resolves the game's imports with plain `LoadLibraryA`.)
* Lua: ezz loads `boiii/ui_scripts` like official BOIII; none of ezz's own
  ui_scripts override the join path (`LobbyAddLocalClient`,
  `CoD.Menu.HandleButtonPress`, `unused_gamepad_button`).
* ASLR: the game image is already loaded at a randomised base in every run,
  and every relocated array is allocated within 1.5 GB above it
  (`allocate_near_module`, fails closed if nothing is free), so rewritten
  rip-relative and image-relative operands stay in range. Every patch checks
  the live bytes first. The real limit is a different one: the reference
  tables are specific to exe 0x06531394.

## 9. Players 3/4 need the offline lobby - usage, not an ezz change

ezz's normal ZOMBIES -> PRIVATE GAME lobby runs in network mode 2 (LIVE, read
at 0x156CE31C). The engine caps LIVE at two local players on purpose
(SplitscreenShouldBeOnline 0x0283AC60), and the mod refuses to seat player 3
there (seating a third local player into a LIVE party is what produced the
"Failed to host lobby" loop). ezz's main menu has **PLAY OFFLINE** - after
it the mode is 1 (LAN) and players 3/4 join with A as on official BOIII.
The mod's README for ezz must say: PLAY OFFLINE first, then ZOMBIES (or
MULTIPLAYER), then the extra players press A.

## 10. Guest names - fixed in the mod (2.2.2)

ezz's LiveUser_UserGetName (live.cpp:19) names a guest `<Steam name>(<ci+1>)`.
The mod gives controllers 2/3 an identity copied from controller 1 at boot,
and item 3's workaround returns the stored gamertag text. Up to 2.2.1 the
copy only overwrote one character with the controller digit, so players 3
and 4 showed as "(2)" and "(3)". Since 2.2.2 the mod builds the whole
gamertag (char[32] at userData+0x08) the way ezz formats guests: the donor's
name without its "(n)" plus "(<ci+1>)" (`write_guest_gamertag`,
src/component/splitscreen/01_core.inl). Native fix: item 3.

## 11. Observed, not explained yet

* ~~The first A press of player 4 did nothing~~ - **not ezz, fixed in the
  mod (2.2.3)**. The stock LobbyAddLocalClient (uieditor/actions.lua) shows
  the warning MENU_RESTRICTED_TO_LOCAL_GAMES once the third controller is
  in; the dialog accepts any controller, so player 4's first A only closed
  it. ui_scripts/zz_splitscreen drops that one message in offline lobbies
  (wrapper around ShouldOpenMessageDialog).
* ~~Multiplayer freezes under ezz~~ - **not ezz, a mod bug, fixed in 2.0**.
  The 4-player MP freeze ("Connection Interrupted", clock stuck) and a /GS
  crash during MP load had one cause: Con_ClearNotify (PC 0x01339210) was
  missing from the mod's con.messageBuffer relocation and cleared 16-byte
  blocks of the engine's print queue for local clients 2/3. 2-player MP under
  ezz worked before the fix; 4-player MP (Splash TDM, bots) ran 3.5+ minutes
  after it. The same bug is in the official-build release (v1.1).

## 12. ClientCommand hook hangs the server in 6 of 16 launches - BUG, every ezz user (worked around, verified)

`component/client_command.cpp:46` detours `ClientCommand` (0x0193DFC0) and
calls the original through `ClientCommand_hook.invoke(client_num)` (line 26)
whenever no registered handler matches - i.e. for every normal client command
("mr" at class selection, "score", ...). That call returns into boiii.exe.

ClientCommand begins with an Arxan caller check (0x0193E00F): the PEB address
`ror 12, and 0xF` picks one of 16 variants (jump table 0x019402E8, the 16th
value falls through to 0x0193FA7B). Each variant tests ClientCommand's own
return address (`[rbp+0x348]`) and feeds the result into a flattened state
machine. Nine variants test the address range, the other seven test for a
call instruction in front of the return address:

| variants | test | caller in boiii.exe |
|---|---|---|
| 0, 2, 5, 6, 11, 12 | return address > image + 0x20000000 → fail | **hangs** (boiii.exe is mapped above the game image) |
| 4, 9, 14 | return address < image base → fail | passes (would hang if boiii.exe were mapped below) |
| 1, 3, 7, 8, 10, 13, 15 | byte before the return address is a call | passes (`call rax`) |

"Fail" is not a crash: it loads a state from which the state machine cycles
between two values forever (variant 2: 0xAE9E14DA ⇄ 0xF285CC6C, loop
0x0193E400..0x0193E557). The server thread spins, the client sees
"Connection Interrupted" and the main thread waits in `SV_WaitServer`
(`Com_ErrorCleanup`, EXE_ERR_SERVER_TIMEOUT). The PEB address is random per
launch, so the hang is intermittent: about 6 launches in 16 freeze on the
first client command that is passed on, with any number of players.

Evidence (2026-09-29, 4-player MP under ezz v3.0.0): server thread in the loop
above, `[rbp+0x348]` = boiii.exe+0x19C53 (right after `ff d0` = `call rax`),
PEB 0xC445112000 → variant 2, game image at 0x7FF694260000 < boiii.exe at
0x7FF76C330000. The guard is skipped only when the encrypted pointer it
decodes (global 0x0A573608, 1108 readers) is zero; it was not. `python
tools/arxan_caller_guard.py 0193DFC0` simulates all variants: above-image
callers hang in 0/2/5/6/11/12, below-image in 4/9/14, in-image in none.

Verified live 2026-09-29 with the 2.0 player build: a launch that drew
variant 11 (PEB 0xFD4B32B000), started with the command line Steam builds
from the launch option (`boiii.exe -launch "<folder>\BlackOps3.exe"`),
played 4-player MP past
class selection (clock 8:04 -> 7:06, score 3 -> 5); no thread inside
ClientCommand in three stack samples.

Of ezz's other detours only `Com_FPSLimit` has the same entry guard, and ezz
replaces it without calling the original, so it cannot hang. Official BOIII
does not hook ClientCommand.

**Mod workaround** (`splitscreen_ezz.hpp`, `install_client_command_guard`):
each range test ends in `cmova/cmovb edx, r10d` (loads the
failing state). The mod replaces those nine 4-byte instructions with a 4-byte
nop, all or none, after checking the bytes:
0x0193E064, 0x0193E3F6, 0x0193E72E, 0x0193E8DC, 0x0193EA70, 0x0193EFB7,
0x0193F38C, 0x0193F531, 0x0193F8F1. The call-instruction variants stay active.

**Same trap, other functions.** Not every guard is PEB-selected:
`Dvar_SetInt` (0x0226B3A0 -> body 0x0226B3B0) always compares its return
address with image base and image+0x20000000 and xors its state-machine
start when the caller is outside; the machine mixes in the PEB, so a call
from a DLL hangs in some launches while holding the dvar lock (mod bug,
fixed in 2.1 - the mod no longer calls it). Before calling ANY game function
from boiii.exe or a plugin, check it with `tools/caller_guard_audit.py`'s
method (a `lea reg,[rip+d]` to image+0x20000000 in its own body).

**Native fix (ezz):** the same nine nops next to ezz's other Arxan patches
(`arxan.cpp`), or call the original from a small stub inside the game image
so the return address lies in the image. Any future ezz detour that calls
the original of a guarded function has the same problem - the tool above
checks a function in seconds.

## 13. The mod as an ezz plugin (2.1) - loader behaviours the plugin works around

Since 2.1 the mod ships as `boiii\plugins\bo3_local_splitscreen.dll`
(`component/plugins.cpp` loads it). Verified 2026-09-29: plugins.log
"Loaded: BO3 Local Splitscreen", host-chain mask 0x1F7 (every bridge of
items 2-4 and 12 in place).

* **Start point.** Plugins get `post_unpack` in component order (priority,
  then registration order), so a plugin cannot rely on ezz's own detours
  (auth.cpp, live.cpp, client_patches.cpp) existing yet. The plugin
  therefore redirects boiii.exe's `SetProcessDPIAware` import and starts
  when `set_process_dpi_aware_stub` (main.cpp) calls it - after ALL
  components. Ask: an export ezz calls after every component's
  `post_unpack` (e.g. `post_start`), so plugins need no import redirect.
* **p_name is effectively required.** `lib.invoke<const char*>("p_name")`
  returns nullptr for a plugin without it, and `std::string(plugin_name)`
  in the log line then reads a null pointer. The plugin exports it. Ask:
  fall back to the file name when the export is missing (the code already
  has `fallback_name` for the exception path).
* **Unload at exit.** The component destructor frees every plugin while
  game threads may still run through a plugin's hooks; the mod pins itself.
  Ask: do not `FreeLibrary` plugins, or call `pre_destroy` and wait.
* **Updater.** A precaution, not a problem seen: please make sure the
  updater never removes files from `<game>\boiii\plugins\` (not checked in
  ezz's updater code yet).
