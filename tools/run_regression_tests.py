#!/usr/bin/env python3
"""Offline production-code regression tests; see src/standalone/test/REGRESSION.md.

Only generated headers/executables in TemporaryDirectory are written at runtime.
Source selections are literal, fail-closed slices, not rewritten implementations.
"""
from __future__ import annotations

import argparse
import importlib.util
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TEST = ROOT / "src/standalone/test"


def section(path: str, start: str, end: str) -> str:
    text = (ROOT / path).read_text(encoding="utf-8")
    if text.count(start) != 1 or text.count(end) != 1:
        raise RuntimeError(f"Production selection markers changed: {path}")
    first, last = text.index(start), text.index(end)
    if first >= last:
        raise RuntimeError(f"Production selection markers reordered: {path}")
    return text[first:last]


def generate(directory: Path) -> None:
    selections = {
        "stats": section("src/component/splitscreen/15_stats_cache.inl",
                         "\t\tstruct stats_change", "\t\t// Every slot-0 reference"),
        "update": section("src/component/splitscreen/16_update_check.inl",
                          "\t\tconstexpr wchar_t update_host", "\t\t// The Lua shows the notice"),
        "lighting": section("src/component/splitscreen/17_lighting_and_restart.inl",
                            "\t\tconstexpr uint32_t lightobj_max", "\t\tvoid install_lightobj_flag_fix()"),
        "context": section("src/component/splitscreen_midhook.hpp",
                           "\tstruct context", "\tusing callback"),
    }
    # Also use the real release version and real layout/continuation constants.
    update = (ROOT / "src/component/splitscreen/16_update_check.inl").read_text()
    version = re.findall(r'^\s*constexpr char mod_version\[\] = .*;', update, re.M)
    addresses = (ROOT / "src/component/splitscreen_addresses.hpp").read_text()
    constants = re.findall(r'^\s*inline constexpr uint32_t lightobj_\w+ = .*;', addresses, re.M)
    if len(version) != 1 or not constants:
        raise RuntimeError("Production version/layout constants changed")
    selections["update"] = version[0] + "\n" + selections["update"]
    selections["lighting"] = "\n".join(constants) + "\n" + selections["lighting"]
    for name, text in selections.items():
        (directory / f"production_{name}.hpp").write_text(text, encoding="utf-8")


def run(command: list[str]) -> None:
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True, timeout=120)


def lua_test() -> bool:
    if importlib.util.find_spec("lupa") is None:
        print("SKIP optional Lua: lupa is not installed (native tests unaffected)", flush=True)
        return False
    from lupa import LuaRuntime
    source = (ROOT / "src/ui_scripts/zz_table_insert/__init__.lua").read_text()
    for has_select in (True, False):
        lua = LuaRuntime()
        lua.execute("calls = 0; raw = table.insert; table.insert = function(...) calls = calls + 1; return raw(...) end")
        if not has_select:
            lua.execute("select = nil")
        lua.execute(source)
        lua.execute("t = {}; table.insert(nil, 1); table.insert(t); table.insert(t, nil); assert(calls == 0)")
        lua.execute("table.insert(t, 42); table.insert(t, 1, 'front'); assert(t[1] == 'front' and t[2] == 42 and calls == 2)")
        if has_select:
            lua.execute("table.insert(t, 1, nil); assert(#t == 2 and calls == 2)")
        else:
            lua.execute("table.insert(t, 1, nil); assert(t[3] == 1 and calls == 3)")
    print("PASS optional Lua: select and documented ambiguous fallback", flush=True)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "/usr/bin/g++"),
                        help="Compiler command (supports flags, e.g. clang++ -stdlib=libc++)")
    parser.add_argument("--no-sanitize", action="store_true", help="Disable default UBSan trap instrumentation")
    parser.add_argument("--require-lua", action="store_true", help="Fail rather than skip when lupa is unavailable")
    args = parser.parse_args()
    flags = ["-std=c++17", "-O1", "-g", "-pthread", "-Wall", "-Wextra"]
    if not args.no_sanitize:
        flags += ["-fsanitize=undefined", "-fsanitize-undefined-trap-on-error"]
    runs = 0
    with tempfile.TemporaryDirectory(prefix="splitscreen-regression-") as temp:
        directory = Path(temp)
        generate(directory)
        for suite, scenarios in (
            ("safety", [None]), ("stats", [None]), ("update", [None]),
            ("lighting", [None]),
            ("runtime", ["hooks", "init-fail", "scheduler", "startup", "unsupported"]),
            ("plugin", ["imports", "retry", "exception", "concurrent"]),
        ):
            exe = directory / suite
            run(shlex.split(args.cxx) + flags + ["-I", str(directory), "-I", str(TEST / "mocks"),
                "-I", str(ROOT / "src"), "-I", str(TEST), str(TEST / f"regression_{suite}.cpp"), "-o", str(exe)])
            for scenario in scenarios:
                run([str(exe)] + ([scenario] if scenario else []))
                runs += 1
    lua_ok = lua_test()
    if args.require_lua and not lua_ok:
        return 1
    print(f"PASS: {runs} native scenarios; optional Lua {'passed' if lua_ok else 'skipped'}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", flush=True)
        raise SystemExit(1)