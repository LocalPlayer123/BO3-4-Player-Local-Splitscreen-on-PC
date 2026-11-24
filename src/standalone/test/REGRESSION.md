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
  address constants. Local buffers model objects, lists, views, and saved XMM
  stack storage. Tests actual value/flag hook writes/reads, same-address reset,
  fallback for unaligned indices, and 20,000 concurrent reset/read/write iterations.
  Stock object fields remain unchanged. Machine-code trampolines, patch installation,
  memory-free callbacks, and game object lifetime are not simulated. This concurrent
  stress test plus UBSan is not a race detector or proof of race freedom.
* **Runtime:** includes **all of actual runtime.cpp**, with actual compatibility
  headers. One native-portability shim maps `static_cast` to `reinterpret_cast`
  only while including the detour declaration header: its void-pointer-to-function
  cast is an MSVC extension rejected by GCC. Runtime function bodies are unchanged;
  this does not validate the Windows compiler's cast behavior. Windows calls and
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