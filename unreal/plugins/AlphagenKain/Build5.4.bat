@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM [SETUP] AUTO-DETECT PLUGIN IN CURRENT FOLDER
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
    pause
    exit /b
)

echo [TARGET] Plugin: %PLUGIN_NAME%
echo [ENGINE] Unreal Engine 5.4 ONLY

REM Define Output Directory
set "OUTPUT_ROOT=%SCRIPT_DIR%\_Builds"

REM ==============================================================================
REM [PATH] YOUR 5.4 INSTALL LOCATION
REM ==============================================================================

set "UE_5_4=D:\Unreal\UE_5.4"

REM ==============================================================================
REM [EXECUTION]
REM ==============================================================================

if exist "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo.
    echo -----------------------------------------------------------------------
    echo [BUILDING] %PLUGIN_NAME% for UE 5.4...
    echo -----------------------------------------------------------------------
    
    REM Run the build
    call "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.4" -Rocket -TargetPlatforms=Win64
    
    echo.
    echo [STATUS] Build process finished.
    echo [CHECK] Look inside: %OUTPUT_ROOT%\%PLUGIN_NAME%_5.4
) else (
    echo [ERROR] Could not find UE 5.4 at: %UE_5_4%
    echo Please check the path in the script.
)

pause