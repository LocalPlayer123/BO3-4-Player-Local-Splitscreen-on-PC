# Plan: move the 4-player splitscreen mod into ezz BOIII, rebuilt to be maintainable

Status: draft 1, 2026-09-29; progress section updated the same evening.
Iterative - each phase ends with something that works and is tested, so the
project can stop after any phase without leaving a broken mod behind.

## 0. Progress (2026-09-29 evening)

* **Phase 3 done** - ported to the ezz exe `0x06531394` (mod 2.0); 4 players
  in Zombies and offline Multiplayer under ezz BOIII 3.0.
* **Phase 4 done** - shipped as an ezz plugin (`boiii\plugins\`, mod 2.1).
* **Phase 2 in progress** (every step either proven byte-identical or checked
  with the regression test below):
  * comments cut by ~73 %; the one file split into 14 topic parts
    (`src/component/splitscreen/*.inl`, one translation unit, same order);
  * the development switches and all test instrumentation removed - one
    fixed behaviour, no status counters, no trace hooks in the player build;
  * dead code removed; the twelve detour installs use one helper; the
    per-client array relocations (62 of them) are one ordered table
    (`perclient_rows`) with one driver instead of ~17 hand-written functions;
  * regression test without gameplay built and used: dump the patched game
    image twice per build and compare (differences allowed only in the
    addresses of moved arrays and detour relays). The cleanup build matched
    2.1 except the one intended change, then passed a 4-player MP match.
* **Phase 1 replaced** (roadmap item 10, mod 2.3): instead of a separate manifest,
  every game address, the stock and patch bytes that go with it and the layout
  values they need are in ONE header, `src/component/splitscreen_addresses.hpp`;
  the code holds names only and the release build refuses an address anywhere
  else. The move was proven line for line (nothing added to the code, the header
  is exactly what left it) and the patched game image compared at boot. A port
  rewrites this one file.
* **Phase 5** candidates measured: of 98 relocated arrays, 44 are touched by
  only 1-3 engine functions - those are the first to replace by
  re-implemented functions (see phase 5).
* **Phase 5 started** (roadmap item 8, mod 2.5): measured the functions of the 42
  arrays touched by at most 3 of them (`tools/array_functions.py --sizes`). Only 7
  have functions that are all small (<= 878 bytes) with no reference in flattened
  code; the others sit in 4-25 KB functions or Arxan-flattened code, where the
  table-driven relocation stays the maintainable form. Of the 7, an analysis with
  adversarial verification found two worth replacing: `cg_zbarriers` (done in 2.5:
  its allocator and CG_InitZBarriers re-implemented, 220/220 identical to the stock
  functions in a side-by-side diagnostic build, the old array untouched in play)
  and `s_cachedStatsChanges` (done in 2.5.1: LiveStats_SetStatChanged and the
  cache reset re-implemented; controllers 0/1 keep the engine's slots, 2/3 use the
  mod's own; 440/440 identical to the stock function, DDL writes included). The
  other five stay relocated: `rightstick` needs its live records imported on a
  late apply, and for the rest a re-implementation adds more code and risk than
  the few rewritten references it removes.

## 1. Where we started (first draft)

* The mod works today on official BOIII (game exe checksum `0x06517980`,
  Steam build 21201493). 4 players verified in Zombies and offline Multiplayer.
* One C++ file, `component/splitscreen.cpp`: 17 764 lines, of which 6 549 are
  comments, 1 075 patch-site rows in 85 tables, 3 252 hard-coded addresses.
  It works, but a human cannot maintain it (the Discord review is right).
* ezz BOIII expects a different game exe (checksum `0x06531394`, Steam build
  24784313) and refuses the one the mod runs on (tested). Every address in the
  mod must be re-found for that exe.

## 2. The review points and what we do with each

| Review point | Fact check | Decision |
|---|---|---|
| Unmaintainable, one huge file | true (numbers above) | Split into modules + one data manifest (phase 2) |
| Thousands of lines of AI comments | 37 % of the file | Code keeps 1-3 lines per fix (what, PS4 reference, where); history moves to docs (phase 2) |
| disp32/LEA patches break under ASLR | Our moved arrays are allocated at runtime within 1.5 GB above wherever Windows loaded the game (`allocate_near_module`), so 32-bit displacements stay valid. Verified live: game loaded at a relocated base with ASLR on, mod working. Weak spot: if that window is full, the move is skipped (safe, but the fix is then missing) | Keep for now, add a status report per fix, and replace the densest patches by function re-implementations over time (phase 5) |
| Port to the BOIII >= 3.0 exes (2023-03-03 / 2026-09-10) | ezz v3.0.0 accepts `0x06531394` (2026) and, deprecated until v3.1.0, `0x0888C368` (the mod's pre-port target). It REFUSES `0x06517980`, the exe official BOIII installs and the mod runs on today (tested 2026-09-29) | Target the 2026 exe (phase 3) |
| Offer of IDA databases for both exes | - | Accept. Named functions in both exes turn the port from pattern-guessing into name matching |
| Re-implement functions instead of byte patches | The right end state; too big to do at once (hundreds of functions) | Do it per subsystem, highest-risk first (phase 5) |

## 3. Target architecture

```
src/splitscreen/
  core/       patch engine: verify original bytes -> apply -> read back -> roll back;
              near allocation; status block; one log format
  manifest/   fixes.json - every fix as data: name, PS4 symbol, method
              (move array / widen bound / replace function / hook), addresses per exe
  generated/  per-exe headers built from the manifest (no hand-typed addresses)
  fixes/      one file per subsystem: lobby+signin, input, cgame, renderer,
              ui+lua, storage+stats, sun shadows ...
  replaced/   C++ re-implementations of engine functions (grows in phase 5)
  diag/       diagnostics, compiled only into test builds
```

Rules that carry over unchanged: every write checks the original bytes first
and skips with a log line on mismatch; all-or-nothing per fix; PS4 debug build
is the reference for sizes and logic.

## 4. Phases

**Phase 0 - finish the current release (days)**
* Ship v1.1 on Nexus (4-slot sun shadows fix the flicker; Multiplayer).
* Reply to the BOIII dev: accept the IDA databases, ask which exe ezz targets.
* Done when: v1.1 is public and the databases are here.

**Phase 1 - inventory (1-2 sessions)**
* Generate `fixes.json` from the current source automatically: every table,
  site, bound and hook, with its PS4 name and test status.
* Done when: the manifest reproduces exactly the addresses the current build
  patches (tool compares both).

**Phase 2 - restructure without changing behaviour (several sessions)**
* Split the file into the modules above, drive all patches from the manifest,
  cut comments to the essentials.
* Regression test that needs no gameplay: dump the patched game image from the
  old and the new build after start-up; they must be byte-identical apart from
  the addresses of the moved arrays. Then one 4-player Zombies + MP round.
* Done when: identical patched image + both rounds pass.

**Phase 3 - port to the ezz exe (several sessions)**
* Map every manifest entry to the new exe by function name (IDA databases),
  cross-checked by our own byte-level port tools (used once already for the
  2023 exe). Any entry that is not an exact match is listed, never guessed.
* Done when: every fix either verified on the new exe or explicitly disabled
  with a reason.

**Phase 4 - ezz packaging + test matrix (1-2 sessions)**
* Build as an ezz plugin (ezz loads `boiii/plugins/*.dll`) or as a component
  inside ezz - the ezz admins decide which they prefer.
* Test matrix: Zombies and Multiplayer x 2/3/4 players, joins in lobby and in
  the main menu, leave/rejoin, game over -> lobby, bots.

**Phase 5 - replace patches by re-implemented functions (ongoing)**
* Per subsystem: decompile every function that touches a per-player array,
  re-implement it for 4 players in C++, route the engine call to it, delete the
  byte patches it made unnecessary. Start with the arrays touched by the
  FEWEST engine functions (44 of 98 are touched by 1-3: e.g. scrPlaceView,
  uiElemHandles, prevview, the HUD player tables) - each is a small, testable
  step. The densest by function count (fps_ctx 90 functions, visbits 48,
  playerKeys 37, playersKb 34) stay relocated, or become ezz-owned `[4]`
  storage the engine is pointed at (the design ezz already uses for its
  cgame pools).
* Each replacement is A/B tested against the patched version.

## 5. Known bugs (fixed along the way, not blockers for the port)

1. MP: player 3's HUD panels turn into white blocks in bright areas; players 3
   and 4 look overexposed. Not caused by the sun change (verified 2026-09-29).
   Suspect: exposure measurement for players 3/4.
2. Main menu: a 3rd controller joining there gets a garbage identity (its lobby
   ID is read from a 2-entry array). Quick fix: allow players 3/4 to join only
   inside a lobby; real fix: relocate that array.
3. Nexus: hit indicator texture missing; players 1 and 2 share a controller with
   mixed third-party + Xbox pads (likely one pad seen twice - check Steam Input).
4. Engine visibility (Umbra) has two per-player arrays still sized for 2 players.

## 6. Who does what

* User: sends the Discord reply, receives the IDA databases, installs ezz BOIII
  (the game exe comes from ezz's own updater), uploads releases.
* Claude: phases 1-5 in the repo, each phase committed and logged; asks before
  any change that touches the user's game install.
* ezz admins: choose plugin vs built-in, review, merge.
