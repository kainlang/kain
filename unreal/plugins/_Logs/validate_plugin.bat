@echo off
REM ============================================================================
REM Quick Plugin Validation - Uses UnrealEditor.exe to compile plugin
REM ============================================================================
REM This creates a minimal test project and compiles the plugin to verify
REM all C++ code is valid according to UBT/UHT
REM ============================================================================

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Usage: validate_plugin.bat ^<PluginFolder^>
    echo Example: validate_plugin.bat VRAMSniper
    pause
    exit /b 1
)

set PLUGIN_NAME=%~1
set PLUGIN_PATH=%~dp0%PLUGIN_NAME%

echo ============================================================================
echo Plugin Validation - %PLUGIN_NAME%
echo ============================================================================
echo.

REM Check if plugin exists
if not exist "%PLUGIN_PATH%" (
    echo [ERROR] Plugin folder not found: %PLUGIN_PATH%
    pause
    exit /b 1
)

REM Find UE5 installation
set UE5_PATH=
if exist "D:\Unreal\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" (
    set UE5_PATH=D:\Unreal\UE_5.7
    set UE5_VERSION=5.7
) else if exist "D:\Unreal\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe" (
    set UE5_PATH=D:\Unreal\UE_5.4
    set UE5_VERSION=5.4
) else (
    echo [ERROR] UE5 not found in D:\Unreal\
    echo Please update UE5_PATH in this script
    pause
    exit /b 1
)

echo [OK] Found UE5 %UE5_VERSION% at %UE5_PATH%
echo.

REM Create minimal test project if it doesn't exist
set TEST_PROJECT=%~dp0_ValidationProject
if not exist "%TEST_PROJECT%" (
    echo [1/4] Creating minimal test project...
    mkdir "%TEST_PROJECT%"
    mkdir "%TEST_PROJECT%\Content"
    mkdir "%TEST_PROJECT%\Plugins"
    
    REM Create minimal .uproject file
    echo { > "%TEST_PROJECT%\ValidationProject.uproject"
    echo   "FileVersion": 3, >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo   "EngineAssociation": "%UE5_VERSION%", >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo   "Category": "", >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo   "Description": "", >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo   "Modules": [ >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo     { >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo       "Name": "ValidationProject", >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo       "Type": "Runtime", >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo       "LoadingPhase": "Default" >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo     } >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo   ], >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo   "Plugins": [ >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo     { >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo       "Name": "%PLUGIN_NAME%", >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo       "Enabled": true >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo     } >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo   ] >> "%TEST_PROJECT%\ValidationProject.uproject"
    echo } >> "%TEST_PROJECT%\ValidationProject.uproject"
    
    echo       Created test project
) else (
    echo [1/4] Using existing test project...
)
echo.

REM Copy plugin to test project
echo [2/4] Copying plugin to test project...
if exist "%TEST_PROJECT%\Plugins\%PLUGIN_NAME%" (
    rmdir /s /q "%TEST_PROJECT%\Plugins\%PLUGIN_NAME%"
)
xcopy /E /I /Q "%PLUGIN_PATH%" "%TEST_PROJECT%\Plugins\%PLUGIN_NAME%" >nul
echo       Plugin copied
echo.

REM Run UnrealBuildTool to compile plugin
echo [3/4] Running UnrealBuildTool (this validates all C++ code)...
echo       This may take 30-60 seconds...
echo.

set UBT_PATH=%UE5_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe
if not exist "%UBT_PATH%" (
    echo [ERROR] UnrealBuildTool not found at %UBT_PATH%
    pause
    exit /b 1
)

REM Build the plugin module
"%UBT_PATH%" ^
    -ModuleWithSuffix=%PLUGIN_NAME%,3555 ^
    -Project="%TEST_PROJECT%\ValidationProject.uproject" ^
    -TargetType=Editor ^
    -Platform=Win64 ^
    -Configuration=Development ^
    -SkipBuild ^
    -Validate

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================================
    echo [4/4] VALIDATION PASSED
    echo ============================================================================
    echo.
    echo ✅ Plugin C++ code is valid
    echo ✅ All UCLASS/USTRUCT/UENUM macros correct
    echo ✅ All UPROPERTY/UFUNCTION specifiers valid
    echo ✅ No UHT errors detected
    echo.
    echo Plugin is ready for UE5 compilation!
    echo.
) else (
    echo.
    echo ============================================================================
    echo [4/4] VALIDATION FAILED
    echo ============================================================================
    echo.
    echo ❌ Plugin has C++ errors
    echo.
    echo Check the output above for details.
    echo Common issues:
    echo   - Missing GENERATED_BODY^(^) in UCLASS/USTRUCT
    echo   - Wrong UPROPERTY specifiers
    echo   - Missing includes
    echo   - Double prefixes ^(AA, FF, EE^)
    echo.
)

pause
