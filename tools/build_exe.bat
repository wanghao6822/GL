@echo off
chcp 65001 >nul
echo ============================================
echo   硅链控制板 - 上位机 EXE 打包脚本
echo ============================================
echo.

REM 检查 PyInstaller 是否安装
py -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
    echo [1/2] 安装 PyInstaller...
    py -m pip install pyinstaller
    echo.
)

echo [2/2] 打包生成 EXE...
py -m PyInstaller --onefile --windowed --name "SiliconChainMonitor" --clean --noconfirm "硅链控制板_上位机.py"

if exist "dist\SiliconChainMonitor.exe" (
    echo.
    echo ============================================
    echo   ✓ 打包完成!
    echo   输出路径: tools\dist\SiliconChainMonitor.exe
    echo   文件大小:
    for %%A in ("dist\SiliconChainMonitor.exe") do echo   %%~zA 字节
    echo ============================================
) else (
    echo.
    echo   ✗ 打包失败，请检查错误信息
)

REM 清理临时文件
if exist "build" rmdir /s /q "build"
echo.
echo 提示: 将 dist\SiliconChainMonitor.exe 拷贝到U盘即可独立运行
pause
