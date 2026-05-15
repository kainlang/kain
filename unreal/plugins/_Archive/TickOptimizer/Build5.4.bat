@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM SMART BUILD - Regenerates KAIN source, then builds plugin
REM ==============================================================================

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PLUGIN_FILE="
set "PLUGIN_NAME="

for %%F in ("%SCRIPT_DIR%\*.uplugin") do (
    set "PLUGIN_FILE=%%F"
    set "PLUGIN_NAME=%%~nF"
    goto :FoundPlugin
)

:FoundPlugin
if "%PLUGIN_FILE%"=="" (
    echo [ERROR] No .uplugin file found!
    exit /b 1
)

set "UE_5_4=D:\Unreal\UE_5.4"
set "OUTPUT_ROOT=%SCRIPT_DIR%\_Builds"

echo ============================================================================
echo [STEP 1/2] Regenerating C++ from KAIN source...
echo ============================================================================
kain build --ue5
if errorlevel 1 (
    echo [ERROR] KAIN compilation failed
    exit /b 1
)

echo.
echo ============================================================================
echo [STEP 2/2] Building plugin with UE5...
echo ============================================================================

if not exist "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo [ERROR] Could not find UE 5.4 at: %UE_5_4%
    exit /b 1
)

REM BuildPlugin is smart - it does incremental builds automatically
call "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.4" -Rocket -TargetPlatforms=Win64

if errorlevel 1 (
    echo [FAILED] Build failed - check errors above
    exit /b 1
)

echo.
echo ============================================================================
echo [SUCCESS] Build complete!
echo ============================================================================
echo Plugin: %PLUGIN_NAME%
echo Build: %OUTPUT_ROOT%\%PLUGIN_NAME%_5.4
