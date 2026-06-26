@echo off
title SiChain - Build EXE
echo.
echo ============================================
echo   SiChain Monitor - EXE Builder
echo ============================================
echo.

REM Check PyInstaller
py -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
    echo [1/2] Installing PyInstaller...
    py -m pip install pyinstaller
    echo.
)

echo [2/2] Building EXE...
py -m PyInstaller --onefile --windowed --name "SiliconChainMonitor" --clean --noconfirm "硅链控制板_上位机.py"

if exist "dist\SiliconChainMonitor.exe" (
    echo.
    echo ============================================
    echo   [OK] Build SUCCESS!
    echo   Output: tools\dist\SiliconChainMonitor.exe
    for %%A in ("dist\SiliconChainMonitor.exe") do echo   Size: %%~zA bytes
    echo ============================================
) else (
    echo.
    echo   [FAIL] Build failed, check errors above
)

if exist "build" rmdir /s /q "build"
echo.
echo Copy dist\SiliconChainMonitor.exe to USB drive to run standalone
pause
