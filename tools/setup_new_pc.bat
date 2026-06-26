@echo off
setlocal enabledelayedexpansion
title SiChain - One-Click Setup

echo.
echo ============================================
echo   SiChain Project - Offline Setup
echo ============================================
echo.

set "SDK=%~dp0.."
set "ERR=0"

REM ============================================================
REM Step 1: Check project path (no Chinese characters!)
REM ============================================================
echo [1/4] Checking project path...
echo   %SDK%
echo %SDK% | findstr /r "[^\x00-\x7F]" >nul 2>&1
if not errorlevel 1 (
    echo   [FAIL] Path contains non-ASCII characters!
    echo.
    echo   ARM GCC does NOT support Chinese paths.
    echo   Please move the project to a path like:
    echo     C:\Users\DELL\GL
    echo     D:\GL
    echo.
    set "ERR=1"
) else (
    echo   [OK] Path is clean
)

if "!ERR!"=="1" goto failed

REM ============================================================
REM Step 2: Link PlatformIO packages
REM ============================================================
echo.
echo [2/4] Setting up PlatformIO packages...

set "PIO_HOME=%USERPROFILE%\.platformio"
set "PIO_SDK=%SDK%\.platformio"

if not exist "%PIO_SDK%" (
    echo   [FAIL] .platformio folder not found in SDK
    echo   Make sure the GL_Portable_SDK folder is complete
    set "ERR=1"
    goto failed
)

if exist "%PIO_HOME%" (
    echo   Existing .platformio found, backing up...
    if exist "%PIO_HOME%.bak" rmdir /s /q "%PIO_HOME%.bak"
    move "%PIO_HOME%" "%PIO_HOME%.bak" >nul 2>&1
)

mklink /J "%PIO_HOME%" "%PIO_SDK%" >nul 2>&1
if errorlevel 1 (
    echo   Symlink failed, copying packages...
    mkdir "%PIO_HOME%" 2>nul
    robocopy "%PIO_SDK%" "%PIO_HOME%" /E /NFL /NDL
    echo   [OK] Packages copied
) else (
    echo   [OK] Packages linked (saves disk space)
)

REM ============================================================
REM Step 3: Verify key components
REM ============================================================
echo.
echo [3/4] Verifying components...

set "OK=1"
if not exist "%PIO_HOME%\penv\Scripts\platformio.exe" (
    echo   [FAIL] PlatformIO CLI missing
    set "OK=0"
)
if not exist "%PIO_HOME%\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-gcc.exe" (
    echo   [FAIL] ARM GCC compiler missing
    set "OK=0"
)
if not exist "%PIO_HOME%\packages\framework-arduinoststm32" (
    echo   [FAIL] Arduino STM32 framework missing
    set "OK=0"
)

if "!OK!"=="0" (
    echo   SDK files incomplete. Please re-copy GL_Portable_SDK.
    set "ERR=1"
    goto failed
)
echo   [OK] All components present

REM ============================================================
REM Step 4: Test build
REM ============================================================
echo.
echo [4/4] Testing build...

cd /d "%SDK%"
call "%PIO_HOME%\penv\Scripts\platformio.exe" run 2>&1
if errorlevel 1 (
    echo.
    echo   [FAIL] Build failed
    set "ERR=1"
    goto failed
)

REM ============================================================
REM Success!
REM ============================================================
echo.
echo ============================================
echo   Setup SUCCESS!
echo ============================================
echo.
echo   What you can do now:
echo.
echo   1. Open GL folder with VSCode
echo      File - Open Folder - select GL
echo.
echo   2. Build: Ctrl+Alt+B (or click checkmark)
echo   3. Flash: Ctrl+Alt+U (or click arrow)
echo      Requires ST-Link driver!
echo.
echo   4. Run monitor: double-click SiliconChainMonitor.exe
echo.
echo   NOTE: First time using ST-Link?
echo   Download Zadig (https://zadig.akeo.ie/)
echo   Select STM32 STLink - install WinUSB driver
echo.
goto end

REM ============================================================
REM Failed
REM ============================================================
:failed
echo.
echo ============================================
echo   Setup FAILED - see errors above
echo ============================================
echo.
echo   Common fixes:
echo   - Move project to a path without Chinese characters
echo   - Make sure GL_Portable_SDK folder is complete
echo   - Run as Administrator for symlink creation
echo.

:end
echo.
pause
