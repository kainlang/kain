@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM SMART BUILD - Regenerates KAIN source, then builds plugin with auto-logging
REM ==============================================================================

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PLUGIN_FILE="
set "PLUGIN_NAME="
set "FACTORY_ROOT=M:\Code\Factory"
set "COMBINED_LOG=%FACTORY_ROOT%\COMBINEDLOG.md"
set "TEMP_LOG=%TEMP%\kain_build_%RANDOM%.log"
set "FIRST_BUILD=0"

REM Try to find existing .uplugin file
for %%F in ("%SCRIPT_DIR%\*.uplugin") do (
    set "PLUGIN_FILE=%%F"
    set "PLUGIN_NAME=%%~nF"
    goto :FoundPlugin
)

REM If not found in root, scan nested folders (one level deep)
for /d %%D in ("%SCRIPT_DIR%\*") do (
    for %%F in ("%%D\*.uplugin") do (
        set "PLUGIN_FILE=%%F"
        set "PLUGIN_NAME=%%~nF"
        goto :FoundPlugin
    )
)

REM If no .uplugin found, this is a first build - extract name from KAIN.toml
if "%PLUGIN_FILE%"=="" (
    echo [INFO] No .uplugin found - this appears to be a first build
    set "FIRST_BUILD=1"
    
    if not exist "%SCRIPT_DIR%\KAIN.toml" (
        echo [ERROR] No KAIN.toml found in %SCRIPT_DIR%
        exit /b 1
    )
    
    REM Extract plugin name from KAIN.toml
    for /f "tokens=2 delims==" %%N in ('findstr /c:"plugin_name" "%SCRIPT_DIR%\KAIN.toml"') do (
        set "PLUGIN_NAME=%%N"
        set "PLUGIN_NAME=!PLUGIN_NAME: =!"
        set "PLUGIN_NAME=!PLUGIN_NAME:"=!"
    )
    
    if "!PLUGIN_NAME!"=="" (
        echo [ERROR] Could not extract plugin_name from KAIN.toml
        exit /b 1
    )
    
    echo [INFO] Plugin name from KAIN.toml: !PLUGIN_NAME!
)

:FoundPlugin

set "UE_5_4=D:\Unreal\UE_5.4"
set "OUTPUT_ROOT=%SCRIPT_DIR%\_Builds"
set "KAIN_REPO=M:\Code\Kain"

echo ============================================================================
echo [STEP 1/3] Updating KAIN compiler...
echo ============================================================================
if exist "%KAIN_REPO%\crates\cli" (
    pushd "%KAIN_REPO%"
    cargo install --path crates/cli --force
    set "INSTALL_RESULT=!ERRORLEVEL!"
    popd
    
    if !INSTALL_RESULT! neq 0 (
        echo [WARN] KAIN compiler update failed, using existing version
    ) else (
        echo [SUCCESS] KAIN compiler updated
    )
) else (
    echo [WARN] KAIN repo not found at %KAIN_REPO%, using existing compiler
)

echo.
echo ============================================================================
echo [STEP 2/3] Regenerating C++ from KAIN source...
echo ============================================================================
kain build --ue5 > "%TEMP_LOG%" 2>&1
set "KAIN_RESULT=%ERRORLEVEL%"

if !KAIN_RESULT! neq 0 (
    echo [ERROR] KAIN compilation failed
    type "%TEMP_LOG%"
    call :LogErrors "KAIN COMPILATION" "%TEMP_LOG%"
    del "%TEMP_LOG%" 2>nul
    exit /b 1
)

echo [SUCCESS] KAIN compilation complete
del "%TEMP_LOG%" 2>nul

echo.
echo ============================================================================
echo [STEP 3/3] Building plugin with UE5...
echo ============================================================================

REM After KAIN build, find the generated .uplugin file
if !FIRST_BUILD! equ 1 (
    echo [INFO] Locating generated .uplugin file...
    
    for %%F in ("%SCRIPT_DIR%\*.uplugin") do (
        set "PLUGIN_FILE=%%F"
        goto :FoundGeneratedPlugin
    )
    
    for /d %%D in ("%SCRIPT_DIR%\*") do (
        for %%F in ("%%D\*.uplugin") do (
            set "PLUGIN_FILE=%%F"
            goto :FoundGeneratedPlugin
        )
    )
    
    echo [ERROR] KAIN build succeeded but no .uplugin file was generated
    exit /b 1
)

:FoundGeneratedPlugin
echo [INFO] Using plugin file: %PLUGIN_FILE%

if not exist "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo [ERROR] Could not find UE 5.4 at: %UE_5_4%
    exit /b 1
)

REM BuildPlugin is smart - it does incremental builds automatically
call "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.4" -Rocket -TargetPlatforms=Win64 > "%TEMP_LOG%" 2>&1
set "BUILD_RESULT=%ERRORLEVEL%"

if !BUILD_RESULT! neq 0 (
    echo [FAILED] Build failed - check errors above
    type "%TEMP_LOG%"
    call :LogErrors "UE5 BUILD" "%TEMP_LOG%"
    del "%TEMP_LOG%" 2>nul
    exit /b 1
)

echo.
echo ============================================================================
echo [SUCCESS] Build complete!
echo ============================================================================
echo Plugin: %PLUGIN_NAME%
echo Build: %OUTPUT_ROOT%\%PLUGIN_NAME%_5.4

del "%TEMP_LOG%" 2>nul
exit /b 0

REM ==============================================================================
REM Function: LogErrors - Append errors to COMBINEDLOG.md with smart truncation
REM Parameters: %1 = Error type (KAIN/UE5), %2 = Log file path
REM ==============================================================================
:LogErrors
setlocal enabledelayedexpansion
set "ERROR_TYPE=%~1"
set "LOG_FILE=%~2"
set "MAX_LINES=50"
set "LINE_COUNT=0"
set "ERROR_COUNT=0"

REM Create combined log if it doesn't exist
if not exist "%COMBINED_LOG%" (
    echo # Combined Build Errors Log > "%COMBINED_LOG%"
    echo. >> "%COMBINED_LOG%"
)

REM Append header
echo. >> "%COMBINED_LOG%"
echo ------------- >> "%COMBINED_LOG%"
echo %PLUGIN_NAME% - %ERROR_TYPE% >> "%COMBINED_LOG%"
echo ------------- >> "%COMBINED_LOG%"

REM Extract and append errors with smart truncation
for /f "usebackq delims=" %%L in ("%LOG_FILE%") do (
    set "LINE=%%L"
    
    REM Check if line contains error indicators (case insensitive)
    echo.!LINE! | findstr /i /c:"error" /c:"Error:" /c:"ERROR" /c:"failed" /c:"FAILED" >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        set /a ERROR_COUNT+=1
        
        REM Only log first MAX_LINES errors
        if !LINE_COUNT! lss %MAX_LINES% (
            echo !LINE! >> "%COMBINED_LOG%"
            set /a LINE_COUNT+=1
        )
    )
)

REM Add truncation notice if needed
if !ERROR_COUNT! gtr %MAX_LINES% (
    set /a REMAINING=!ERROR_COUNT! - %MAX_LINES%
    echo. >> "%COMBINED_LOG%"
    echo [TRUNCATED: !REMAINING! more errors not shown] >> "%COMBINED_LOG%"
)

REM Add summary
echo. >> "%COMBINED_LOG%"
echo Total errors found: !ERROR_COUNT! >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"

echo [LOG] Errors appended to: %COMBINED_LOG%
endlocal
exit /b 0
