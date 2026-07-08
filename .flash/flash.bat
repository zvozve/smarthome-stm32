@echo off
set JL=C:\PROGRA~1\SEGGER\JLINK_V~1\JLink.exe
if not exist "%JL%" set JL=C:\PROGRA~2\SEGGER\JLINK_V~1\JLink.exe
:: Fallback to exact path
if not exist "%JL%" set "JL=C:\Program Files\SEGGER\JLink_V798b\JLink.exe"

set ELF=%~dp0..\build\Release\SmartHome.elf
if not exist "%ELF%" (
    echo ERROR: %ELF% not found
    echo Run cmake configure + build first.
    pause
    exit /b 1
)

echo Flashing (incremental, no full erase)...
set JL_OPTS=-device STM32F407ZG -if SWD -speed 4000 -autoconnect 1 -nogui 1

(
echo r
echo h
echo loadfile "%ELF%"
echo r
echo g
echo exit
) | "%JL%" %JL_OPTS%

if errorlevel 1 (
    echo FAILED!
    pause
    exit /b 1
)
echo Done!
