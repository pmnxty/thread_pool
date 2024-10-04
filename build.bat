@echo off
REM ���ù���Ŀ¼
set BUILD_DIR=build
set EXE_DIR=bin
set EXE_NAME=app.exe

echo.
echo ========== ִ��[cmake .. -G Ninja]���� ==========
echo.

REM �������Ŀ¼�����ڣ�������
if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)

REM ���빹��Ŀ¼
cd %BUILD_DIR%

REM ���� CMake ����
cmake .. -G Ninja

echo.
echo ========== ִ��[ninja]���� ==========
echo.

REM ִ�й���
ninja

REM ������һ��Ŀ¼
cd ..

echo.
echo ========== ִ��[���� .exe �ļ�]���� ==========
echo.

REM ִ�� exe �ļ�
cd %EXE_DIR%
%EXE_NAME%

echo.