@echo off
REM ============================================================================
REM  build_examples_v2.bat — Build all examples_v2 test demos
REM
REM  Run from: runtime/native/src/ui_v2/
REM  Usage:    build_examples_v2.bat
REM ============================================================================
setlocal

set "INCLUDES=-I ../../include -I ."
set "CFLAGS=-std=c11 -O0 -g"
set "CORE=tree.c box_math.c damage.c draw_pixels.c arena.c hash_table.c color.c attr_table.c kaintana_runtime_stubs.c"
set "RTCORE=../../src/core/arena.c ../../src/core/version.c ../../src/core/component_surface.c ../../src/core/handle.c ../../src/core/input_system.c"

echo.
echo === Building DPI Scaling Verification ===
gcc %CFLAGS% %INCLUDES% %CORE% %RTCORE% examples_v2/dpi_scaling_verification.c -o examples_v2/dpi_scaling_verification.exe
if %ERRORLEVEL% NEQ 0 goto :fail

echo === Building Input & State Tracker ===
gcc %CFLAGS% %INCLUDES% %CORE% %RTCORE% examples_v2/input_state_tracker.c -o examples_v2/input_state_tracker.exe
if %ERRORLEVEL% NEQ 0 goto :fail

echo === Building Interactive Terminal Panel ===
gcc %CFLAGS% %INCLUDES% %CORE% %RTCORE% examples_v2/interactive_terminal_panel.c -o examples_v2/interactive_terminal_panel.exe
if %ERRORLEVEL% NEQ 0 goto :fail

echo.
echo === ALL BUILDS OK ===
echo.
echo Running tests...
echo.

echo ========================================
echo   TEST 1: DPI SCALING
echo ========================================
examples_v2\dpi_scaling_verification.exe
if %ERRORLEVEL% NEQ 0 echo [WARN] DPI test exited with code %ERRORLEVEL%

echo.
echo ========================================
echo   TEST 2: INPUT & STATE
echo ========================================
examples_v2\input_state_tracker.exe
if %ERRORLEVEL% NEQ 0 echo [WARN] Input test exited with code %ERRORLEVEL%

echo.
echo ========================================
echo   TEST 3: INTERACTIVE PANEL
echo ========================================
echo  (press Enter to advance through frames)
echo.
echo test | examples_v2\interactive_terminal_panel.exe

echo.
echo === ALL DONE ===
goto :eof

:fail
echo [FAIL] Build failed with error %ERRORLEVEL%
exit /b 1
