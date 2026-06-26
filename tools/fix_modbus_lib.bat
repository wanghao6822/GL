@echo off
title Fix Modbus Library
echo.
echo ============================================
echo   Fix Modbus Library (private -> public)
echo ============================================
echo.
echo This fixes Modbus-Arduino so RTU and TCP
echo can share register memory.

REM Find Modbus.h in PlatformIO cache
set "MODBUS_H="
for /r "%USERPROFILE%\.platformio" %%f in (Modbus.h) do (
    findstr /m "_regs_head" "%%f" >nul 2>&1
    if !errorlevel! equ 0 set "MODBUS_H=%%f"
)

if "!MODBUS_H!"=="" (
    echo [FAIL] Modbus.h not found.
    echo Run "pio run" first to download libraries.
    goto end
)

echo Found: !MODBUS_H!

REM Check if fix needed
findstr /c:"public:" "!MODBUS_H!" >nul 2>&1
if errorlevel 1 goto needfix

findstr /c:"TRegister *_regs_head" "!MODBUS_H!" >nul 2>&1
if errorlevel 1 goto needfix

echo [OK] Already patched - no fix needed
goto end

:needfix
echo [>>] Patching...
REM Backup original
copy "!MODBUS_H!" "!MODBUS_H!.bak" >nul

REM PowerShell: replace private: before _regs_head with public:
powershell -NoProfile -Command ^
  "$f='!MODBUS_H!'; $c=Get-Content $f -Raw; $c=$c -replace 'private(\s*:\s*\r?\n\s*)(TRegister\s+\*_regs_head)', 'public$$1$$2'; [IO.File]::WriteAllText($f, $c); Write-Host '[OK] Done'"

:end
echo.
pause
