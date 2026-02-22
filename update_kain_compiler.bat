@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM UPDATE KAIN COMPILER - Installs latest kain.exe to cargo bin
REM ==============================================================================

echo ============================================================================
echo Updating KAIN compiler...
echo ============================================================================
echo.

cargo install --path crates/cli --force

if !ERRORLEVEL! neq 0 (
    echo.
    echo ============================================================================
    echo [FAILED] Compiler update failed!
    echo ============================================================================
    exit /b 1
)

echo.
echo ============================================================================
echo [SUCCESS] KAIN compiler updated!
echo ============================================================================
echo.
echo Installed to: C:\Users\Admin\.cargo\bin\kain.exe
echo.

exit /b 0
