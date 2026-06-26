@echo off
setlocal enabledelapsedexpansion
title Fix Modbus Library

echo.
echo ============================================
echo   Fix Modbus Library
echo ============================================
echo.
echo This replaces Modbus-Arduino/src/Modbus.h
echo with a pre-patched version (private -> public)
echo.

set "PATCHED=%~dp0Modbus_Patched.h"

if not exist "%PATCHED%" (
    echo [FAIL] Modbus_Patched.h not found in tools\
    pause
    exit /b 1
)

set "COUNT=0"

REM Fix all copies in PlatformIO cache
for /r "%USERPROFILE%\.platformio" %%f in (Modbus.h) do (
    findstr /c:"_regs_head" "%%f" >nul 2>&1
    if !errorlevel! equ 0 (
        findstr /c:"public:  // patched" "%%f" >nul 2>&1
        if !errorlevel! neq 0 (
            echo Patching: %%~nxf
            copy /y "%PATCHED%" "%%f" >nul
            set /a COUNT+=1
        )
    )
)

REM Fix all copies in project .pio folder
for /r "%~dp0..\.pio" %%f in (Modbus.h) do (
    findstr /c:"_regs_head" "%%f" >nul 2>&1
    if !errorlevel! equ 0 (
        findstr /c:"public:  // patched" "%%f" >nul 2>&1
        if !errorlevel! neq 0 (
            echo Patching: %%~nxf
            copy /y "%PATCHED%" "%%f" >nul
            set /a COUNT+=1
        )
    )
)

echo.
if !COUNT! equ 0 (
    echo [OK] Already patched - no changes needed.
) else (
    echo [OK] Patched !COUNT! file(s).
)
echo.
echo Now rebuild in VSCode (Ctrl+Alt+B).
pause
