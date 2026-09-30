# BO3 4-Player Local Splitscreen on PC

Up to **four local splitscreen players** in Call of Duty: Black Ops III on PC
(the game itself allows two). Built as a component for the BOIII client -
since 2.0 for **ezz BOIII** - and shipped as an ezz plugin
(`boiii\plugins\bo3_local_splitscreen.dll`, since 2.1).

Status: **beta**. 4-player Zombies and 4-player offline Multiplayer (bots too)
work under ezz BOIII 3.0 (lobby, loadouts, 2x2 screens, HUD per pane,
back-to-back rounds). See [OPEN_PROBLEMS.md](docs/OPEN_PROBLEMS.md) for
everything that is not solved yet - please read it before changing things.

**ezz BOIII developers:** [EZZ_REQUIRED_CHANGES.md](docs/EZZ_REQUIRED_CHANGES.md)
lists what ezz could change to support four local players natively, each with
the ezz source line and the game address. None of it is required: the mod
works around every point itself today, and each item says how. Item 12 is an
ezz bug that hits every ezz player, with or without this mod.

**This code is public domain ([The Unlicense](LICENSE)).** Take it, change it,
merge it into your client, ship it under your own name. No credit needed.

---

## How it works (short version)

The PC executable was compiled for two local players: most per-local-client
arrays are `[2]` and most loops over local clients stop at 2. The PS4 build of
the same game keeps them `[4]`. The component makes the PC behave like the
console again, at runtime, without touching any file on disk:

1. **Relocate** every per-local-client array that is `[2]` on PC to a new `[4]`
   allocation near the module and rewrite every instruction that addresses it
   (rip-relative and image-relative `disp32`, found by scanning every function
   and every gap between functions byte by byte). Where only a few small
   functions use an array, those functions are re-implemented in C++ on the
   mod's own `[4]` storage instead, after running side by side with the
   engine's versions in a diagnostic build (so far: `cg_zbarriers`).
2. **Widen** the loop bounds / range checks from 2 to 4 - but only after every
   array such a loop touches has been checked for slot 2/3 (slot 2 of a `[2]`
   array usually lies on a foreign global; widening first only moves the crash).
3. **Hooks** for the few places where the PC made a different, 2-player-only
   decision (pane geometry, sign-in of controllers 2/3, sun-shadow slots, ...):
   C++ functions behind MinHook detours, rewritten `call` sites, or - where
   there is no call or function boundary - a mid-function hook
   (`splitscreen_midhook.hpp`: one shared trampoline hands all registers to a
   C++ function and continues where it says; every build runs a native
   self-test of it). No patch-specific machine code remains.
   `splitscreen_ezz.hpp` bridges the places where ezz BOIII itself is sized
   for two players (XUID table, name map, static cgame pools) and works
   around ezz's ClientCommand hook (see EZZ_REQUIRED_CHANGES.md).
4. **Lua** (`ui_scripts`) for the lobby side: console-style join with A / leave
   with B, and a table fix the stock scripts need with more than two players.

Every patch verifies the original bytes first and skips itself (with a log
line in diagnostic builds) if they differ - it never writes into code it does
not recognise. The comments in the component are the real documentation:
almost every site names the PS4 function it was checked against and what was
measured.

**Game build:** everything targets `BlackOps3.exe` with PE checksum
**0x06531394** (the build ezz BOIII 3.0 installs). On any other build the
component stands down and the game runs unmodified. A new game build needs
every RVA re-mapped - they are all in one file,
`src/component/splitscreen_addresses.hpp` (the code refers to them by name,
and the release build refuses an address anywhere else). Versions up to 1.1
targeted 0x06517980, the build the CBServers BOIII client runs.

---

## Repository layout

```
src/component/splitscreen.cpp          the component: readiness, try_apply (the order every patch
                                       is applied in) and the component class
src/component/splitscreen/*.inl        the patches by topic (core helpers, guest storage, sign-in,
                                       panes, renderer, relocations, lobby, ...), #included in
                                       order into splitscreen.cpp - one translation unit
src/component/splitscreen_addresses.hpp  every game address for exe 0x06531394: patch sites,
                                       expected and patch bytes, the relocations' reference
                                       tables and the layout values they need
src/component/splitscreen_ezz.hpp      ezz BOIII bridges (see docs/EZZ_REQUIRED_CHANGES.md)
src/ui_scripts/zz_splitscreen/         lobby: console join with A / leave with B (the stock
                                       console Lua branch, switched on for PC), fixes the
                                       PC-only Activate/Deactivate Splitscreen button
src/ui_scripts/zz_table_insert/        table.insert tolerates nil (stock Zombies HUD code
                                       raises a full-screen UI error with 3+ players)
src/ui_scripts/zz_mplan/               enables MULTIPLAYER in the offline menu (retail greys
                                       it out unless a mod is loaded; keeps the ownership check)
src/standalone/                        the ezz plugin build (bo3_local_splitscreen.dll)
  compat/                              stand-ins for the few BOIII headers the component uses
  runtime.cpp                          component registry, scheduler, hooks - no BOIII code
  ezz_plugin.cpp, ezz_plugin.def       plugin entry (p_name) + start trigger
  test/smoke.cpp                       offline smoke test of the DLL
build_standalone.cmd                   builds the plugin (MSVC + MinHook)
docs/OPEN_PROBLEMS.md                  what is broken or unverified - start here
docs/EZZ_REQUIRED_CHANGES.md           what ezz BOIII could change natively (none of it required)
docs/PLAN_EZZ_PORT.md                  the plan for moving the mod into ezz BOIII
docs/PLAYER_README.txt                 the text players get with the zip
```

---

## Option A: build it into the client (recommended)

1. Copy `src/component/*` into the client's `src/client/component/`. For a
   native ezz integration, EZZ_REQUIRED_CHANGES.md says which parts of
   `splitscreen_ezz.hpp` become unnecessary once ezz sizes its own tables
   for four.
2. The component registers itself with `REGISTER_COMPONENT` and does its work in
   `post_unpack()`. It uses only `utils::hook`, `scheduler` (the `async` and
   `renderer` pipelines) and `game::` basics.
3. There are no switches: the component has one fixed behaviour, the one
   the plugin ships. (The experiment switches and test instrumentation from
   development were removed in the September 2026 cleanup; their history is
   in the git log.)
4. Ship the three `ui_scripts` folders wherever your client loads UI scripts
   from (the plugin package puts them in `<game folder>\boiii\ui_scripts\`).
5. Built in, no plugin DLL is needed.

Define `SS_DIAG` to get a log (`%LOCALAPPDATA%\boiii\splitscreen_ui_trace.txt`)
with one line for every patch that stands down because the game's bytes
differ - useful on a new game build, never needed by players.

## Option B: the ezz plugin

Needs Visual Studio 2022 (C++ build tools) and the MinHook source
(https://github.com/TsudaKageyu/minhook).

```
set MINHOOK=<folder with the MinHook source>
build_standalone.cmd            (or: build_standalone.cmd diag)
```

Output `out\bo3_local_splitscreen.dll`. Players put it in
`<game folder>\boiii\plugins\` together with `boiii\ui_scripts\zz_table_insert`,
`zz_splitscreen` and `zz_mplan`. ezz loads every DLL in that folder and needs
the export `p_name`. Details for players: `docs/PLAYER_README.txt`.

The plugin starts right after ezz has run all its components' `post_unpack`
(it redirects `boiii.exe`'s `SetProcessDPIAware` import, which ezz calls at
that point), so ezz's own detours exist whatever order ezz loads plugins in.
It checks the game build and otherwise does nothing, and pins itself (ezz
frees plugins at exit while game threads may still run through its hooks).

---

## Memory the component uses inside the image

* `0x1A828D00`: the magic `0xB03C0FFE`, written once. The plugin checks it
  so the component is never applied twice (a client with the component built
  in, plus the plugin).
* `0x1A828500`: the seat records (controller -> local client).

Both lie in the unused tail of the last `.data` page.

## Testing notes

* Four real controllers need Steam Input on; Steam decides the XInput order.
* Virtual pads (ViGEm) work well for automated tests.
* Pad input is ignored while the game window is not in the foreground.
* Crashes: BOIII writes `minidumps\boiii-crash-*.zip` in the game folder; the
  zip contains a minidump with the exception record.
