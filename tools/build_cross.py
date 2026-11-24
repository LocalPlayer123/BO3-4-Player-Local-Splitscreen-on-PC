#!/usr/bin/env python3
"""Build the Windows x64 plugin on Linux using an existing LLVM-MinGW and MinHook.

Does not download dependencies or install into the game. Outputs DLL, PDB and
Windows smoke executable in out/cross-diag (or out/cross-release).
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--minhook", type=Path, required=True)
    parser.add_argument("--release", action="store_true", help="Omit diagnostic trace output")
    args = parser.parse_args()
    toolchain, minhook = args.toolchain.resolve(), args.minhook.resolve()
    cc = toolchain / "bin/x86_64-w64-mingw32-clang"
    cxx = toolchain / "bin/x86_64-w64-mingw32-clang++"
    for path in (cc, cxx, minhook / "include/MinHook.h"):
        if not path.is_file():
            parser.error(f"Missing build prerequisite: {path}")
    out = ROOT / "out" / ("cross-release" if args.release else "cross-diag")
    obj = out / "obj"
    obj.mkdir(parents=True, exist_ok=True)
    # MSVC accepts Windows.h/TlHelp32.h; Linux SDK files are lowercase. Supply
    # aliases only in the generated build directory, not in production headers.
    aliases = obj / "sdk-case"
    aliases.mkdir(exist_ok=True)
    sdk = toolchain / "generic-w64-mingw32/include"
    for name in ("Windows.h", "TlHelp32.h"):
        alias = aliases / name
        if not os.path.lexists(alias):
            alias.symlink_to(sdk / name.lower())
    flags = ["-O1", "-gcodeview", "-ffunction-sections", "-fdata-sections",
             "-fms-extensions", "-DWIN32", "-D_WINDOWS", "-DNDEBUG",
             "-Wno-unknown-pragmas", "-I", str(aliases), "-I", str(minhook / "include")]
    if not args.release:
        flags.append("-DSS_DIAG")
    objects = []
    for name in ("buffer.c", "hook.c", "trampoline.c", "hde/hde64.c"):
        target = obj / (Path(name).stem + ".o")
        run([str(cc), *flags, "-c", str(minhook / "src" / name), "-o", str(target)])
        objects.append(str(target))
    cppflags = [*flags, "-std=c++20", "-I", str(ROOT / "src/standalone/compat")]
    for name in ("src/component/splitscreen.cpp", "src/standalone/runtime.cpp", "src/standalone/ezz_plugin.cpp"):
        target = obj / (Path(name).stem + ".o")
        run([str(cxx), *cppflags, "-c", str(ROOT / name), "-o", str(target)])
        objects.append(str(target))
    run([str(cxx), "-shared", "-static", *objects,
         str(ROOT / "src/standalone/ezz_plugin.def"), "-lwinhttp", "-lkernel32",
         "-Wl,--gc-sections", "-Wl,--no-insert-timestamp",
         "-Wl,--pdb=" + str(out / "bo3_local_splitscreen.pdb"),
         "-o", str(out / "bo3_local_splitscreen.dll")])
    run([str(cxx), *cppflags, "-static", str(ROOT / "src/standalone/test/smoke.cpp"),
         "-luser32", "-lkernel32", "-Wl,--no-insert-timestamp", "-o", str(out / "smoke.exe")])
    print(f"Built {out}; run smoke.exe under Windows/Proton before deployment.")


if __name__ == "__main__":
    main()