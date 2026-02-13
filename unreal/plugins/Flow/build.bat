@echo off
echo ==========================================
echo      KAIN-PRO UE5 BUILD SYSTEM
echo ==========================================
echo.
echo Building plugin from KAIN.toml...
echo.

kain-pro build --ue5

echo.
echo ==========================================
if %ERRORLEVEL% EQU 0 (
    echo      BUILD SUCCESSFUL!
) else (
    echo      BUILD FAILED!
)
echo ==========================================
echo.
pause