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

### A8. MP 3-4 players: crash in CG_DrawNames (name-drawing statics `[2]`)
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
`0x00677B20` (five memsets). Fix: relocate all five (next release).

### A7. User reports, not reproduced yet
Hit indicator texture missing; players 1 and 2 sharing one controller with
mixed third-party + Xbox pads (one pad probably seen through two APIs).

### A3. Occasional silent exit ~20 s after launch
Exit code 0, no dialog, no dump, inside the game's own `Com_Init` (the
component's startup completes first). Intermittent; starting again works.

## B. Fixed (listed because they show where to look next)

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
