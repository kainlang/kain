@echo off
setlocal enabledelayedexpansion

REM ==========================================================================
REM  Kain UI System — TEST_V2 Build Script
REM ==========================================================================
REM  Builds all V2 test programs.
REM
REM  Usage: build.bat            — build all
REM         build.bat calculator — build single test
REM         build.bat clean      — clean binaries
REM ==========================================================================

set UI_SRC=..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c
set UI_HOT_RELOAD=..\ui_hot_reload.c ..\ui_compiled_bundle.c ..\ui_runtime.c
set INPUT_SRC=..\..\core\input_system.c
set STUBS=..\TEST\stubs.c
set INC=-I ..\..\..\include -I .. -I ..\..\core
set LIBS=-luser32 -lgdi32 -lopengl32
set MSVC_LIBDIR="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64"
set SDK_UCRT="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
set SDK_UM="C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
set LIBPATH=-L %MSVC_LIBDIR% -L %SDK_UCRT% -L %SDK_UM%
set CC=clang

echo.
echo === Kain UI System — TEST_V2 ===
echo.

where %CC% >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: clang not found in PATH.
    exit /b 1
)

set TARGET=all
if not "%1"=="" set TARGET=%1

if /I "%TARGET%"=="clean" goto :clean

echo [1/5] calculator.exe ...
%CC% -std=c11 -g -O0 calculator.c %STUBS% %UI_SRC% %INPUT_SRC% %INC% %LIBS% %LIBPATH% -o calculator.exe
if %ERRORLEVEL% EQU 0 ( echo   [OK] ) else ( echo   [FAIL] & set ERR=1 )

echo [2/5] anim_demo.exe ...
%CC% -std=c11 -g -O0 anim_demo.c %STUBS% %UI_SRC% %INPUT_SRC% %INC% %LIBS% %LIBPATH% -o anim_demo.exe
if %ERRORLEVEL% EQU 0 ( echo   [OK] ) else ( echo   [FAIL] & set ERR=1 )

echo [3/5] keypad.exe ...
%CC% -std=c11 -g -O0 keypad.c %STUBS% %UI_SRC% %INPUT_SRC% %INC% %LIBS% %LIBPATH% -o keypad.exe
if %ERRORLEVEL% EQU 0 ( echo   [OK] ) else ( echo   [FAIL] & set ERR=1 )

echo [4/5] full_demo.exe ...
%CC% -std=c11 -g -O0 full_demo.c %STUBS% %UI_SRC% %INPUT_SRC% %INC% %LIBS% %LIBPATH% -o full_demo.exe
if %ERRORLEVEL% EQU 0 ( echo   [OK] ) else ( echo   [FAIL] & set ERR=1 )

set HOT_SRC=%UI_SRC% %UI_HOT_RELOAD%
echo [5/5] hot_reload_test.exe ...
%CC% -std=c11 -g -O0 hot_reload_test.c %STUBS% %UI_SRC% %UI_HOT_RELOAD% %INPUT_SRC% %INC% %LIBS% %LIBPATH% -o hot_reload_test.exe
if %ERRORLEVEL% EQU 0 ( echo   [OK] ) else ( echo   [FAIL] & set ERR=1 )

goto :done

:clean
echo Cleaning...
del /q *.exe *.pdb *.obj *.o 2>nul
echo   Done.
goto :done

:done
if defined ERR (
    echo.
    echo Some builds failed.
    exit /b 1
) else (
    echo.
    echo === All builds successful ===
    echo.
    echo Run tests:
    echo   calculator.exe      - 4-function calculator
    echo   anim_demo.exe       - Animated particle system
    echo   keypad.exe          - PIN entry keypad
    echo   full_demo.exe       - Full UI dashboard
    echo   hot_reload_test.exe - Hot reload channel test
)
