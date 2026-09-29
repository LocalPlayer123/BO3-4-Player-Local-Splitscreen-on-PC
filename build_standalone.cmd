@echo off
rem Builds the ezz BOIII plugin out\bo3_local_splitscreen.dll from src\ - the
rem same flags as the tested packages.
rem
rem   Run from an "x64 Native Tools Command Prompt for VS 2022".
rem   set MINHOOK=<path to the MinHook source>   (github.com/TsudaKageyu/minhook)
rem   build_standalone.cmd          player build (no files written by the DLL)
rem   build_standalone.cmd diag     diagnostic build (trace in %%LOCALAPPDATA%%\boiii)
setlocal
if "%MINHOOK%"=="" (
    echo Set MINHOOK to the MinHook source folder first.
    exit /b 1
)
set DIAG=
if /i "%~1"=="diag" set DIAG=/DSS_DIAG
set CC=/nologo /c /O1 /GL /Gy /Gw /MT /DNDEBUG /DWIN32 /D_WINDOWS /W3 /Brepro %DIAG%
set CPP=%CC% /std:c++20 /EHsc /Zc:__cplusplus
cd /d "%~dp0"
if not exist out\obj mkdir out\obj
del /q out\obj\*.obj 2>nul

cl %CC% /I"%MINHOOK%\include" /Foout\obj\ "%MINHOOK%\src\buffer.c" "%MINHOOK%\src\hook.c" "%MINHOOK%\src\trampoline.c" "%MINHOOK%\src\hde\hde64.c" || exit /b 2
cl %CPP% /I"src\standalone\compat" /Foout\obj\ "src\component\splitscreen.cpp" || exit /b 3
cl %CPP% /I"%MINHOOK%\include" /Foout\obj\ "src\standalone\runtime.cpp" "src\standalone\ezz_plugin.cpp" || exit /b 4
link /nologo /DLL /LTCG /OPT:REF /OPT:ICF /DEBUG:NONE /Brepro /MANIFEST:NO /MACHINE:X64 /DEF:"src\standalone\ezz_plugin.def" /OUT:out\bo3_local_splitscreen.dll /IMPLIB:out\obj\bo3_local_splitscreen.lib out\obj\*.obj kernel32.lib || exit /b 5

echo.
echo Built out\bo3_local_splitscreen.dll
echo Ship it as boiii\plugins\bo3_local_splitscreen.dll together with
echo src\ui_scripts\zz_table_insert, zz_splitscreen and zz_mplan (as
echo boiii\ui_scripts\...), see docs\PLAYER_README.txt.
