@echo off
REM 设置构建目录
set BUILD_DIR=build
set EXE_DIR=bin
set EXE_NAME=app.exe

echo.
echo ========== 执行[cmake .. -G Ninja]命令 ==========
echo.

REM 如果构建目录不存在，创建它
if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)

REM 进入构建目录
cd %BUILD_DIR%

REM 运行 CMake 配置
cmake .. -G Ninja

echo.
echo ========== 执行[ninja]命令 ==========
echo.

REM 执行构建
ninja

REM 返回上一级目录
cd ..

echo.
echo ========== 执行[运行 .exe 文件]命令 ==========
echo.

REM 执行 exe 文件
cd %EXE_DIR%
%EXE_NAME%

echo.