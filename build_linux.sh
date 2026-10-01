#!/usr/bin/env bash
# Cross-build the Windows plugin on Linux with LLVM-MinGW.
set -euo pipefail
cd -- "$(dirname -- "$0")"
: "${LLVM_MINGW:?Set LLVM_MINGW to the extracted LLVM-MinGW directory}"
: "${MINHOOK:?Set MINHOOK to the MinHook source directory}"
mode=${1:-release}
case "$mode" in
  release) diag=() ;;
  diag) diag=(-DSS_DIAG) ;;
  *) echo 'Usage: build_linux.sh [release|diag]' >&2; exit 1 ;;
esac
cc="$LLVM_MINGW/bin/x86_64-w64-mingw32-clang"
cxx="$LLVM_MINGW/bin/x86_64-w64-mingw32-clang++"
obj="out/linux-$mode"
mkdir -p "$obj/include"
# The upstream MSVC source uses Windows.h; Linux filesystems are case sensitive.
printf '#include <windows.h>\n' > "$obj/include/Windows.h"
printf '#include <tlhelp32.h>\n' > "$obj/include/TlHelp32.h"
flags=(-Os -DNDEBUG -DWIN32 -D_WINDOWS -ffunction-sections -fdata-sections -I"$obj/include" "${diag[@]}")
for source in buffer hook trampoline; do
  "$cc" "${flags[@]}" -I"$MINHOOK/include" -c "$MINHOOK/src/$source.c" -o "$obj/$source.o"
done
"$cc" "${flags[@]}" -c "$MINHOOK/src/hde/hde64.c" -o "$obj/hde64.o"
"$cxx" "${flags[@]}" -std=c++20 -Isrc/standalone/compat -c src/component/splitscreen.cpp -o "$obj/splitscreen.o"
for source in runtime ezz_plugin; do
  "$cxx" "${flags[@]}" -std=c++20 -I"$MINHOOK/include" -c "src/standalone/$source.cpp" -o "$obj/$source.o"
done
"$cxx" -shared -static -Wl,--gc-sections -o "$obj/bo3_local_splitscreen.dll" "$obj/"*.o src/standalone/ezz_plugin.def -lkernel32
"$cxx" "${flags[@]}" -std=c++20 -static src/standalone/test/smoke.cpp -o "$obj/smoke.exe" -luser32
echo "Built $obj/bo3_local_splitscreen.dll and smoke.exe"
