#!/usr/bin/env python3
"""Exercise the unchanged production overhead hook with mocked game/patch APIs."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

from run_regression_tests import ROOT, TEST, section


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "/usr/bin/g++"))
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="splitscreen-match-names-") as temp:
        directory = Path(temp)
        (directory / "production_match_byte_check.hpp").write_text(section(
            "src/component/splitscreen/13_lobby_join.inl",
            "\t\ttemplate <size_t N>\n\t\tbool engine_bytes_match", "\t\t// LiveUser_GetXuid"))
        exe = directory / "match_names"
        command = shlex.split(args.cxx) + ["-std=c++17", "-O1", "-g", "-pthread",
            "-Wall", "-Wextra", "-fsanitize=undefined", "-fsanitize-undefined-trap-on-error",
            "-I", str(directory), "-I", str(ROOT / "src"), "-I", str(TEST),
            str(TEST / "regression_match_names.cpp"), "-o", str(exe)]
        subprocess.run(command, check=True, timeout=120)
        subprocess.run([str(exe)], check=True, timeout=120)


if __name__ == "__main__":
    main()