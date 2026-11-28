# Offline production regression tests

Run from any directory: `python3 tools/run_regression_tests.py` (use the runner's
absolute path outside the repository). Requires Python 3 and a C++17 compiler.
Default compiler: `/usr/bin/g++`; override with `--cxx` or `CXX`, including flags.
No downloads, Windows SDK, game executable, or game/network access are required.
The runner fails on compiler errors, nonzero test exits, source marker drift, or
a 120-second subprocess timeout. Generated headers and binaries live only in a
temporary directory, automatically cleaned up. Repository sources are read-only.

Default instrumentation is UBSan with trap-on-error (no shared UBSan runtime
needed). `--no-sanitize` disables it. Assertions remain active regardless of
`NDEBUG`. This is **not** the Windows DLL smoke test or a game integration test.

## Production code and mock boundaries

* **Readiness:** literal production `enable_extra_clients_when_ready` plus its
  table-readiness flag. Mock patch installers count calls. Every prerequisite
  independently blocks installation; local-entities becoming ready later enables
  once, with subsequent calls remaining idempotent. Does not emulate the engine
  free callback, code patch bytes, or prove that installer internals succeed.
* **Safety:** includes the actual production header. Seven-site transactions
  test success and every failed write position, each with every subset of earlier
  rollback writes refused. Checks exact reverse attempt order and resulting bytes.
  Refused mock writes obey the production writer contract: no target modification.
  Also tests zero patches, wrapping address/length inputs, object stride alignment,
  out-of-range/negative sizes and counts.
* **Stats:** literal production slice from `struct stats_change` to the comment
  before `stats_hash_on_engine_slot0`, including actual `set_stat_changed`, cache
  selection and reset. RVAs point at mock decoder/error/HWM/internal functions and
  local storage; `base()` is zero. Decoder results -1, 65 and INT_MAX are rejected;
  negative/oversized counts never invoke the decoder; invalid queued sizes are
  skipped; valid 0/64-byte changes flush. The mock ERR_DROP returns to test that
  production immediately returns. No detour installation, game memory, or real
  yEnc decoding is tested.
* **Update:** literal production constants/version, parsers, WinHTTP fetch, worker
  and starter functions, stopping before Lua publication. WinHTTP is modeled as
  deterministic handles/read chunks; real `utils::finally` closes them. Exercises
  each acquisition/request failure, a read error **after a complete-looking newer
  release prefix**, empty/exact-limit/oversized bodies, and worker exceptions on
  a real detached thread (condition-variable completion, no sleeps). The actual
  worker's exception catch is tested; OS thread-construction/detach failure is not
  injected. No HTTPS/JSON parser library/game-thread publication is exercised.
* **Lighting:** literal sidecar/lock/reset/accessor/hook functions through the
  start of `install_lightobj_flag_fix`, actual context layout and production
  address constants. Aligned local buffers model object arrays and saved XMM stack
  storage; no lists/views are needed by the address-keyed callbacks. Actual value
  setters receive `rdx = index * stride / 4 + lc` with `rcx` at the array base, or
  `rdx = lc` with `rcx` at the object. Tests both clients 2/3 across two independent
  arrays, index 12347, client-0 fallback before each client's first write, independent
  values/flags, all seven callbacks (including flag restore/save), same-address reset,
  a verified hash collision, and all 32768 slots filled with real object addresses.
  Capacity failure skips writes, reads client 0, preserves existing entries, and
  reset permits insertion again. Entire object buffers remain unchanged by hooks.
  Unrelated registers are varied per site; registers carrying that site's client,
  object, flag, or comparison operand remain meaningful (e.g. value-get `rbx`,
  flag-set `r10`, restore `r14`). Clients 0/1 leave context/XMM/storage untouched,
  but their displaced stock instructions are not executed. Maintains 20,000
  concurrent reset/read/write iterations with stable buffers. Machine-code
  trampolines, instruction execution/Windows ABI, patch installation, memory-free
  callbacks, invalid/unmapped pointers, and real game object lifetime are not
  simulated; same-address replacement is modeled by resetting then changing stock
  fields, not freeing live objects. This stress test plus UBSan is not a race
  detector or proof of race freedom; logging is a no-op mock, so diagnostic text
  and emission count are not tested.
* **Runtime:** includes **all of actual runtime.cpp**, with actual compatibility
  headers, without source-rewriting or compiler-cast shims. Windows calls and
  MinHook outcomes are mocked. Tests pre-write refusal,
  post-write protection/flush failures (flush still attempted), detour initialization,
  creation/enabling failures and cleanup, scheduler throwing-callback removal,
  reentrant task addition, renderer forwarding, component activation order/server
  exclusion, release file restrictions, unsupported-build retry and consumed
  supported-start failure. Startup's injected hook failure occurs before worker
  creation; the infinite async scheduler driver and real executable allocation
  are not launched/tested. Process-lifetime storage intentionally leaks as in production.
* **Plugin:** includes **all of actual ezz_plugin.cpp**. Mock VirtualQuery models
  a single allocation/region; PE fixtures use a minimal field model, **not SDK
  binary layouts**. Tests import/IAT bounds, missing original name table, ordinal
  skipping, unterminated strings, malformed headers/directories, page protection,
  allocation mismatch, overflow rejection and trigger installation. Mock runtime
  tests retry, exception containment, forwarding and concurrent one-time startup.
  Real PE ABI, multi-region mappings, loader lock, DLL pinning, and Windows calling
  conventions remain covered only by the existing Windows smoke/integration tests.

Selections are copied verbatim into temporary headers; unique boundary markers
fail closed if removed/repeated/reordered. No test-owned replicas of the production
algorithms are substituted. Runtime/plugin scenarios use fresh processes to reset
their production one-time statics without modifying production code.

## Optional Lua

If `lupa` is already installed in the invoking Python environment, the runner
executes the actual table-insert script in fresh Lua runtimes with and without
`select`. Checks nil no-ops, real value/positional forwarding, and the documented
ambiguous fallback behavior. Missing `lupa` prints an explicit SKIP, not a claimed
pass. `--require-lua` makes absence a failure. The runner never installs packages.

The lobby suite also compiles and executes the **entire actual** splitscreen UI
script, with fresh Lua globals per scenario. Lua LUI elements implement handler
registration, consume-before-child dispatch and model subscriptions. Engine model
trees provide stable per-controller roots, `GetModel`/`CreateModel`, values and
ordered synchronous change notifications, including nested `SetModelValue(0)`
reentry; unchanged values do not notify. Engine/Dvar/queue doubles provide
deterministic controller seats, active pads, pad ports and limits. Independent
stock join/leave doubles record exact menus and controllers;
joins normally remain asynchronous, with opt-in synchronous seat publication and
reentrant callbacks. Toggle scenarios install room/stock-toggle mocks before
production loads; the stock-toggle double records calls and can maliciously seat
controller 1 to detect accidental delegation. Tests exercise the exported
wrapped `LobbyAddLocalClient`, `LobbySplitscreenToggle`, `CoD.Menu.HandleButtonPress`, and real registered
watcher handlers: delayed-seat deduplication across menus, distinct pending pads,
pending/seated port conflicts, unknown ports, invalid/used controllers, online or
missing network mode, full lobbies, busy queues, retry at exactly six ticks of
**one** watcher (not both roots), consumed join events, A-only/model reset and
missing-enum denial, localization with `pcall = nil`, and already-used controller
navigation. Guest B tests cover each controller's targeted removal, busy-queue
tick retry, repeated/reentrant B deduplication **within each epoch**, sent/unsent six-tick timeouts,
host/submenu/online/in-game stock navigation, stale join cleanup and immediate
targeted rejoin. Toggle tests cover synchronous A-seat publication with reentrant
and subsequent toggle callbacks, first-free active-pad selection with controller 1
absent, shared add reservations, any pending join's expiration, unsent/sent leave
blocking, and leaving two guests then rejoining only one.

Model-driven scenarios optionally install `LUI.createMenu.Lobby`. Its stock
button subscriptions capture `Engine.GetMaxLocalControllers()` at construction:
one (host-only, reproducing a Zombies menu built before native widening) or four.
The bound widens before the outer ezz constructor installs its subscriptions.
`CoD.BitUtility` and `Enum.LUIButtonFlags.FLAG_DOWN` distinguish DOWN from other
flag bits. These scenarios notify actual fixture models rather than invoking
`CoD.Menu.HandleButtonPress` directly:

* The fixture reproduces the relevant behavior of the installed ezz
  `splitscreen_join` integration, inspected read-only on 2026-11-24: A arms on
  DOWN and joins on **release** using `LobbySplitscreenToggle(menu, joining)`.
  Both stock A-press and ezz A-release routes cover every guest, synchronous
  zero-reset reentry, other free pads, expired reservations, late seated-guest
  toggles and guarded host clicks. A release-only case also tests synchronous
  seat publication and a reentrant host click. Controller 0's fresh generic
  first-free-pad behavior remains covered by the existing toggle cases.
* Zombies menus with no stock guest B subscription still remove each guest via
  production's constructor-installed direct subscriptions. Repeated B and direct
  global remove calls defer while the lobby queue is busy, preserve the original
  menu and deadline, and cannot resend within an epoch. A refused removal is
  retried on the next tick and stops once the seat disappears. The legacy
  `sent_leave_expires_at_six_ticks` case now permits **at most six attempts**, not
  one, and verifies that timeout stops retries until a new request arrives.
* Occlusion and both input-disable flags block hidden/disabled menu handling;
  online and in-game B retain mapped stock navigation without guest removal.
  Clearing an overlay/disable flag preserves subsequent navigation.
* Installing ezz's remove wrapper **after** production, then ticking to rewrap
  it, must neither recurse through the captured older production wrapper nor
  lose a queued removal. Both the gamepad-host delegation chain and keyboard-host
  direct `CoD.LobbyBase` removal branch are checked, including a second request.

An optional stock-shaped `CoD.FE_ListAdditonal.new` double supplies the PC toggle,
`StringA` hint, an unrelated control, state conditions, and captured parent clips.
The production constructor wrapper must pin only `SplitscreenLobbyButtonPC` to
alpha zero even after direct and parent-clip `setAlpha(1)` calls, disable its mouse
and focus handling, and consume both `button_action` and `mouse_left_click`
without toggling or joining. Only `SplitscreenButtonPC` becomes an always-false
condition. The hint's text, alpha, focus and action handler, the unrelated control,
and unrelated state predicates remain intact. An ezz-shaped constructor wrapper
adds `AddControllerState_Play`; its predicate identity and true/false behavior
must survive both initial installation and late rewrapping. Repeated watcher
ticks retain constructor identity and exact constructor-call counts. All existing
A-down/ezz-A-release and guest-B scenarios remain present.

The ezz reproduction is a test-owned integration double, not copied production
helpers. It omits module loading; the installed external script is never executed
or modified by the tests.
No production splitscreen helpers are extracted or reimplemented. LUI routing,
stock join/leave/seat publication, game timers, models and sandbox behavior are
mocks, not real game integration;
controller-list/update-dialog paths are not covered by this suite.
`lupa` supplies its bundled Lua runtime, not the
game's VM. Use the existing validation environment's Python with `--require-lua`
when Lua coverage must not be skipped.

### Isolated settings menu

The runner also executes the entire production settings script from
`src/ui_scripts/zz_splitscreen_settings/__init__.lua` using
`regression_settings.lua`, with a fresh VM per scenario. The existing
`zz_splitscreen` script is not changed by this feature. Coverage includes all four
slots without signed-in guests, displayed current values, prefill, nil/flagged
cancel versus accepted empty reset, both four-slot presets, native readiness/error
gating, strict 31-byte UTF-8 validation, offline frontend guards, ModeSelect and
Zombies/LAN target lookup, existing button/callback preservation, duplicate
prevention, late callback installation/replacement, stable focus and subscriptions,
and menu timer/button-model cleanup. Stock-shaped keyboard tests simulate engine
scratch-buffer initialization, explicit accept without an input event, overlay
cancellation without an input event, and duplicate completion notifications.
`pcall`, `io`, and `os` are absent in these fixtures.

Offline guards require `core_frontend` and numeric network mode 0 or 1, independent
of a missing or conflicting `Enum.LobbyNetworkMode`. Missing `Engine.IsInGame`
still permits visible/editable settings in both offline modes; an available API
returning true blocks them. Missing required map/network APIs, nil/unknown/online
modes, string-valued modes, and non-frontend maps fail closed. Both `UIRoot0` and
`UIRootFull` receive exactly one watcher, including when the full root appears
late; the late full-root timer independently captures a replacement callback.
`ForceLobbyButtonUpdate` records the primary controller once per offline callback
wrap, never on idempotent ticks or an online wrap. Duplicate script execution
does not add timers or refreshes.

**Keyboard verification boundary:** the inspected PC ship `actions.lua` defines
`ShowKeyboard(menu, element, controller, keyboardName)` using `ui_keyboard_new`
and an existing `Enum.KeyboardType` entry. Its `UIKeyboardComplete` issues
`ui_keyboard_complete`; the stock PC overlay calls that helper only for explicit
acceptance. `InputTextField` reads `ui_keyboard_dvar_edit`. Those contracts are
modeled, not executed from the game. The inspected `lua/Shared/LuaEnums.lua`
does not define keyboard enums, and the availability/engine semantics of
`KEYBOARD_TYPE_EDIT_DVAR` **have not been established**. The production script
never creates an enum or guesses a number: name editing fails closed with a
visible message if that entry is missing. The fixture's value `7` is deliberately
synthetic, not evidence that the game supports the type. Presets/reset do not
require a keyboard. Live keyboard availability, rendering/navigation, real timer
delivery, native save-poll timing, and persistence across restarts remain
unverified; passing these mocks must not be represented as game validation.

Validation on 2026-11-24: the existing Lua-enabled validation environment ran
`tools/run_regression_tests.py --require-lua` with default GCC/UBSan trap
instrumentation: **14 native scenarios, 49 lobby Lua scenarios and both
table-insert Lua variants passed; zero failures, no Lua skips**.

Settings follow-up validation: the same required-Lua runner passed **15 native
UBSan scenarios, 49 lobby Lua scenarios, 13 settings Lua scenarios, and both
table-insert variants**, with zero failures and no skips. No game launch,
deployment, installed-file edit, process termination, or commit was performed.

Tests-only guard/widget follow-up validation on 2026-11-24: the full runner with
`--require-lua` passed **15 native UBSan scenarios, 51 lobby Lua scenarios,
20 settings Lua scenarios, 11 roster Lua scenarios, and both table-insert
variants**: **zero failures and zero skips**. Changes are limited to the two Lua
regression suites and this test document. No production edits, commit, game
launch, or deployment were performed. These results validate mocks, not live
widget rendering, input routing, or engine API availability.