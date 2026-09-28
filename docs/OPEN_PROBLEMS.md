# Open problems

Everything known to be broken, unverified or unfinished. All addresses are
image RVAs of `BlackOps3.exe` build **0x06517980** (PE checksum) unless marked
otherwise. "Measured" means read from the live process or a minidump; nothing
here is guessed.

A method that saved a lot of time: the PS4 debug build of the same game
(full symbols, unoptimised, per-client arrays still `[4]`) answers most "what is
this array / how big should it be" questions. Its struct sizes differ from PC,
names and logic do not.

---

## 1. Crash: stray write over the cgame media table (4-player MP) - NOT FIXED

Seen once, 4-player offline Multiplayer (game mode 1), a few minutes into the
match. The crash itself:

* `0x01F285B2 test dword [r12+8], 0x1000`, AV reading `0x100000009`.
* Call chain `0x02038E61 -> 0x0279C130 -> 0x01F28550`. The pointer is a material
  chosen at `0x02038A56`: `[0x04749900]`, or `[0x04749908]` when bit 51 of the
  per-client UI visibility bits is set. `[0x04749908]` held `0x0000000100000001`.

The real problem is a memory corruption. Comparing the crash dump with an
earlier dump of the same build:

* `0x04749688 .. 0x04749FE8` (0x960 bytes) was overwritten by **30 records of
  0x50 bytes** (= 2 x 15 x 0x50, i.e. plausibly two local clients' worth of a
  15-record per-client block). Each record: dword +0 = 1, dword +4 = 1, qword
  +0x20 = 0, dword +0x2C = 0, byte +0x38 = 0, dword +0x3C = 0, qword +0x40 = 0;
  the other bytes untouched.
* That range is part of the cgame media table (material pointers registered
  by `0x00243847 .. 0x002438B7`); no instruction in the image addresses it
  except those registrations - so it is an indexed write from somewhere else,
  most likely a per-local-client array that is still `[2]` on PC and has a
  15 x 0x50 block per client, written for clients 2 and 3.
* Not found yet: the writer. Candidates to check first are record
  initialisers with stride 0x50 and 15 entries per client.
* The run had the experimental 4-slot sun shadows enabled (see 2). They are the
  newest change, so they are now **opt-in** - but they are not proven to be
  the cause, and the corruption may also be specific to that game mode.

## 2. Sun shadows: players 2-4 share one shadow slot - PARTIALLY DONE

The PC renders sun shadows per view into render target 5 (depth,
`SHADOWMAP_SUN_1P`) and 9 (colour, `SHADOWMAP_TRANS_1P`) with 3 partitions per
view, created with 6 slices = 2 views. The component clamps views 2-4 to slot 1
(otherwise the 3rd view indexes slice 6+, which crashed in d3d11). Result:
views 2-4 overwrite each other's shadows - shimmering / blocky shadow patches.

The PS4 renders every view into the same 3 slices but finishes one view before
the next; the PC does not (tried: everyone in slot 0 spreads the problem to
player 1 too).

**Implemented, opt-in with `BO3_SUN4=on`, NOT verified:** 12 slices for RT 5/9
(descriptor stores `0x01CD1357` / `0x01CD140A` via caves - the shared
`mov r8d,6` at `0x01CD12AD` also supplies RT 6's id), RT 9's colour views for
slices 8-11 in a sidecar (the view set has only 8 inline slots,
`0x01CD53D0` builds per-slice views only for 2..7 slices), and the two places
that pick a colour view by slice take the sidecar: `R_SetRenderTargetSlice`
`0x01CF550D` and the clear `0x01CF367D` (the first version missed the clear
and crashed in d3d11). Live values were checked (12 depth views, 8 inline +
4 sidecar colour views). The visual result was never seen - the test runs
ended in problem 1 or in the pool crash that is now fixed.

## 3. Blocky, wrongly lit patches on panes 3 and 4 - NOT FIXED

Der Eisendrache, hangar interior: panes 3/4 show grid-aligned rectangles where
the wall is lit and the rest dark (staircase edges). Stable per camera pose (no
flicker), reproducible; panes 1/2 looking at the same place are clean. Looks
like tile-based light culling data for views 3/4. Leads, none verified:

* PS4 `GfxLightingData` holds per-view tile/cull data
  (hTileGlobals, cullConstantsPack, ...); the PS4 ring is 32 deep, the PC ring
  4 deep - growing it was planned.
* A per-view index at view `+0xB267C` feeds `1 << idx` masks (`0x01CB95E7`) -
  check every mask / array that index reaches for a 2- or 3-view size.

## 4. Real controllers as players 3/4 (Steam Input)

With Steam Input on (required - off, the game sees no controller), Steam
decides the in-process XInput order and it changed between launches. Only
tested with two physical pads plus virtual ones. Needs community testing.

## 5. Occasional silent exit ~20 s after launch

The game sometimes exits with code 0 about 20 s after start, no dialog, no
dump. The component's startup completes first; the exit happens inside the
game's own `Com_Init` (UI init never runs). Intermittent; starting again
works. Cause unknown.

## 6. Crash when closing the game (not splitscreen code)

Closing the window (WM_CLOSE / Alt+F4) can crash at shutdown: AV reading 0 in
the CRT's `_getstream` (`0x02BDD406` on this build) on a worker thread that is
scanning another process's image while the CRT tears its stream table down.
The menu QUIT is usually clean. Not caused by the mod as far as measured;
not re-checked on this game build.

## 7. One-off boot crash (seen once)

11 s after launch: EXECUTE AV at an address outside every module. The game's
copy routine `0x02BC4EB0` started with a `jmp` into a hook page ~2 GB below the
image (present in an image dump taken while the mod was inactive, so not the
mod's), and that page was gone. Called from `0x01CA5F1C`. The next launch was
fine.

## 8. Offline Multiplayer needs a Lua switch

The retail PC build greys out MULTIPLAYER in the offline menu unless a mod is
loaded (`CoD.LobbyButtons.MP_LAN.disabledFunc` checks ownership, network mode,
ship build and "using mods"). `src/ui_scripts_optional/zz_mplan` keeps only the
ownership check. With it, 4-player offline MP works (lobby, class selection,
spawns, HUD per pane). It is not part of the player package yet.

## 9. Lobby UI polish

* The PC-only "Activate Splitscreen" button could read "Press A to join"
  when a seat is free, like the console (`SplitscreenLobbyButtonPC`).
* ESC on the keyboard opens the pause menu for every local player; each guest
  closes their own with B. Unknown whether that is stock PC behaviour.
* Pad input is ignored while the game window is not in the foreground (stock).

## 10. Untested

Bots in MP custom games; maps other than Der Eisendrache, Shadows of Evil,
The Giant (Zombies) and Safeguard / one other mode (MP); two-player online
play with the mod present on current BOIII.

## 11. Game build

Every address targets game build 0x06517980. If BOIII ships a different
`BlackOps3.exe`, the component stands down (the game runs normally) and all
RVAs - in `splitscreen.cpp` and the generated `splitscreen_reloc.hpp` - must be
re-mapped.

## 12. Known small gaps from the build port

A few per-client relocation tables have references that were never rewritten
on either build (players' key bindings 167, voice 12, gamepads 7, client UI
2). They have never caused a fault. Do not "fix" them without checking what
the unrewritten references do.

---

## Fixed recently (for context)

* Player 4's pane turned grey mid-round: the LUI per-controller state
  (`s_perController`) is `[2]` on PC; controller 3's "UI active" byte lay in a
  button-glyph text buffer. Relocated to `[4]`.
* 4-player MP round entry crashed on a full `ClientCache_ClientPool`
  (per-player-entity caches, sized for 2 local clients on PC) - doubled.
* MP HUD of players 3/4 (Lua `GetClientNum` answered -1 for controllers 2/3),
  "Failed to host lobby" with 3-4 players seated, UI model pool / string hunk
  exhaustion with 4 class-selection menus, command buffers for clients 2/3.
