@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo.
echo  ╔══════════════════════════════════════════╗
echo  ║   项目离线移植工具 - 打包 / 安装          ║
echo  ╚══════════════════════════════════════════╝
echo.
echo  请选择操作:
echo    [1] 打包项目 (在这台电脑上打包)
echo    [2] 安装项目 (在新电脑上还原)
echo.
set /p choice="  输入 1 或 2: "

if "%choice%"=="1" goto pack
if "%choice%"=="2" goto install
echo  无效选择
pause
exit /b 1

REM ============================================================
REM 打包模式：复制所有依赖到目标文件夹
REM ============================================================
:pack
set "OUTPUT_DIR=%~dp0..\GL_Portable_SDK"
echo.
echo [→] 打包到: %OUTPUT_DIR%
echo.

REM 1. 项目源码
echo [1/6] 复制项目源码...
robocopy "%~dp0.." "%OUTPUT_DIR%\GL" /E /NFL /NDL /XD ".pio" ".git" "GL_Portable_SDK" "remote-ioguide" >nul
echo    ✓ 项目源码

REM 2. PlatformIO 核心(pip包)
echo [2/6] 复制 PlatformIO 核心...
if exist "%USERPROFILE%\.platformio\penv" (
    robocopy "%USERPROFILE%\.platformio\penv" "%OUTPUT_DIR%\.platformio\penv" /E /NFL /NDL >nul
    echo    ✓ PlatformIO Core
) else (
    echo    ✗ 未找到 .platformio\penv
)

REM 3. 编译工具链 (ARM GCC ~1.2G)
echo [3/6] 复制 ARM GCC 编译器 (约1.2GB，请耐心等待)...
if exist "%USERPROFILE%\.platformio\packages\toolchain-gccarmnoneeabi" (
    robocopy "%USERPROFILE%\.platformio\packages\toolchain-gccarmnoneeabi" "%OUTPUT_DIR%\.platformio\packages\toolchain-gccarmnoneeabi" /E /NFL /NDL >nul
    echo    ✓ ARM GCC 编译器
) else (
    echo    ✗ 未找到
)

REM 4. Arduino STM32 框架
echo [4/6] 复制 Arduino STM32 框架...
if exist "%USERPROFILE%\.platformio\packages\framework-arduinoststm32" (
    robocopy "%USERPROFILE%\.platformio\packages\framework-arduinoststm32" "%OUTPUT_DIR%\.platformio\packages\framework-arduinoststm32" /E /NFL /NDL >nul
    echo    ✓ Arduino STM32 框架
)

REM 5. CMSIS 框架
if exist "%USERPROFILE%\.platformio\packages\framework-cmsis" (
    robocopy "%USERPROFILE%\.platformio\packages\framework-cmsis" "%OUTPUT_DIR%\.platformio\packages\framework-cmsis" /E /NFL /NDL >nul
    echo    ✓ CMSIS 框架
)
if exist "%USERPROFILE%\.platformio\packages\framework-cmsis-dsp" (
    robocopy "%USERPROFILE%\.platformio\packages\framework-cmsis-dsp" "%OUTPUT_DIR%\.platformio\packages\framework-cmsis-dsp" /E /NFL /NDL >nul
    echo    ✓ CMSIS-DSP 框架
)

REM 6. 其他必要文件
echo [5/6] 复制其他必要文件...

if exist "%USERPROFILE%\.platformio\packages\tool-openocd" (
    robocopy "%USERPROFILE%\.platformio\packages\tool-openocd" "%OUTPUT_DIR%\.platformio\packages\tool-openocd" /E /NFL /NDL >nul
    echo    ✓ OpenOCD (烧录工具)
)

if exist "%USERPROFILE%\.platformio\platforms" (
    robocopy "%USERPROFILE%\.platformio\platforms" "%OUTPUT_DIR%\.platformio\platforms" /E /NFL /NDL >nul
    echo    ✓ PlatformIO 平台定义
)

if exist "%USERPROFILE%\.platformio\.cache" (
    robocopy "%USERPROFILE%\.platformio\.cache" "%OUTPUT_DIR%\.platformio\.cache" /E /NFL /NDL >nul
    echo    ✓ PlatformIO 缓存
)

REM 复制 Python 和上位机 exe (已有)
echo [6/6] 复制上位机...
if exist "%~dp0dist\SiliconChainMonitor.exe" (
    copy "%~dp0dist\SiliconChainMonitor.exe" "%OUTPUT_DIR%\" >nul 2>&1
    echo    ✓ 上位机 EXE
)
if exist "%~dp0flash_stlink.bat" (
    copy "%~dp0flash_stlink.bat" "%OUTPUT_DIR%\" >nul 2>&1
    echo    ✓ 一键烧录脚本
)
if exist "%~dp0firmware\firmware.hex" (
    robocopy "%~dp0firmware" "%OUTPUT_DIR%\firmware" /E /NFL /NDL >nul
    echo    ✓ 预编译固件
)

REM 复制说明文档
if exist "%~dp0..\项目交接文档.md" (
    copy "%~dp0..\项目交接文档.md" "%OUTPUT_DIR%\" >nul 2>&1
    echo    ✓ 项目交接文档
)

REM 创建使用说明
(
echo # GL 便携式 SDK 使用说明
echo.
echo ## 新电脑上还原步骤
echo.
echo ### 前提
echo - 已安装 VSCode
echo - 已安装 PlatformIO IDE 扩展（VSCode 扩展商店搜索安装，需联网一次）
echo.
echo ### 步骤
echo 1. 将整个 GL_Portable_SDK 文件夹复制到新电脑
echo 2. 运行 setup_portable.bat（本文件夹内）
echo 3. 等待脚本完成
echo 4. 用 VSCode 打开 GL 文件夹，即可编译+烧录
echo.
echo ### 如果无法联网安装 PlatformIO 扩展
echo 已附带 platformio-ide.vsix 离线安装包
echo VSCode → 扩展 → ... → 从 VSIX 安装 → 选择此文件
echo.
echo ## 一键操作
echo - flash_stlink.bat: 烧录固件（需要ST-Link）
echo - SiliconChainMonitor.exe: 运行上位机（无需Python）
echo - 项目交接文档.md: 完整项目说明
) > "%OUTPUT_DIR%\README.md"
echo    ✓ 使用说明

echo.
echo  ╔══════════════════════════════════════════╗
echo  ║          打包完成！                      ║
echo  ╠══════════════════════════════════════════╣
echo  ║  输出目录: GL_Portable_SDK               ║
echo  ╚══════════════════════════════════════════╝
echo.
echo  将整个 GL_Portable_SDK 文件夹复制到U盘即可。
echo.
pause
exit /b 0

REM ============================================================
REM 安装模式：在新电脑上创建符号链接
REM ============================================================
:install
echo.
echo [→] 开始设置离线开发环境...
echo.

set "SDK_DIR=%~dp0"
set "TARGET=%USERPROFILE%\.platformio"

REM 检查是否有旧的 PlatformIO
if exist "%TARGET%" (
    echo [!] 检测到已有 .platformio 目录
    set /p overwrite="   是否覆盖? (y/n): "
    if /i not "!overwrite!"=="y" (
        echo    已取消
        goto :done
    )
    rmdir /s /q "%TARGET%"
)

REM 创建目录链接 (或者直接复制)
echo [1/3] 链接 PlatformIO 包...
mklink /J "%TARGET%" "%SDK_DIR%.platformio" >nul 2>&1
if errorlevel 1 (
    echo    链接失败，改用复制方式...
    robocopy "%SDK_DIR%.platformio" "%TARGET%" /E /NFL /NDL >nul
    echo    ✓ 已复制
) else (
    echo    ✓ 已链接（节省磁盘空间）
)

REM 验证
echo [2/3] 验证文件...
if exist "%TARGET%\penv\Scripts\platformio.exe" (
    echo    ✓ PlatformIO CLI 可用
) else (
    echo    ✗ PlatformIO CLI 未找到，请检查文件完整性
)

echo [3/3] 测试编译...
cd /d "%SDK_DIR%GL"
"%TARGET%\penv\Scripts\platformio.exe" run --silent 2>&1
if errorlevel 1 (
    echo    ✗ 编译测试失败，请检查
) else (
    echo    ✓ 编译测试通过！
)

:done
echo.
echo  设置完成！用 VSCode 打开 GL 文件夹即可开始开发。
echo.
pause
exit /b 0
