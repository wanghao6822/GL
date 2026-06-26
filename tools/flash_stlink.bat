@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo.
echo  ╔══════════════════════════════════════════╗
echo  ║   硅链控制板 - 一键烧录工具 (ST-Link)    ║
echo  ╚══════════════════════════════════════════╝
echo.

REM ============================================================
REM 1. 定位 PlatformIO 安装目录
REM ============================================================
set "PIO_DIR="

REM 尝试1: 默认路径
if exist "%USERPROFILE%\.platformio" (
    set "PIO_DIR=%USERPROFILE%\.platformio"
)

REM 尝试2: 其他盘符
if not defined PIO_DIR (
    for %%d in (C D E F G) do (
        if exist "%%d:\.platformio" (
            set "PIO_DIR=%%d:\.platformio"
        )
    )
)

if not defined PIO_DIR (
    echo [✗] 未找到 PlatformIO 安装目录
    echo.
    echo 请先安装 VSCode + PlatformIO 扩展
    echo 或手动运行: pio run --target upload
    pause
    exit /b 1
)

REM ============================================================
REM 2. 定位 OpenOCD
REM ============================================================
set "OPENOCD=%PIO_DIR%\packages\tool-openocd\bin\openocd.exe"
set "OCD_SCRIPTS=%PIO_DIR%\packages\tool-openocd\openocd\scripts"

if not exist "%OPENOCD%" (
    echo [✗] 未找到 OpenOCD: %OPENOCD%
    pause
    exit /b 1
)

REM ============================================================
REM 3. 定位固件文件
REM ============================================================
set "HEX_FILE=%~dp0firmware\firmware.hex"

if not exist "%HEX_FILE%" (
    echo [✗] 未找到固件文件: %HEX_FILE%
    echo.
    echo 请先在 VSCode 中编译项目 (Ctrl+Alt+B)
    echo 或手动复制 firmware.hex 到 tools\firmware\
    pause
    exit /b 1
)

echo [✓] PlatformIO: %PIO_DIR%
echo [✓] 固件文件: firmware\firmware.hex
echo.

REM ============================================================
REM 4. 烧录
REM ============================================================
echo [→] 正在通过 ST-Link 烧录...

"%OPENOCD%" ^
    -s "%OCD_SCRIPTS%" ^
    -f interface/stlink.cfg ^
    -f target/stm32f1x.cfg ^
    -c "program %HEX_FILE% verify reset exit" ^
    2>&1

if errorlevel 1 (
    echo.
    echo [✗] 烧录失败！请检查:
    echo   1. ST-Link 是否正确连接
    echo   2. 控制板是否已上电
    echo   3. 设备管理器中是否有 ST-Link 设备
    pause
    exit /b 1
)

echo.
echo  ╔══════════════════════════════════════════╗
echo  ║         ✓ 烧录成功！设备已复位运行        ║
echo  ╚══════════════════════════════════════════╝
echo.
pause
