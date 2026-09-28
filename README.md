# BO3 4-Player Local Splitscreen on PC

Up to **four local splitscreen players** in Call of Duty: Black Ops III on PC
(the game itself allows two). Built as a component for the BOIII client and
also shippable as a drop-in DLL next to `boiii.exe`.

Status: **beta**. 4-player Zombies works (lobby, loadouts, 2x2 screens, HUD per
pane, back-to-back rounds). 4-player offline Multiplayer works in development
builds. See [OPEN_PROBLEMS.md](docs/OPEN_PROBLEMS.md) for everything that is
not solved yet - please read it before changing things.

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
   and every gap between functions byte by byte).
2. **Widen** the loop bounds / range checks from 2 to 4 - but only after every
   array such a loop touches has been checked for slot 2/3 (slot 2 of a `[2]`
   array usually lies on a foreign global; widening first only moves the crash).
3. **Caves** for the few places where the PC made a different, 2-player-only
   decision (pane geometry, sign-in of controllers 2/3, sun-shadow slots, ...).
4. **Lua** (`ui_scripts`) for the lobby side: console-style join with A / leave
   with B, and a table fix the stock scripts need with more than two players.

Every patch verifies the original bytes first and skips itself (with a log
line in diagnostic builds) if they differ - it never writes into code it does
not recognise. The comments in `splitscreen.cpp` are the real documentation:
almost every site names the PS4 function it was checked against and what was
measured.

**Game build:** everything targets `BlackOps3.exe` with PE checksum
**0x06517980** (the build BOIII installs). On any other build the component
stands down and the game runs unmodified. A new game build needs every RVA
re-mapped (they are all in `splitscreen.cpp` / `splitscreen_reloc.hpp`).

---

## Repository layout

```
src/component/splitscreen.cpp          the component (all engine patches)
src/component/splitscreen_reloc.hpp    generated reference tables for the relocations
src/component/splitscreen_signin.hpp   sign-in helpers
src/ui_scripts/zz_splitscreen/         lobby: console join with A / leave with B (the stock
                                       console Lua branch, switched on for PC), fixes the
                                       PC-only Activate/Deactivate Splitscreen button
src/ui_scripts/zz_table_insert/        table.insert tolerates nil (stock Zombies HUD code
                                       raises a full-screen UI error with 3+ players)
src/ui_scripts_optional/zz_mplan/      enables MULTIPLAYER in the offline menu (not in the
                                       player package yet, see OPEN_PROBLEMS.md #8)
src/standalone/                        the drop-in DLL build (XINPUT9_1_0.dll proxy)
  compat/                              stand-ins for the few BOIII headers the component uses
  runtime.cpp                          component registry, scheduler, hooks - no BOIII code
  xinput_proxy.cpp, xinput9_1_0.def    XInput pass-through + start trigger
  test/smoke.cpp                       offline smoke test of the DLL
build_standalone.cmd                   builds the drop-in DLL (MSVC + MinHook)
docs/OPEN_PROBLEMS.md                  what is broken or unverified - start here
docs/PLAYER_README.txt                 the text players get with the drop-in zip
```

---

## Option A: build it into BOIII (recommended)

1. Copy `src/component/*` into the client's `src/client/component/`.
2. The component registers itself with `REGISTER_COMPONENT` and does its work in
   `post_unpack()`. It uses only `utils::hook`, `scheduler` (the `async` and
   `renderer` pipelines) and `game::` basics.
3. **Switches** are read with `GetEnvironmentVariableA`. The verified
   configuration is **`BO3_CG_FRAME=on`** (the drop-in build hard-wires exactly
   that). Either set it before the component runs or change the default in
   `splitscreen.cpp` (search for `"BO3_CG_FRAME"`). Other switches:
   * `BO3_SUN4=on` - separate sun shadows for all four views (experimental,
     see OPEN_PROBLEMS.md).
   * `BO3_SKIP_FIX=<names>` - disable individual relocations for bisecting.
4. Ship the two `ui_scripts` folders wherever your client loads UI scripts
   from (the drop-in package puts them in `<game folder>\boiii\ui_scripts\`).
5. Built in, no proxy DLL and no `-allowproxydlls` flag are needed.

Define `SS_DIAG` to get the diagnostic trace
(`%LOCALAPPDATA%\boiii\splitscreen_ui_trace.txt`) - extremely useful while
working on it, never needed by players.

## Option B: the drop-in DLL

Needs Visual Studio 2022 (C++ build tools) and the MinHook source
(https://github.com/TsudaKageyu/minhook).

```
set MINHOOK=<folder with the MinHook source>
build_standalone.cmd            (or: build_standalone.cmd diag)
```

Output `out\XINPUT9_1_0.dll`. Players put it next to `boiii.exe` together with
`boiii\ui_scripts\zz_table_insert` and `boiii\ui_scripts\zz_splitscreen`, and
start BOIII with `-allowproxydlls` (recent BOIII builds preload the System32
copy of any game-folder DLL named like a Windows DLL unless started with that
flag). Details for players: `docs/PLAYER_README.txt`.

The DLL starts right after BOIII has run its own components' `post_unpack`
(it hooks `boiii.exe`'s `SetProcessDPIAware` import, which BOIII calls at that
point), checks the game build, and otherwise stays a pure XInput pass-through.

---

## Runtime status block

Diagnostic readers (and the component itself, to avoid patching twice) find a
status block at image RVA `0x1A828D00`, magic `0xB03C0FFE`, followed by
per-feature counters. Seat records (controller -> local client) are at
`0x1A828500`.

## Testing notes

* Four real controllers need Steam Input on; Steam decides the XInput order.
* Virtual pads (ViGEm) work well for automated tests.
* Pad input is ignored while the game window is not in the foreground.
* Crashes: BOIII writes `minidumps\boiii-crash-*.zip` in the game folder; the
  zip contains a minidump with the exception record.
