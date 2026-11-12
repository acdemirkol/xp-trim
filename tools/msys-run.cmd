@echo off
rem Run a command inside the MSYS2 MINGW32 environment from cmd/PowerShell,
rem in the current directory. Example:  tools\msys-run.cmd make test
rem Set MSYS2_ROOT if MSYS2 is not installed in C:\msys64.
setlocal
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
set "MSYSTEM=MINGW32"
set "CHERE_INVOKING=1"
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "%*"
