@echo off
title 硅链控制板 - 离线环境安装
echo.
echo ============================================
echo   硅链控制板 - 离线开发环境安装
echo ============================================
echo.

set "SDK=%~dp0"
set "PIO_HOME=%USERPROFILE%\.platformio"

REM --- 1. 检查旧 PlatformIO ---
if exist "%PIO_HOME%" (
    echo [!] 已有旧的 .platformio
    choice /c yn /m "    是否备份旧目录并替换 (Y/N)?"
    if errorlevel 2 goto skip_pio
    if exist "%PIO_HOME%.bak" rmdir /s /q "%PIO_HOME%.bak"
    move "%PIO_HOME%" "%PIO_HOME%.bak" >nul 2>&1
)

REM --- 2. 创建目录链接 ---
echo [1/3] 链接 PlatformIO 离线包...
mklink /J "%PIO_HOME%" "%SDK%.platformio" >nul 2>&1
if errorlevel 1 (
    echo       链接失败，改用复制模式...
    mkdir "%PIO_HOME%" 2>nul
    robocopy "%SDK%.platformio" "%PIO_HOME%" /E /NFL /NDL
    echo       复制完成
) else (
    echo       链接完成 (节省磁盘空间)
)

:skip_pio

REM --- 3. 验证 ---
echo [2/3] 验证关键文件...

if not exist "%PIO_HOME%\penv\Scripts\platformio.exe" (
    echo [FAIL] PlatformIO CLI 未找到
    echo        请确认 GL_Portable_SDK 文件夹完整
    pause
    exit /b 1
)

if not exist "%PIO_HOME%\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-gcc.exe" (
    echo [FAIL] ARM GCC 编译器未找到
    pause
    exit /b 1
)
echo       OK

REM --- 4. 测试编译 ---
echo [3/3] 测试编译...
cd /d "%SDK%GL"
call "%PIO_HOME%\penv\Scripts\platformio.exe" run 2>&1
if errorlevel 1 (
    echo.
    echo [FAIL] 编译失败，请检查错误信息
    pause
    exit /b 1
)

echo.
echo ============================================
echo    安装成功！
echo ============================================
echo.
echo  - 用 VSCode 打开 GL 文件夹即可开发
echo  - flash_stlink.bat  一键烧录
echo  - SiliconChainMonitor.exe  上位机监控
echo  - 项目交接文档.md  完整文档
echo.
pause
