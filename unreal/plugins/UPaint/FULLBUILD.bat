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
set "LOCAL_LOG=%SCRIPT_DIR%\BUILD_LOG.md"
set "TEMP_LOG=%TEMP%\kain_build_%RANDOM%.log"
set "WARNINGS_AS_ERRORS=0"

REM Always extract plugin name from KAIN.toml (source of truth)
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

set "UE_5_4=D:\Unreal\UE_5.4"
set "OUTPUT_ROOT=%SCRIPT_DIR%\_Builds"
set "KAIN_REPO=M:\Code\Kain"

echo.
echo ============================================================================
echo [STEP 1/3] Verifying KAIN compiler...
echo ============================================================================
where kain >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] kain.exe not found in PATH
    echo [INFO] Ensure M:\Code\Kain\target\release is in your PATH
    exit /b 1
)

REM Verify kain.exe exists in the dev location
if not exist "M:\Code\Kain\target\release\kain.exe" (
    echo [ERROR] kain.exe not found at M:\Code\Kain\target\release\kain.exe
    echo [INFO] Run 'cargo build --release' in Kain repo to build the compiler
    exit /b 1
)

echo [SUCCESS] KAIN compiler found and ready (using dev build)

echo.
echo ============================================================================
echo [STEP 2/5] Cleaning build artifacts...
echo ============================================================================

REM Force delete _Builds folder
if exist "%OUTPUT_ROOT%" (
    echo [CLEAN] Removing _Builds folder...
    rmdir /s /q "%OUTPUT_ROOT%" 2>nul
    if exist "%OUTPUT_ROOT%" (
        echo [WARN] Could not fully remove _Builds, attempting file-by-file cleanup...
        del /f /s /q "%OUTPUT_ROOT%\*" 2>nul
        rmdir /s /q "%OUTPUT_ROOT%" 2>nul
    )
    echo [SUCCESS] _Builds folder cleaned
) else (
    echo [INFO] No _Builds folder to clean
)

REM Clean generated artifacts in ROOT plugin directory
echo [CLEAN] Cleaning root plugin directory artifacts...

REM Delete .uplugin files in root
for %%U in ("%SCRIPT_DIR%\*.uplugin") do (
    del /f /q "%%U" 2>nul
    echo   - Removed %%~nxU
)

if exist "%SCRIPT_DIR%\Config" (
    rmdir /s /q "%SCRIPT_DIR%\Config" 2>nul
    echo   - Removed Config
)
if exist "%SCRIPT_DIR%\Intermediate" (
    rmdir /s /q "%SCRIPT_DIR%\Intermediate" 2>nul
    echo   - Removed Intermediate
)
if exist "%SCRIPT_DIR%\Shaders" (
    rmdir /s /q "%SCRIPT_DIR%\Shaders" 2>nul
    echo   - Removed Shaders
)
if exist "%SCRIPT_DIR%\Source" (
    rmdir /s /q "%SCRIPT_DIR%\Source" 2>nul
    echo   - Removed Source
)
if exist "%SCRIPT_DIR%\Content" (
    rmdir /s /q "%SCRIPT_DIR%\Content" 2>nul
    echo   - Removed Content
)

REM Clean generated plugin folders (nested directories with .uplugin files)
for /d %%D in ("%SCRIPT_DIR%\*") do (
    set "FOLDER_NAME=%%~nxD"
    
    REM Skip certain folders
    if /i not "!FOLDER_NAME!"=="_Builds" if /i not "!FOLDER_NAME!"=="_Docs" if /i not "!FOLDER_NAME!"=="Docs" if /i not "!FOLDER_NAME!"=="Kain" (
        REM Check if this folder contains a .uplugin file (generated plugin folder)
        if exist "%%D\*.uplugin" (
            echo [CLEAN] Cleaning generated artifacts in !FOLDER_NAME!...
            
            REM Delete .uplugin files in nested folder
            for %%U in ("%%D\*.uplugin") do (
                del /f /q "%%U" 2>nul
                echo   - Removed !FOLDER_NAME!\%%~nxU
            )
            
            REM Delete Config, Intermediate, Shaders, Source, Content folders
            if exist "%%D\Config" (
                rmdir /s /q "%%D\Config" 2>nul
                echo   - Removed !FOLDER_NAME!\Config
            )
            if exist "%%D\Intermediate" (
                rmdir /s /q "%%D\Intermediate" 2>nul
                echo   - Removed !FOLDER_NAME!\Intermediate
            )
            if exist "%%D\Shaders" (
                rmdir /s /q "%%D\Shaders" 2>nul
                echo   - Removed !FOLDER_NAME!\Shaders
            )
            if exist "%%D\Source" (
                rmdir /s /q "%%D\Source" 2>nul
                echo   - Removed !FOLDER_NAME!\Source
            )
            if exist "%%D\Content" (
                rmdir /s /q "%%D\Content" 2>nul
                echo   - Removed !FOLDER_NAME!\Content
            )
        )
    )
)

echo [SUCCESS] Build artifacts cleaned

echo.
echo ============================================================================
echo [STEP 3/5] Regenerating C++ from KAIN source...
echo ============================================================================
kain build --ue5 > "%TEMP_LOG%" 2>&1
set "KAIN_RESULT=%ERRORLEVEL%"

if !KAIN_RESULT! neq 0 (
    echo [ERROR] KAIN compilation failed
    echo.
    echo ============================================================================
    echo KAIN COMPILATION ERRORS:
    echo ============================================================================
    type "%TEMP_LOG%"
    echo ============================================================================
    call :LogKainErrors "%TEMP_LOG%"
    call :LogKainToLocal "%TEMP_LOG%"
    del "%TEMP_LOG%" 2>nul
    exit /b 1
)

echo [SUCCESS] KAIN compilation complete
del "%TEMP_LOG%" 2>nul

echo.
echo ============================================================================
echo [STEP 4/5] Building plugin with UE5...
echo ============================================================================

echo.
echo ============================================================================
echo [STEP 5/5] Packaging plugin...
echo ============================================================================

REM After KAIN build, find the generated .uplugin file
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

:FoundGeneratedPlugin
echo [INFO] Using plugin file: %PLUGIN_FILE%

if not exist "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo [ERROR] Could not find UE 5.4 at: %UE_5_4%
    exit /b 1
)

REM BuildPlugin is smart - it does incremental builds automatically
set "UAT_UBT_ARGS="
if /I "%WARNINGS_AS_ERRORS%"=="0" (
    echo [INFO] UBT warnings-as-errors: OFF
    set "UAT_UBT_ARGS=-UbtArgs=-NoWarningsAsErrors"
) else (
    echo [INFO] UBT warnings-as-errors: ON
)
call "%UE_5_4%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%PLUGIN_FILE%" -Package="%OUTPUT_ROOT%\%PLUGIN_NAME%_5.4" -Rocket -TargetPlatforms=Win64 %UAT_UBT_ARGS% > "%TEMP_LOG%" 2>&1
set "BUILD_RESULT=%ERRORLEVEL%"

if !BUILD_RESULT! neq 0 (
    echo [FAILED] Build failed - check errors above
    type "%TEMP_LOG%"
    call :LogErrors "UE5 BUILD" "%TEMP_LOG%"
    call :LogToLocal "UE5 BUILD" "%TEMP_LOG%"
    del "%TEMP_LOG%" 2>nul
    exit /b 1
)

echo.
echo ============================================================================
echo [SUCCESS] Build complete!
echo ============================================================================
echo Plugin: %PLUGIN_NAME%
echo Build: %OUTPUT_ROOT%\%PLUGIN_NAME%_5.4
echo Local Log: %LOCAL_LOG%

REM Log success to local log
call :LogSuccess

del "%TEMP_LOG%" 2>nul
exit /b 0

REM ==============================================================================
REM Function: LogKainErrors - Append KAIN errors to COMBINEDLOG.md (full output)
REM Parameters: %1 = Log file path
REM ==============================================================================
:LogKainErrors
setlocal enabledelayedexpansion
set "LOG_FILE=%~1"

REM Create combined log if it doesn't exist
if not exist "%COMBINED_LOG%" (
    echo # Combined Build Errors Log > "%COMBINED_LOG%"
    echo. >> "%COMBINED_LOG%"
)

REM Append header
echo. >> "%COMBINED_LOG%"
echo ------------- >> "%COMBINED_LOG%"
echo %PLUGIN_NAME% - KAIN COMPILATION >> "%COMBINED_LOG%"
echo ------------- >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"

REM Append full KAIN output (it's already formatted nicely)
type "%LOG_FILE%" >> "%COMBINED_LOG%"

echo. >> "%COMBINED_LOG%"
echo [LOG] KAIN errors appended to: %COMBINED_LOG%
endlocal
exit /b 0

REM ==============================================================================
REM Function: LogKainToLocal - Write KAIN errors to local log file
REM Parameters: %1 = Log file path
REM ==============================================================================
:LogKainToLocal
setlocal enabledelayedexpansion
set "LOG_FILE=%~1"

REM Get current timestamp
for /f "tokens=1-4 delims=/ " %%a in ('date /t') do (
    set "BUILD_DATE=%%a %%b %%c %%d"
)
for /f "tokens=1-2 delims=: " %%a in ('time /t') do (
    set "BUILD_TIME=%%a:%%b"
)

REM Create/replace local log file
echo # Build Log - %PLUGIN_NAME% > "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo **Build Date**: %BUILD_DATE% %BUILD_TIME% >> "%LOCAL_LOG%"
echo **Status**: FAILED >> "%LOCAL_LOG%"
echo **Error Type**: KAIN COMPILATION >> "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo ## KAIN Compilation Errors >> "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo ```>> "%LOCAL_LOG%"

REM Append full KAIN output
type "%LOG_FILE%" >> "%LOCAL_LOG%"

echo ```>> "%LOCAL_LOG%"

echo [LOG] Local log updated: %LOCAL_LOG%
endlocal
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

REM ==============================================================================
REM Function: LogToLocal - Write/replace local log file with current build results
REM Parameters: %1 = Error type (KAIN/UE5), %2 = Log file path
REM ==============================================================================
:LogToLocal
setlocal enabledelayedexpansion
set "ERROR_TYPE=%~1"
set "LOG_FILE=%~2"
set "ERROR_COUNT=0"

REM Get current timestamp
for /f "tokens=1-4 delims=/ " %%a in ('date /t') do (
    set "BUILD_DATE=%%a %%b %%c %%d"
)
for /f "tokens=1-2 delims=: " %%a in ('time /t') do (
    set "BUILD_TIME=%%a:%%b"
)

REM Create/replace local log file
echo # Build Log - %PLUGIN_NAME% > "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo **Build Date**: %BUILD_DATE% %BUILD_TIME% >> "%LOCAL_LOG%"
echo **Status**: FAILED >> "%LOCAL_LOG%"
echo **Error Type**: %ERROR_TYPE% >> "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo ## Errors >> "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo ```>> "%LOCAL_LOG%"

REM Extract and append all errors
for /f "usebackq delims=" %%L in ("%LOG_FILE%") do (
    set "LINE=%%L"
    
    REM Check if line contains error indicators
    echo.!LINE! | findstr /i /c:"error" /c:"Error:" /c:"ERROR" /c:"failed" /c:"FAILED" >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        set /a ERROR_COUNT+=1
        echo !LINE! >> "%LOCAL_LOG%"
    )
)

echo ```>> "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo **Total Errors**: !ERROR_COUNT! >> "%LOCAL_LOG%"

echo [LOG] Local log updated: %LOCAL_LOG%
endlocal
exit /b 0

REM ==============================================================================
REM Function: LogSuccess - Write success to local log file
REM ==============================================================================
:LogSuccess
setlocal enabledelayedexpansion

REM Get current timestamp
for /f "tokens=1-4 delims=/ " %%a in ('date /t') do (
    set "BUILD_DATE=%%a %%b %%c %%d"
)
for /f "tokens=1-2 delims=: " %%a in ('time /t') do (
    set "BUILD_TIME=%%a:%%b"
)

REM Create/replace local log file with success message
echo # Build Log - %PLUGIN_NAME% > "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo **Build Date**: %BUILD_DATE% %BUILD_TIME% >> "%LOCAL_LOG%"
echo **Status**: SUCCESS >> "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo ## Build Output >> "%LOCAL_LOG%"
echo. >> "%LOCAL_LOG%"
echo - Plugin: %PLUGIN_NAME% >> "%LOCAL_LOG%"
echo - Build Location: %OUTPUT_ROOT%\%PLUGIN_NAME%_5.4 >> "%LOCAL_LOG%"
echo - KAIN Compilation: SUCCESS >> "%LOCAL_LOG%"
echo - UE5 Compilation: SUCCESS >> "%LOCAL_LOG%"

echo [LOG] Local log updated: %LOCAL_LOG%
endlocal
exit /b 0
