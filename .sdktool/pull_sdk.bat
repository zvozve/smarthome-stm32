@echo off
rem pull_sdk.bat - Windows entry, calls pull_sdk.py
rem Usage: double-click or run from CMD/PowerShell/Git Bash
setlocal
set "TOOL_DIR=%~dp0"
set "PY="

if exist "C:\Users\ZOZE\.workbuddy\binaries\python\versions\3.13.12\python.exe" (
    set "PY=C:\Users\ZOZE\.workbuddy\binaries\python\versions\3.13.12\python.exe"
) else (
    where py >nul 2>nul && set "PY=py"
)
if "%PY%"=="" (
    echo [ERR] python not found (need >= 3.11)
    exit /b 1
)
"%PY%" "%TOOL_DIR%pull_sdk.py" %*
endlocal
