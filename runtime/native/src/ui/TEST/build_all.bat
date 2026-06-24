@echo off
setlocal enabledelayedexpansion

REM ==========================================================================
REM  Kain UI System — Build All Test Paths
REM ==========================================================================
REM  Builds all three test approaches.
REM  Path A: Full Kain pipeline
REM  Path B: Kain window + direct framebuffer write
REM  Path C: Pure GDI control test
REM ==========================================================================

set UI_SRC=..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c
set INPUT_SRC=..\..\core\input_system.c
set INC=-I ..\..\..\include -I .. -I ..\..\core
set LIBS=-luser32 -lgdi32 -lopengl32

echo === Kain UI System — Build All Tests ===
echo.

REM Find clang
set CC=clang
where clang >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo clang not found, trying cl...
    set CC=cl
)

if "%CC%"=="clang" (
    echo === Compiling Path A: Full Kain Pipeline ===
    clang -std=c11 -g -O0 path_a_full_pipeline.c stubs.c ^
        %UI_SRC% %INPUT_SRC% %INC% %LIBS% -o path_a_full_pipeline.exe
    if !ERRORLEVEL! EQU 0 ( echo [OK] path_a_full_pipeline.exe created ) else ( echo [FAIL] )
    echo.

    echo === Compiling Path B: Direct Framebuffer Write ===
    clang -std=c11 -g -O0 path_b_direct_fb.c stubs.c ^
        %UI_SRC% %INPUT_SRC% %INC% %LIBS% -o path_b_direct_fb.exe
    if !ERRORLEVEL! EQU 0 ( echo [OK] path_b_direct_fb.exe created ) else ( echo [FAIL] )
    echo.

    echo === Compiling Path C: Pure GDI ===
    clang -std=c11 -g -O0 path_c_pure_gdi.c ^
        -luser32 -lgdi32 -o path_c_pure_gdi.exe
    if !ERRORLEVEL! EQU 0 ( echo [OK] path_c_pure_gdi.exe created ) else ( echo [FAIL] )
) else (
    echo MSVC build not implemented for three-file build.
    echo Use individual clang commands instead.
)

echo.
echo === Build complete ===
echo.
echo To run:
echo   path_c_pure_gdi.exe    (control test, no Kain)
echo   path_b_direct_fb.exe   (Kain window + direct framebuffer paint)
echo   path_a_full_pipeline.exe (full Kain pipeline)
