@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM [SETUP] AUTO-DETECT PLUGIN IN CURRENT FOLDER
REM ==============================================================================

REM Get the directory where this script is running
set "SCRIPT_DIR=%~dp0"
REM Remove trailing backslash for consistency
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

REM Find the first .uplugin file in this directory
set "PLUGIN_FILE="
set "PLUGIN_NAME="

for %%F in ("%SCRIPT_DIR%\*.uplugin") do (
    set "PLUGIN_FILE=%%F"
    set "PLUGIN_NAME=%%~nF"
    goto :FoundPlugin
)

:FoundPlugin
if "%PLUGIN_FILE%"=="" (
    echo [ERROR] No .uplugin file found in %SCRIPT_DIR%
    echo Please place this script inside your plugin's root folder.
    pause
    exit /b
)

echo [INFO] Detected Plugin: %PLUGIN_NAME%
echo [INFO] Path: %PLUGIN_FILE%

REM Define Output Directory inside the plugin folder
set "OUTPUT_ROOT=%SCRIPT_DIR%\_Builds"
if not exist "%OUTPUT_ROOT%" mkdir "%OUTPUT_ROOT%"

REM ==============================================================================
REM [ENGINE PATHS] YOUR SPECIFIC INSTALLS
REM ==============================================================================

set "UE_5_4=D:\Unreal\UE_5.4"
set "UE_5_5=M:\UnrealEngine\UE\UE_5.5"
set "UE_5_6=M:\UnrealEngine\UE\UE_5.6"
set "UE_5_7=D:\Unreal\UE_5.7"

REM ==============================================================================
REM [BUILD LOOP]
REM ==============================================================================

echo.
echo [START] Building %PLUGIN_NAME% for all detected engines...

REM --- BUILD 5.4 ---
if exist "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo.
    echo -----------------------------------------------------------------------
    echo [BUILDING] %PLUGIN_NAME% for Unreal Engine 5.4...
    echo -----------------------------------------------------------------------
    call "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.4" -Rocket -TargetPlatforms=Win64
) else (
    echo [SKIP] UE 5.4 not found.
)

REM --- BUILD 5.5 ---
if exist "%UE_5_5%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo.
    echo -----------------------------------------------------------------------
    echo [BUILDING] %PLUGIN_NAME% for Unreal Engine 5.5...
    echo -----------------------------------------------------------------------
    call "%UE_5_5%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.5" -Rocket -TargetPlatforms=Win64
) else (
    echo [SKIP] UE 5.5 not found.
)

REM --- BUILD 5.6 ---
if exist "%UE_5_6%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo.
    echo -----------------------------------------------------------------------
    echo [BUILDING] %PLUGIN_NAME% for Unreal Engine 5.6...
    echo -----------------------------------------------------------------------
    call "%UE_5_6%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.6" -Rocket -TargetPlatforms=Win64
) else (
    echo [SKIP] UE 5.6 not found.
)

REM --- BUILD 5.7 ---
if exist "%UE_5_7%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo.
    echo -----------------------------------------------------------------------
    echo [BUILDING] %PLUGIN_NAME% for Unreal Engine 5.7...
    echo -----------------------------------------------------------------------
    call "%UE_5_7%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.7" -Rocket -TargetPlatforms=Win64
) else (
    echo [SKIP] UE 5.7 not found.
)

echo.
echo [SUCCESS] Builds completed.
echo Location: %OUTPUT_ROOT%
pause