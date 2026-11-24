# Stability hardening — 2026-11-23

This is a hardening pass, not a claim that every game-specific bug is fixed.
On 2026-11-24 the Windows x64 diagnostic DLL was cross-built with LLVM-MinGW
20261117 and MinHook v1.3.4 and passed all seven Windows smoke checks under
GE-Proton11-14 in an isolated prefix. The DLL and all three UI scripts were
installed into the supported local BO3 installation with hash verification and
a timestamped backup/rollback manifest. No in-game session has been validated.
The existing reports in [OPEN_PROBLEMS.md](OPEN_PROBLEMS.md) remain open unless
they already have recorded game verification.

## Changes

- **Hook failures:** the standalone runtime checks page-protection and MinHook
  activation results. Failed creation/activation clears the detour state and
  attempts removal. Code writes flush the instruction cache.
- **Partial relocations:** the main relocation-table writer and seven lighting
  hook sites prepare a transaction and attempt reverse-order rollback when a
  write fails. Script bounds are only recorded as applied after successful writes.
- **Critical storage gate:** generic tables alone no longer authorize larger
  allocations. Local entities, collision, client fields, per-CG context, LUI roots,
  element handles, screen placement, and aim-assist storage must also be relocated,
  and all seven lighting hooks must be installed.
  Otherwise the player-count hook answers at most two and the lobby capacity is
  not widened. A successful local-entities retry at the existing safe client-memory
  free point can now enable the extra-client paths after rechecking every prerequisite.
  Other startup-only failures still require restarting with a compatible host/build.
- **Lighting state:** extra-client sidecar lookup/reset/access now share one lock.
  Client-memory free clears the sidecar even if the next map reuses the same object
  address. State is now keyed by actual object address, not one current list per
  client: switching lighting lists cannot discard earlier writes. The setter
  derives the object address from its displaced instruction's effective address,
  rather than assuming a separate register's index agrees with the renderer.
  Objects above index 4095 are no longer silently excluded. The bounded sidecar
  holds 32768 objects and logs capacity fallback in diagnostic builds. The gate-hit counter is
  atomic. These fixes do not prove that all reported brightness bugs are resolved.
- **Stats:** negative/oversized cache counts and decoded lengths are rejected;
  null/empty messages are dropped; an unexpectedly returning overflow error cannot
  continue into the flush path.
- **Update worker:** failed streaming reads no longer accept a partial response.
  The body limit uses subtraction, parser null input is rejected, and C++ exceptions
  in the worker or thread startup cannot terminate the process. WinHTTP's normal
  certificate verification has not been disabled or replaced.
- **Startup/lifetime:** DPI-trigger startup is serialized outside the loader lock.
  Unsupported executables remain retryable; a supported-game attempt is consumed
  even if partially unsuccessful, preventing duplicate patching. Scheduler timing
  uses a monotonic clock, throwing callbacks are removed, the renderer still calls
  its original function, and the renderer hook is installed before the worker starts.
  Objects used by process-lifetime hooks/threads are deliberately not destroyed.
- **Import parsing:** bounded loaded-image reads reject malformed headers, name
  tables, strings and thunks. A missing original import-name table fails closed
  instead of dereferencing resolved IAT addresses as RVAs.
- **Lua:** real-value insertion retains native argument validation. The fallback
  without `select` remains inherently ambiguous for an explicit nil third argument;
  its comments now state this limitation instead of incorrectly claiming it is dropped.
  Lobby joining now reserves a pending controller/known pad port before calling
  stock code, consumes the join event instead of forwarding it to children, and
  clears the A button model. It rejects a port already seated or pending under
  another controller. Failed requests can retry after six watcher ticks (normally
  about three seconds; both roots ticking can shorten it, stopped UI timers can
  delay it). Missing `pcall` disables optional Steam name queries without breaking
  the menu. A script loaded before its dependencies no longer marks itself loaded.
- **Build/tests:** the Windows build compiles and runs the smoke test against the
  DLL it just linked. The detour wrapper uses `reinterpret_cast` for function
  pointers instead of an MSVC-only `static_cast` extension.

## Offline validation

[Regression runner](../tools/run_regression_tests.py) and
[test/mock boundaries](../src/standalone/test/REGRESSION.md):

- All 14 native scenarios passed with both GCC and Clang/libc++ using
  UBSan traps. The targeted follow-up includes a readiness scenario:
  every prerequisite independently blocks enabling, a safe late success enables,
  and repeated enabling does not repatch.
- Includes write-failure/rollback injection, actual stats/update/lighting function
  slices, actual standalone runtime/plugin sources with Windows/MinHook mocks,
  and 20,000 concurrent lighting reset/read/write iterations.
- Actual table-insert Lua passed with and without `select`, using isolated `lupa`.
- Fifteen full lobby-script Lua scenarios cover delayed-seat repeated presses,
  pending/seated port conflicts, separate pads, invalid requests, online/full/busy
  restrictions, retry expiry, A-only event consumption, stock navigation and
  missing-`pcall` localization. Lighting tests cover multiple lists, high indices,
  distinct clients, hash collisions, reset and capacity fallback.
- Editor diagnostics reported no errors. Patch formatting passed with the existing
  Windows batch-file CRLF line endings allowed.

Native tests are not a Windows SDK/ABI build, a race-detector proof, MinHook
machine-code validation, or a live executable address audit. The existing caller
audit cannot run without the unpacked supported game executable (and Capstone
for its related analysis tool).

## Required game validation / remaining risks

1. The cross-built diagnostic DLL has passed the Proton smoke test. MSVC/release
  builds and actual in-game hook validation remain outstanding.
2. ezz 3.0 and 3.1: 1/2/3/4-player Zombies and offline Multiplayer; transitions
   4→1, 4→2, 3→4; map restart, game-over return and repeated launches/exits.
3. Der Eisendrache power and Kino doors/indoor lighting, comparing players 1 and 3
   at the same position; Workshop FIVE with four players.
4. Rapid frontend guest joins, hot-plug and controller swaps, and Steam Deck/Proton.

Rollback remains best-effort: the OS may also refuse restoring earlier writes.
Replacement memory is retained to avoid dangling references in that case. Not
every patch group in the mod is transactional, and the startup sequence is not
one global transaction. Live code rewrites and game-owned data still depend on
the documented engine lifecycle. C++ exception handling does not catch access
violations/SEH. Process-lifetime threads do not have a graceful unload protocol;
the plugin must remain pinned. Crash dumps/live measurements are still required
to diagnose the remaining menu exits, frontend identities and map-specific reports.

## The three reported symptoms

### Live leave/rejoin follow-up — 2026-11-24

User testing reported most behavior working, but B sometimes failed to remove
a guest and one A rejoin brought both guests back. The diagnostic trace confirms
lighting hooks installed, critical storage ready, no missing startup moves, and
some seated controllers later signed out because they were absent from the game
lobby. That trace does not prove which UI callback caused the unwanted seats.

A Lua-only hotfix now handles B on a seated guest in the offline Lobby directly
through the existing `LobbyRemoveLocalClientFromLobby` function. Busy-queue leave
requests wait for watcher ticks rather than dropping the press; sent requests are
deduplicated and expire, and stale join reservations clear on leaving. Primary
controller, submenu/dialog, online and in-game B navigation are not intercepted.
The PC hard-coded controller-1 toggle is no longer called; activation selects one
free pad through the guarded join. Pending join/leave requests suppress a second
toggle action from the same press, including synchronous seat publication.

All 29 lobby Lua cases and 14 native scenarios passed. The Lua-only hotfix was
installed with SHA-256 verification and the prior script backed up; the previously
smoke-tested DLL was unchanged. Live confirmation of this follow-up is pending.

#### Zombies lobby / installed ezz integration correction

The user reported the same symptoms in the Zombies lobby after that hotfix.
Inspection of the installed ezz `splitscreen_join` UI integration found an
additional per-controller A-release subscription which calls
`LobbySplitscreenToggle(menu, joining)`. The preceding tests did not model this
subscription. Treating every toggle as a host click could select a different
unused pad, or deactivate all guests, rather than respecting `joining`.
Guest-triggered toggles now only attempt to add that exact guest, even after a
pending reservation expires. Host/mouse toggles retain their guarded behavior.

Stock Zombies uses the shared `Lobby` menu and the same two-argument removal
action; a distinct menu name or network-mode bug was not established. The removal
action itself is now wrapped, including existing stock B callbacks, with at most
one attempt per watcher epoch until the seat disappears or the six-tick deadline
expires. Previously a refused attempt was marked sent and never retried. Lobby
creation also subscribes each guest's B model, covering menus constructed before
the stock subscription bound widens. Occluded/disabled menus, online sessions,
in-game menus and the primary controller remain excluded from this added path.
The installed ezz removal wrapper is preserved, including its keyboard-host pad
mapping fix; late wrapper installation is covered against recursion.

Validation now includes 49 lobby Lua scenarios with synchronous model callbacks,
A-press/A-release integration, missing stock guest subscriptions, refused leaves,
busy queues and late ezz wrappers, plus 14 native scenarios and table-insert tests.
These are offline integration tests, not confirmation of the user's live session.

- **Workshop maps/mods:** the deferred-enable fix removes one source of depending
  on startup timing, but does not identify or fix every Workshop load failure.
  The earlier FIVE NULL-pointer measurements may share the local-entity overwrite
  mechanism subsequently recorded as B28; do not assume every Workshop report has
  that cause. Do not mask a NULL engine allocation with a fabricated pointer.
- **Kino whitewash on players 3/4:** preserving object-specific writes across lists
  and deriving the setter target directly addresses concrete lighting-state loss
  paths. This still needs same-position indoor/outdoor testing on Kino, including
  a return indoors and a map restart. No live brightness capture was available.
- **One pad controls two players after A:** the Lua changes prevent duplicate
  queued requests and refuse a duplicate known engine port. They cannot identify
  one physical pad exposed as two different API/device ports, nor change the
  intentional stock keyboard-plus-one-pad toggle. Test Steam Input and any input
  remapper configuration separately if the symptom persists.