# Open problems

Everything known to be broken, unverified or unfinished, sorted by kind.
Addresses are image RVAs of `BlackOps3.exe` build **0x06531394** (PE
checksum; the exe ezz BOIII 3.0 runs, which the code targets since 2.0).
Most points were measured on the previous build 0x06517980 and translated
with a build-to-build instruction map; the method is given where it matters.
Nothing here is guessed: each point says what was measured.

For merging into ezz BOIII natively, see [EZZ_REQUIRED_CHANGES.md](EZZ_REQUIRED_CHANGES.md).

A method that saved a lot of time: the PS4 debug build of the same game (full
symbols, unoptimised, per-client arrays still `[4]`) answers most "what is this
array / how big should it be" questions. Its struct sizes differ from PC,
names and logic do not. Almost every bug below and in the history was a
per-local-client array that the PC sized for 2 clients - see the last section.

---

## A. Real bugs, not fixed

### A1. One crash in the main menu (seen once)
40 s after launch, before any match: AV in a linked-list walk at `0x01D60534`
(list node pointer read as `0x9A1`), called from `0x01D43F51`. It happened in a
build that still had the two memory-corruption bugs fixed below (B1, B2), so it
may be the same family - but those two only write during a match. Not seen
again since.

### A2. Blocky, wrongly lit patches on panes 3/4 (Der Eisendrache)
Hangar interior: grid-aligned rectangles on the upper walls, panes 3/4 only.
Not re-tested since the sun-shadow change (B3); A4/A6 are the better leads.
(`view+0xB267C` is the light-state index, copied from the refdef at
`0x01CE01D0` - not a view index.)

### A4. MP: panes 3/4 look overexposed - not reproduced (2.2.4)
Panes 3 and 4 looked too bright in Multiplayer (seen together with player 3's
white HUD panels, which are fixed - B7). Re-measured with 2.2.3: 4-player
offline MP on Rise, all four players moving, 30 captures; mean brightness per
pane 94.4 / 87.2 / 91.9 / 85.7 (0-255), clipped pixels about 0.2% in every
pane, no white HUD blocks. Not re-tested on Splash, where it was first seen.
For reference: exposure records `[5]` are relocated and selected per client
(`0x01C5FC0C`); luminance target RT 35 (56x32) is shared by all views.
Test trap: idle test players stand still, so a symptom looks "latched".

### A5. Garbled name for a 3rd controller joining in the main menu
Frontend join of controllers 1-3 within ~2 s: seat 2's lobby member row
(stride 0x490) gets xuid = a vtable pointer (image `+0x303BD98`), whose bytes
show as the name (`+0x303BCA8` in 0x06531394). A per-controller xuid source
still sized `[2]` is read on that path (slot 2 = a foreign object). Joins made inside the ZM/MP lobby get
correct identities.

### A6. Umbra visibility: two per-client int arrays still `[2]`
`umbraGlob+0x12DC48` and `+0x12DC50` (PS4 `UmbraGlob.clientGates[4]`,
`viewCount[4]`). Client 2 writes `+0x12DC48[2]` = `+0x12DC50[0]` (client 0's
slot), client 3 the same for client 1; clients 2/3 read their `+0x12DC50`
entry from padding (0). Users: `0x01C8CC72`, `0x01C8DA64`, `0x01C8DB20`,
`0x01C8DB68`, `0x01C8CBB2` (same addresses in both builds). Relocate after a
full reference scan.

### A8. (fixed in 2.6.1, see B12)
Seen 2026-09-30 (2.6 test, Rise TDM, just after spawn): AV reading
`0xFFFFFFFFFFFFFFFF` at `0x0068293D` (PC CG_DrawNames `0x0067E0C0`, lc 2):
`drawNameEntities[1].entnum` (`0x049482D0`) held the game time `0x22FDC` and
`alpha` `0x3F800001` - a FriendlyHeadTrace {lastTraceTime, inView = 1} of
player 3 written over the name list. The five per-client statics of
cg_draw_names.cpp (PS4 all `[4]`) are `[2]` on PC and packed in front of the
list: `actorOverheadFade` `0x04945F30` [2][64] x 0x10, `centOverheadFade`
`0x04946730` [2][32] x 0x50, `overheadFade` `0x04947B60` [2][18] x 0x10,
`s_friendlyHeadTrace` `0x04947DA0` [2][18] x 8, `s_friendlyActorHeadTrace`
`0x04947EC0` [2][64] x 8 - so slots 2/3 of each overwrite the next array
(actor head traces of lc 2 start exactly at `drawNameEntities`). Reset in
`0x00677B20` (five memsets).

### A7. User reports, not reproduced yet
Hit indicator texture missing; players 1 and 2 sharing one controller with
mixed third-party + Xbox pads (one pad probably seen through two APIs).

### A3. Occasional silent exit ~20 s after launch
Exit code 0, no dialog, no dump, inside the game's own `Com_Init` (the
component's startup completes first). Intermittent; starting again works.

### A9. (fixed in 2.6.8, see B22)

### A10. Controller slots after hot-plugging (2.6.6)
Removing a seated guest's controller in the lobby and plugging another one in mixes up
seats: the real pad turned on after three virtual pads became device 0 (player 1's pad)
instead of player 4 - Steam Input lists physical pads first. The seat stays bound to the
slot, not to the pad. Needs a defined rule for unplug/replug (console: the seat waits for
its controller). Also seen (user screenshot, 2.6.6): with 3 players the lobby button still
reads ACTIVATE SPLITSCREEN, and the lobby gives no reliable way back to fewer players.
Since 2.6.8 the button reads DEACTIVATE whenever guests are in and no further pad is waiting, and an unplugged guest keeps the seat (B23). Still open: Steam's renumbering is invisible to the game (no device disappears); the PC's own per-player Gamepad option (Options > Controls, `Engine.GamepadsConnectedMap(controller, port)` -> `0x02284C30`, no 2-slot bound once the pad table is relocated) is the planned answer (docs/SIGNIN_REDESIGN.md D4).

## B. Fixed (listed because they show where to look next)

### B23. Lobby button could not remove players; unplug removed a guest (2.6.8)
The PC button (SplitscreenLobbyButtonPC) takes the first matching state: Hide, MapController,
Available (ACTIVATE: play available, `IsSplitscreenLobbyRoomAvailable()`, any pad), Active
(DEACTIVATE), AddController. Offline there is room for 4, so with 2-3 players DEACTIVATE never
showed. zz_splitscreen narrows `IsSplitscreenLobbyRoomAvailable` while guests are in: room only
while a controller has an active pad and no seat (`Engine.GamepadsConnectedIsActive` = `0x02285700`,
the per-controller test of GetNonUsedControllerCount, PS4 `0xD5BBA0`); the click uses the same
predicate. The component's 30-frame unplug leave for controllers 2/3 is gone: PS4
CL_ControllerRemoved (`0x415D80`) only raises the LUI event and no stock Lua signs a player out.

### B22. Smaller match after 3-4 players crashed at 0x00A11EC0 (fixed in 2.6.8)
`lc < cl_maxLocalClients` (`0x00A11E8B`) dereferenced client 2's NULL per-client pointer: the mod
committed the player count and the allocation floor one-way per session, so cl_maxLocalClients
(2632 compares; written only by the allocator, `0x0135D4A9` / `0x0135DC8D`) stayed 3-4. PS4
CL_SetupClientsForIngame (`0x40B870`: CompressClients, AssignUIContextsForInGame,
SetAllUsedActive) runs right before each match's CL_AllocatePerLocalClientMemory (`0x416A10`), so
every match is sized for its own players. Fix: the SetAllUsedActive detour latches the clients in
use; the count detour recognises the allocator by the return address of its count call
(`0x0135D685`) and consumes the latch. A smaller match lowers the session commitment to its size
and gets the state of a fresh match of that size first: frame pump, netchan poll and cgame frame
loop bounds (they index heap memory sized by cl_maxLocalClients), the LUI context bound back to
3, allocation floor 2. The deferred widens return once a larger match is allocated. Verified: 3
seated -> 2-player match sized 2 (was 3); then a 3-player round sized 3 with the bounds back at 3.


### B21. Players 3/4 could not sprint: stick-click limits 0.0 (fixed in 2.6.7)
User reports: L3 (sprint) dead for players 3/4 in MP and ZM, fine in menus, the same pad
sprints as player 1/2; R3 bound to sprint worked. PS4 GPad_UpdateDigitals (`0xDB8130`)
clears L3 (`0x40`) / R3 (`0x80`) while max(|stick.x|, |stick.y|) exceeds the constant 1.0.
The PC gate `0x02286030` (per slot, from the pad update `0x02285D10`) takes both limits
from the controller's profile once its user_settings file is ready: `0x0164E800(0x10 / 0x11,
c)` = ProfileSetting `GPAD_BUTTON_L/R_STICK_DEFLECT`, DDL members
`gpad_button_[lr]stick_deflect_max` of `profile_common.ddl`. The guest SettingsReadResult
runs only Storage_Reset (see the stock path's dangers in the source), and the DDL reset
memsets the buffer (PS4 `DDL_Buffer_ResetContext`): with no `user_settings_N.cgp` both
limits read 0.0, so pushing forward drops the click. PS4 SaveChanges (`0x6F59B0`) writes a
new profile as initialized, so the zeros can also come back from disk. Machines with
`user_settings_2/3.cgp` from older builds read 1.0 - why tests here always sprinted. Fix:
after the guest's settings read, its limits are decoded from the DDL context
(Storage_GetDDLContext `0x02219F80`); if one is <= 0, player 1's settings buffer is copied
in (same def and length). Verified without guest settings files: controllers 2/3 buffer =
player 1's, limits 1.0.

### B20. Freeze when a controller is switched on mid-match (fixed in 2.6.7)
ezz dialog EXCEPTION_BREAKPOINT at `0x01D3C84B` (hksDefaultPanic): Gpadupdate_f
`0x02286010` -> pad assignment `0x02284780` -> Live_RaiseLUIEvent `0x01E00EE0` -> "attempt
to index a nil value": the event for controller 2 went to `LUI.roots.UIRoot2`, which did
not exist in a 2-player match. UI_CoD_Init (PS4 `0xD04570`) clears s_rootData with memset
`0x2C0` (all four roots) before marking roots of active clients in use; the PC clears
`0x160` (`mov r8d, 0x160` at `0x01F1C9E2`), and the relocated roots 2/3 kept their in-use
flag (+0xAC) from an earlier 3-player UI init, so UI_CoD_GetRootNameForController
`0x01F1C1C0` answered "UIRoot2" instead of "UIRootFull". Fix: the clear covers the
relocated block (`0x2C0`), verified in the live process.

### B19. s_perController: BlurWorld setter not relocated (fixed in 2.6.6)
The [2] -> [4] relocation of LUI_CoD `s_perController` (`0x16263310`, stride 0x14) missed
`UI_CoD_BlurWorld` `0x01F14F40` (`lea rax,[rip+..]` at `0x01F14F47`, field +4). Blur radii
went to the old slots, which the relocated getter (`0x01F1A550`) never reads, and controllers
2/3 wrote floats into the glyph buffer behind the array (`0x1626333C`, `0x16263350`).

### B18. Players 2-4: no Start/Back, and one player's menu paused everyone (fixed in 2.6.6)
Their presses reach `s_gamePads` (bits `0x10` / `0x20`) but `playerKeys[lc].keys[14]`
(K_BUTTON_START) and `[15]` (K_BUTTON_BACK) are bound for lc 0 only. The PC binds them in
`default_bindings_<language>.cfg` (`bind BUTTON_START "togglemenu"` / `bind BUTTON_BACK
"togglescores"`), run for player 1; the pad layouts Settings_UpdateButtonConfig `0x016501B0`
(PS4 `0x6F5350`) execs per controller (`gamedata/configs/common/buttons/<layout>[_fl]`) bind
neither. Fix: its Cmd_ExecuteSingleCommand call `0x01650206` also runs those two binds for
lc > 0. Pause: CG_CanPauseGame `0x00843BD0` (PS4 `0x224FB0`) is true in MP when every client
is local, and UI_SetActiveMenu then opens the pause menu for every other local client (PC
loop `0x0223371D`, PS4 `0xFA21B8`); the PC already refuses ZM/CP with more than one player
(`0x00843C12`). A midhook on `0x00843C35` takes the function's false exit `0x00843BF2` when
2+ local players exist. Not a detour: the function tail-jumps into CG_AllClientsAreLocal
`0x008C1AA0`, which has an Arxan caller guard. Verified: Back/Start per player, catcher
`0x08`/`0x10` only on the pressing client.

### B17. 3-4 players: streamed models never load (Nuk3town cars) (fixed in 2.6.6)
XModelSelectStreamableLod returns -1 when no LOD mesh of a model is resident; the car models
got LOD `0xFF` for every player, only with 3-4 players. PS4 `streamFrontendGlob` has
`savedClientPrevViewPos[4]`, `savedClientViewPos[4]`, then `numClientsLastFrame`; the PC
(glob `0x10698100`) has prev[2] `0x10AB2768`, cur[2] `0x10AB2780`, a PC-only bool[2]
`0x10AB2798`, then numClientsLastFrame `0x10AB279C`. R_Stream_UpdateForClient stores
`cur[queryClient]` (`0x01D09856..0x01D0987D`, r12 = glob), so the third view's y overwrote
numClientsLastFrame and `queryClient == numClientsLastFrame` (`0x01D09891`) never held:
combine and sort never ran. Live: after cur[1] a camera position (x 128.0, z 0.125) where the
count belongs. Fix: the three arrays move to a block for 8 views; every access (rip scan +
glob+disp32 scan of all code): BeginUpdateFrame's copy/clear `0x01D075B8` (now a call),
the four stores, the static-update loop (`0x01D09CF3`/`0x01D09CFF` and three prev disp8).
That loop also appends 8 streamer hints to a stack StreamUpdateCmd holding 10; a midhook on
`0x01D09DA0` stops at 10. Verified: cars drawn with 3 players, numClientsLastFrame intact.

### B16. Load hang at the end of the loading screen - stock PC bug (fixed in 2.6.5)
Clients stay in CA_SENDINGDATA, server clients in CS_CONNECTED. The stats transfer goes out in
0x4C0-byte packets (CL_CheckForResend `0x0134B990`, one per 100 ms); the server answers
`va("statresponse %Iu %Iu", missingLo, missingHi)` (SV_ReceiveTransferData `0x021EB62C`,
string `0x02FD1740`) and CL_DispatchConnectionlessPacket reads both with `I_atoi64`
(`0x0227C180` = CRT `_atoi64`, signed, saturates to `_I64_MAX`) at `0x0134D01D`/`0x0134D032`.
While packet 63 is missing the low mask is >= 2^63, the client gets `0x7FFFFFFFFFFFFFFF`,
drops packet 63 and never sends it again. Hang dump: server received masks
`0x7FFFFFFFFFFFFFFF` / `0x3FFF` (packet 63 missing), client pending `0x7FFFFFFFFFFFFFFF` / 0,
94309 bytes (78 packets). Deterministic once a profile's transfer needs 64+ packets; any
player count. PS4 has one mask (`statresponse %zu`). Fix: both calls -> `strtoull`. Verified:
the profile that hung every time loads.

### B15. 3-4 players: fail-fast in DynEntCl_CleanUpOldModels (fixed in 2.6.4)
`0xC0000409` code 2 (`int 29h` at `0x02BC814C` after `__security_check_cookie`), no ezz dialog
- only a WER dump. Stack: CG_ProcessDestructibleEvents -> DynEntCl_CreateEntityModel ->
DynEntCl_AddEntityModel -> DynEntCl_CleanUpOldModels `0x0146DBE0`, which runs once the extra
dynent model count reaches its limit (halved in split screen). PS4 `0x595790` collects one view
origin per active local client into `vec3_t viewOrigins[4]`; the PC frame stores them at
`[rbp+rcx*4-0x49]` (stride 0xC) with the stack cookie at `[rbp-0x29]` - room for two - while the
loop still runs to `cl_maxLocalClients`, so the third origin's z lands on the cookie. The frame
cannot grow: the loop condition at `0x0146F3F8` (`cmp ebx, r8d`) becomes `cmp ebx, 2`. Debris
near players 3/4 may be cleaned up a little sooner. Seen once (4-player MP, Nuk3town); a
4-player Nuk3town round with the fix ran without it (the path was not provoked on purpose).

### B14. MP: crash in SV_AddModifiedStats at the end of loading (fixed in 2.6.3)
`0x02206280` `cmp edi, [rax+0x18]` with rax = statsDDLCtx.def = NULL. PC client_t
(stride 0xE5170, svs.clients pointer `0x1767A398`): statsDDLCtx +0xE0B80, transferValidated
+0xE0B78, statsModified +0xE5038 (offsets from SV_ReceiveTransferData `0x021EB330`).
Measured live (read-only, every 5 ms): the LOCAL clients are transferValidated with an
empty stats context on PC - Storage_DeserializedTransferData (`0x02219E10`) returns true
without creating one - while bots (SV_AddTestClient) have a full one. Stat writes during
the match (match history, `SV_CacheClientStatChange` `0x021EAA80`) still set statsModified
on a local client, and the PC SV_AddModifiedStats takes its loop bound from ctx.def+0x18
unchecked (PS4 0xF5D780 loops to a constant). Crashed 3 of 3 times with keyboard and
mouse for player 1 and a pad for player 2 (twice for the user, once reproduced with
ACTIVATE SPLITSCREEN on Nuk3town TDM with bots); a 2-pad session did not crash.
Fix: SV_AddModifiedStats is detoured; a client without a stats context gets statsModified
cleared instead of a send (nothing to send from). Same flow with the fix: the guard fired
repeatedly during the match, no crash.

### B13. A lone player's new controller became player 2 (fixed in 2.6.2)
`splitscreen_playerCount` (dvar pointer `0x05355A00`, current int at +0x28) carries
DVAR_ARCHIVE on the PC (flags at +0x18 = 0x1); PS4 CL_RegisterDvars registers it
with flags 0 (`0x414191`). ezz writes archived dvars to
`boiii_players/user/config.cfg` and runs that file at the next start, so a session
that ended with 2 players began the next one at 2 with one player signed in. The
gamepad device assignment (`0x022849F0`) then ends with "new device && count > 1 &&
slot 1 has no device -> give it to controller 1", and the zz_splitscreen lobby join
took any button of that unused controller. Measured with a read-only monitor
(device table `0x17DEF3A0`, s_gamePads, seat bits via IsBeingUsed `0x020E3210`):
pad to slot 0, 50 ms later to slot 1. Fixes: the archive flag is cleared (as on
console; ezz's next config write drops the line), the assignment's call at
`0x02284AE8` gets min(count, signed-in seats), and the lobby joins on A only.
A/B in the measured state (count 2, one seat): 2.6.1 moved the pad to slot 1,
2.6.2 kept it in slot 0.

### B12. MP 3-4 players: crash in CG_DrawNames (fixed in 2.6.1)
The crash of A8 above. A sixth array belongs to the same group: `playerDetails`
`0x04945090` [2][18] x 0x68 (PS4 `[4][18]`, cleared by CG_ClearPlayerDetails
`0x00677B90`), whose slot 2 is `actorOverheadFade`. All six are relocated to
`[4]` (Batch 19, 74 references; the linear decode missed `0x006A7A53`, the lea of
CG_GetActorOverheadFade behind junk bytes after an int3 - every raw candidate is
now cross-checked) and each reset memset is widened from 2 to 4 slots. Verified
in 4-player MP: the old arrays stay all zero, the name list holds real entity
numbers.

### B11. Lens flares off for players 3/4 (fixed in 2.6)
The PC FxLensFlaresManager (`0x032AEC10`, a later rework of the PS4 class)
keeps its per-client state `[2]` (+0xA058 persistent data, +0xA068 visible
lists, +0xB068 counts, +0xB070/+0xB080 pools); lc 2 hit the members behind
them (NULL pool in SpawnInstance), so until 2.6 its five lc entry points
returned early for lc >= 2. 2.6 gives lc 2/3 a second manager: midhooks at the
seven entry points that take lc (and one in the backend buffer update, which
reads lc from the view) continue with (second, lc - 2); players 3/4's sources
keep accumulation ranges of their own (lc * 0x300), and both accumulation
buffers grow from 2 x 0x300 to 4 x 0x300 entries.

### B5. ezz only: ClientCommand hangs the server in 6 of 16 launches (2.0)
ezz detours `ClientCommand` (`0x0193DFC0`) and calls the original from
boiii.exe. The function opens with an Arxan caller check: bits 12..15 of the
PEB address pick one of 16 variants (jump table `0x019402E8`); nine test the
return address against the image, and a caller outside it drives the check's
state machine into an endless two-state loop (variant 2: loop
`0x0193E400..0x0193E557`). boiii.exe is mapped above the image, so variants
0/2/5/6/11/12 hang: "Connection Interrupted" on the first client command.
The component nops the nine `cmova/cmovb edx, r10d` that load the failing
state (`splitscreen_ezz.hpp`, `install_client_command_guard`). Affects every
ezz user, with or without this mod - reported as EZZ_REQUIRED_CHANGES item 12.

### B6. 4-player MP freeze / crash: Con_ClearNotify (2.0)
`Con_ClearNotify` (`0x01339210`, a leaf without unwind data, so the first
reference scan missed it) cleared local clients 2/3's game-message windows
through the whole `con` struct, i.e. inside the OLD `con.messageBuffer[2]`
tail - which is the 32 KB print queue. Its reader then copied 0x5F72 bytes
onto a stack buffer (crash) or stalled (freeze). Called only from a
server-command handler, so Zombies never hit it. 8 con-relative sites added.

### B10. Freeze when a controller is plugged in (2.1)
The component set `splitscreen_playerCount` through `Dvar_SetInt`
(`0x0226B3A0`) from its own thread and module. Dvar_SetInt checks its
caller's return address against the image (`[rsp+0xA8]` vs image base and
image+0x20000000) and xors the start of its Arxan state machine, which also
mixes in the PEB: from the DLL it looped forever in some launches while
holding the dvar lock, and the main thread blocked in
`Com_Frame_Try_Block_Function`. The value is now written directly.
`tools/`-side audit: no other function the component calls or hooks has
such a check (ClientCommand is the ezz case, B5).

### B9. MP lobby crash while a third player joined (2.1)
The 217 client-script local-client checks were widened to a fixed 3. In the
lobby `cl_maxLocalClients` is 2 and the per-client cgame blocks exist for
two, so `CScr_SetShowcaseWeaponPaintshopXUID` (`0x00F01740`) got a NULL cg
for local client 2 and wrote through it (90 of the 217 builtins take cg the
same way). The bound now follows `cl_maxLocalClients` (min(3, count - 1)),
set right after `AllocatePerLocalClientMemory` / `CL_FreePerLocalClientMemory`
- the only writers - on the main thread. A 5 ms background loop was tried
first and hung the 4-player load: the async and renderer pipelines stop
during a map load.

### B8. Screen filters missing for players 3/4 (2.0)
`CScr_SetFilterPassEnabled` (`0x0039DB30`) rejected local clients above 1
(`cmp r9d, edi` against a register holding 1, own error line "called with an
invalid local client", which flooded ezz's console). PS4 accepts 0..4 and the
function only touches the client's cg_t. Widened to 3 (`cmp eax, 3`).

### B7. MP: player 3's HUD panels drawn as white blocks (2.0)
The PC saves the first 6 UI3D texture windows (0x438 bytes) per local client
at `g_ui3dStack+0x58B0`, `[2]` (`R_UI3D_SetupBackendData` `0x01D100D0` saves,
`R_UI3D_PerframeInit` `0x01D0FF20` restores): local client 2 saved and
restored its HUD texture windows over the data behind the array. Relocated to
`[4]` (4 sites).

### B1. 4-player MP crash: HUD target tables sized for 2 players
`s_weakpointIndicators` [20], `s_reticleData` [2], `s_rocketLauncherModels` [2],
`s_armBladeModels` [8] at `0x1626BDB0..0x1626C020` (PS4: [40], [4], [4], [16]);
all indexed per local client. Players 3/4 wrote each table's tail over the next
and the arm-blade tail over the UI model globals (string hunk pointer, global
model, controller roots). Triggered as soon as bots use specialist weapons.
Now relocated (`relocate_lui_target_tables`).

### B2. 4-player MP crash: per-client 32-entity marker blocks
`0x04748220 + lc*0xA00` (32 records of 0x50 per client, only 2 blocks on PC);
players 3/4 wrote over the cgame media table (material pointers).
Now relocated (`relocate_cg_marker_blocks`).

### B3. Sun shadows: players 2-4 shared one shadow slot
Render targets 5 (`SHADOWMAP_SUN_1P`) and 9 (`SHADOWMAP_TRANS_1P`) now have 12
slices = 4 views x 3 partitions; RT 9's colour views for slices 8-11 live in a
sidecar because its view set has only 8 inline slots. Verified: no flicker,
turning player 4 no longer changes panes 2/3.

### B4. Earlier the same day
Player 4's pane grey mid-round (`s_perController` [2]), round-start crash on a
full `ClientCache_ClientPool` (per-player-entity caches), MP HUD of players 3/4,
"Failed to host lobby" with 3-4 seated, UI model pool / string hunk too small
for 4 class menus, command buffers for clients 2/3.

## C. Not caused by the mod

* **Crash when closing the game window** (WM_CLOSE / Alt+F4): AV reading 0 in
  the CRT's `_getstream` (`0x02BDC8C6`) on a worker thread that scans another
  process's image while the CRT tears down. The menu QUIT is usually clean.
* **One-off boot crash**: the game's copy routine `0x02BC47F0` began with a
  `jmp` into a hook page ~2 GB below the image (present while the mod was
  inactive) and the page was gone. Seen once.

## D. Known gaps (not broken today)

* **Campaign cybercom lock-on table** (`setupCybercomLockon`, PC ~`0x1626AA30`,
  index `lc*15 + i`, 0x20 records) is still sized for 2 players; PS4 keeps
  `s_cybercomLockData[60]`. Campaign HUD only; 4-player campaign is untested.
  Relocate it the same way as B1 before supporting campaign.
* Port-audit leftovers (key bindings 167, voice 12, gamepads 7, client UI 2
  references never rewritten on either build) - never caused a fault.
* Only game build 0x06531394 (ezz BOIII). Other builds: the component stands
  down; every RVA must be re-mapped. The CBServers client's build 0x06517980
  was supported up to 1.1.

## E. Untested / wishes

* Real controllers as players 3/4 (Steam Input decides the XInput order).
* Maps other than Der Eisendrache, Shadows of Evil, The Giant (Zombies) and
  Safeguard / Splash (MP).
* Offline Multiplayer relies on `src/ui_scripts/zz_mplan` (retail greys
  MULTIPLAYER out offline unless a mod is loaded); shipped since 1.1. With it
  4-player offline MP with bots works (tested on Safeguard and one other mode).
* Console-style "Press A to join" label on the PC-only splitscreen button.
* ESC opens the pause menu on all four panes (possibly stock PC behaviour).

## How to find the next one of these

Almost everything above was a per-local-client array that the PC compiled for
2 clients. Pattern that worked every time:
1. Catch the crash dump (BOIII writes `minidumps\boiii-crash-*.zip`) or, for
   silent corruption, snapshot a memory range and diff it live.
2. Find the array: stride from the damage pattern, base from who indexes it
   (`base + lc*stride`), size from the PS4 global of the same name.
3. Collect EVERY instruction that addresses the array (scan functions and the
   gaps between functions byte by byte - leaf functions have no unwind data),
   classify every loop bound, relocate all references at once.
