@echo off

REM 获取bat文件所在目录
set "PROJECT_DIR=%~dp0"

REM 进入项目目录
cd /d "%PROJECT_DIR%"

REM 创建 build 文件夹（如果不存在）
if not exist build (
    mkdir build
)

REM 进入 build 文件夹
cd build

REM 执行 cmake 命令
cmake -G "Visual Studio 18 2026" -A x64 ..

pause
