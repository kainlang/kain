@echo off
REM ============================================================================
REM Universal UE5 Plugin Build Script
REM Automatically detects plugin name, UE5 installation, and project location
REM ============================================================================

setlocal enabledelayedexpansion

echo.
echo ========================================
echo   UE5 Plugin Build Script
echo ========================================
echo.

REM --- Step 1: Detect Plugin Name from .uplugin file ---
set "PLUGIN_NAME="
for %%f in (*.uplugin) do (
    set "PLUGIN_NAME=%%~nf"
    goto :found_plugin
)

:found_plugin
if "%PLUGIN_NAME%"=="" (
    echo ERROR: No .uplugin file found in current directory!
    echo Please run this script from the plugin root directory.
    pause
    exit /b 1
)

echo [1/5] Detected Plugin: %PLUGIN_NAME%
echo.

REM --- Step 2: Detect UE5 Installation Path ---
set "UE5_PATH="

REM Check common installation paths
if exist "D:\Unreal\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set "UE5_PATH=D:\Unreal\UE_5.4"
    goto :found_ue5
)

if exist "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set "UE5_PATH=C:\Program Files\Epic Games\UE_5.4"
    goto :found_ue5
)

if exist "C:\Program Files\Unreal Engine\UE_5.4\Engine\Build\BatchFiles\Build.bat" (
    set "UE5_PATH=C:\Program Files\Unreal Engine\UE_5.4"
    goto :found_ue5
)

REM Check for UE_5.5, 5.3, etc.
for %%v in (5.5 5.4 5.3 5.2 5.1 5.0) do (
    if exist "D:\Unreal\UE_%%v\Engine\Build\BatchFiles\Build.bat" (
        set "UE5_PATH=D:\Unreal\UE_%%v"
        goto :found_ue5
    )
    if exist "C:\Program Files\Epic Games\UE_%%v\Engine\Build\BatchFiles\Build.bat" (
        set "UE5_PATH=C:\Program Files\Epic Games\UE_%%v"
        goto :found_ue5
    )
)

:found_ue5
if "%UE5_PATH%"=="" (
    echo ERROR: Could not find UE5 installation!
    echo Please edit this script and set UE5_PATH manually.
    pause
    exit /b 1
)

echo [2/5] Found UE5: %UE5_PATH%
echo.

REM --- Step 3: Detect Project File ---
set "PROJECT_FILE="
set "CURRENT_DIR=%CD%"

REM Go up directories to find .uproject file
pushd "%CURRENT_DIR%"
:find_project
if exist "*.uproject" (
    for %%p in (*.uproject) do (
        set "PROJECT_FILE=%%~fp"
        goto :found_project
    )
)
cd ..
if "%CD%"=="%CD:~0,3%" (
    echo ERROR: Could not find .uproject file!
    echo Please ensure this plugin is inside a UE5 project.
    popd
    pause
    exit /b 1
)
goto :find_project

:found_project
popd

echo [3/5] Found Project: %PROJECT_FILE%
echo.

REM --- Step 4: Clean Build (Optional) ---
echo [4/5] Cleaning old build artifacts...
if exist "Binaries" (
    rmdir /s /q "Binaries" 2>nul
    echo    - Deleted Binaries/
)
if exist "Intermediate" (
    rmdir /s /q "Intermediate" 2>nul
    echo    - Deleted Intermediate/
)
echo    - Clean complete
echo.

REM --- Step 5: Build Plugin ---
echo [5/5] Building plugin...
echo.
echo Command: "%UE5_PATH%\Engine\Build\BatchFiles\Build.bat" %PLUGIN_NAME%Editor Win64 Development "%PROJECT_FILE%" -WaitMutex
echo.

"%UE5_PATH%\Engine\Build\BatchFiles\Build.bat" %PLUGIN_NAME%Editor Win64 Development "%PROJECT_FILE%" -WaitMutex

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   BUILD SUCCESSFUL!
    echo ========================================
    echo.
    echo Plugin: %PLUGIN_NAME%
    echo Output: Binaries\Win64\UnrealEditor-%PLUGIN_NAME%.dll
    echo.
) else (
    echo.
    echo ========================================
    echo   BUILD FAILED!
    echo ========================================
    echo.
    echo Check the error messages above.
    echo.
)

pause
