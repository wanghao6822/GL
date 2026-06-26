@echo off
setlocal enabledelayedexpansion
title SiChain Flasher (ST-Link)

echo.
echo ============================================
echo   SiChain Control Board - Flash Tool
echo ============================================
echo.

REM --- 1. Locate PlatformIO ---
set "PIO_DIR="

if exist "%USERPROFILE%\.platformio" (
    set "PIO_DIR=%USERPROFILE%\.platformio"
)

if not defined PIO_DIR (
    for %%d in (C D E F G) do (
        if exist "%%d:\.platformio" (
            set "PIO_DIR=%%d:\.platformio"
        )
    )
)

if not defined PIO_DIR (
    echo [FAIL] PlatformIO not found
    echo Please install VSCode + PlatformIO extension first
    pause
    exit /b 1
)

REM --- 2. Locate OpenOCD ---
set "OPENOCD=%PIO_DIR%\packages\tool-openocd\bin\openocd.exe"
set "OCD_SCRIPTS=%PIO_DIR%\packages\tool-openocd\openocd\scripts"

if not exist "%OPENOCD%" (
    echo [FAIL] OpenOCD not found: %OPENOCD%
    pause
    exit /b 1
)

REM --- 3. Locate firmware ---
set "HEX_FILE=%~dp0firmware\firmware.hex"

if not exist "%HEX_FILE%" (
    echo [FAIL] Firmware not found: %HEX_FILE%
    echo.
    echo Please compile in VSCode first (Ctrl+Alt+B)
    echo Or copy firmware.hex to tools\firmware\
    pause
    exit /b 1
)

echo [OK] PlatformIO: %PIO_DIR%
echo [OK] Firmware: firmware\firmware.hex
echo.

REM --- 4. Flash ---
echo [>>] Flashing via ST-Link...

"%OPENOCD%" ^
    -s "%OCD_SCRIPTS%" ^
    -f interface/stlink.cfg ^
    -f target/stm32f1x.cfg ^
    -c "program %HEX_FILE% verify reset exit" ^
    2>&1

if errorlevel 1 (
    echo.
    echo [FAIL] Flash failed! Check:
    echo   1. ST-Link connected?
    echo   2. Board powered on?
    echo   3. ST-Link driver installed?
    pause
    exit /b 1
)

echo.
echo ============================================
echo   [OK] Flash SUCCESS - Board is running
echo ============================================
echo.
pause
