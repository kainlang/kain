@echo off
setlocal enabledelayedexpansion

REM ==========================================================================
REM  Kain Native UI — Widget Library Build Script
REM ==========================================================================
REM  Usage: build.bat            — build test_widgets.exe
REM         build.bat clean      — clean binaries
REM
REM  Text rendering via stb_truetype glyph rasterization (no GDI).
REM  21 Z3 proof packs verify the font rasterizer at extras/_stb-truetype/.
REM ==========================================================================

set WIDGET_SRC=test_widgets.c ui_widget.c stubs.c ttf_font_impl.c
set UI_SRC=..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c
set INPUT_SRC=..\..\core\input_system.c
set RENDER_CMD_SRC=..\render_command.c
ender_command.c
set ALL_SRC=%WIDGET_SRC% %UI_SRC% %INPUT_SRC% %RENDER_CMD_SRC%
set INC=-I ..\..\..\include -I .. -I ..\..\core -I ..\..\..\extras\_stb-truetype
set LIBS=-luser32 -lgdi32 -lopengl32
set OUT=test_widgets.exe

REM Auto-detect MSVC LIBPATH
set LIBPATH=
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
    for /d %%d in ("C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*") do set MSVC_VER=%%~nxd
    if defined MSVC_VER (
        set LIBPATH=-L "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\!MSVC_VER!\lib\x64"
    )
)
if exist "C:\Program Files (x86)\Windows Kits\10\Lib" (
    for /d %%d in ("C:\Program Files (x86)\Windows Kits\10\Lib\*") do set SDK_VER=%%~nxd
    if defined SDK_VER (
        set LIBPATH=!LIBPATH! -L "C:\Program Files (x86)\Windows Kits\10\Lib\!SDK_VER!\ucrt\x64"
        set LIBPATH=!LIBPATH! -L "C:\Program Files (x86)\Windows Kits\10\Lib\!SDK_VER!\um\x64"
    )
)

echo.
echo === Kain Native UI — Widget Library Build ===
echo.

REM Try clang first
where clang >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [clang] Compiling...
    clang -std=c11 -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function ^
        -D_CRT_SECURE_NO_WARNINGS ^
        %ALL_SRC% %INC% %LIBS% %LIBPATH% -o %OUT%
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
    cl /nologo /O2 /W3 /utf-8 %ALL_SRC% /I ..\..\..\include /I .. /I ..\..\core /link user32.lib gdi32.lib opengl32.lib /OUT:%OUT%
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
