@echo off
REM Wrapper to run rebuild_combinedlog_kain.ps1
REM This validates KAIN compilation only (no UE5 builds)

echo ============================================================================
echo Running KAIN-only compilation check...
echo ============================================================================
echo.

PowerShell.exe -ExecutionPolicy Bypass -File "%~dp0rebuild_combinedlog_kain.ps1"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ============================================================================
    echo KAIN compilation check completed with failures
    echo ============================================================================
    echo Check COMBINEDLOG_KAIN.md for details
    exit /b 1
)

echo.
echo ============================================================================
echo KAIN compilation check completed successfully
echo ============================================================================
echo Check COMBINEDLOG_KAIN.md for details
exit /b 0
