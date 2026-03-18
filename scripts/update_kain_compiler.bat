@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM UPDATE KAIN COMPILER - Installs latest kain.exe to cargo bin
REM ==============================================================================

echo ============================================================================
echo Updating KAIN compiler...
echo ============================================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0sync-kain-source-of-truth.ps1" -PersistUserEnv

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
echo Installed and refreshed via sync-kain-source-of-truth.ps1
echo.

exit /b 0
