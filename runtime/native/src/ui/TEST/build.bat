@echo off
setlocal enabledelayedexpansion

REM ==========================================================================
REM  Kain UI System — Standalone Build Script
REM ==========================================================================
REM  Compiles the UI system files + input_system into a standalone Win32 exe.
REM  Uses clang (from LLVM) or MSVC if clang is unavailable.
REM ==========================================================================

set UI_SRC=..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c
set INPUT_SRC=..\..\core\input_system.c
set TEST_SRC=main.c stubs.c
set INC=-I ..\..\..\include -I .. -I ..\..\core
set LIBS=-luser32 -lgdi32 -lopengl32
set OUT=KainUIDemo.exe

echo.
echo === Kain UI System — Standalone Build ===
echo.
echo Source files:
echo   %TEST_SRC%
echo   %UI_SRC%
echo   %INPUT_SRC%
echo.

REM Try clang first
where clang >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [clang] Compiling...
    clang -std=c11 -Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-function -g -O2 ^
        %TEST_SRC% %UI_SRC% %INPUT_SRC% %INC% %LIBS% -o %OUT%
    if !ERRORLEVEL! EQU 0 (
        echo [clang] SUCCESS — %OUT% created
        goto :done
    ) else (
        echo [clang] FAILED — trying MSVC fallback...
    )
)

REM MSVC fallback
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [msvc] Compiling...
    cl /nologo /O2 /W3 /utf-8 ^
        %TEST_SRC% %UI_SRC% %INPUT_SRC% ^
        /I ..\..\..\include /I .. /I ..\..\core ^
        /link user32.lib gdi32.lib opengl32.lib /OUT:%OUT%
    if !ERRORLEVEL! EQU 0 (
        echo [msvc] SUCCESS — %OUT% created
        goto :done
    ) else (
        echo [msvc] FAILED
        exit /b 1
    )
)

echo ERROR: Neither clang nor MSVC cl found in PATH.
echo Install LLVM (scoop install llvm) or open "Developer Command Prompt for VS".
exit /b 1

:done
echo.
echo === Build complete ===
echo Run: %OUT%
echo.
