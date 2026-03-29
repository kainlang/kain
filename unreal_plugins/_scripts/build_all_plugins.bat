@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM BUILD ALL PLUGINS - Runs FULLBUILD.bat in every plugin directory
REM ==============================================================================

set "FACTORY_ROOT=%~dp0.."
set "KAIN_REPO=M:\Code\Kain"
set "DRY_RUN=0"
set "TOTAL_PLUGINS=0"
set "SUCCESS_COUNT=0"
set "FAILED_COUNT=0"

REM Check for --dry-run flag
if /i "%~1"=="--dry-run" set "DRY_RUN=1"

if "%DRY_RUN%"=="1" (
    echo ============================================================================
    echo DRY RUN MODE - No builds will be executed
    echo ============================================================================
) else (
    echo ============================================================================
    echo Building all plugins...
    echo ============================================================================
    echo.
    echo ============================================================================
    echo [STEP 0] Verifying KAIN compiler...
    echo ============================================================================
    set "KAIN_EXE="
    for /f "delims=" %%K in ('where kain 2^>nul') do (
        if not defined KAIN_EXE set "KAIN_EXE=%%K"
    )
    if not defined KAIN_EXE (
        echo [ERROR] kain.exe not found in PATH
        echo [INFO] Run M:\Code\Kain\scripts\sync-kain-source-of-truth.ps1
        exit /b 1
    )

    echo [SUCCESS] KAIN compiler found and ready: !KAIN_EXE!
)
echo.

REM Scan all plugin directories
for /d %%D in ("%FACTORY_ROOT%\*") do (
    set "PLUGIN_DIR=%%D"
    set "PLUGIN_NAME=%%~nxD"
    
    REM Skip _scripts and _Archive directories
    if /i not "!PLUGIN_NAME!"=="_scripts" if /i not "!PLUGIN_NAME!"=="_Archive" (
        REM Check if FULLBUILD.bat exists
        if exist "!PLUGIN_DIR!\FULLBUILD.bat" (
            set /a TOTAL_PLUGINS+=1
            
            if "%DRY_RUN%"=="1" (
                echo [!TOTAL_PLUGINS!] Would build: !PLUGIN_NAME!
                echo     Path: !PLUGIN_DIR!\FULLBUILD.bat
                echo.
            ) else (
                echo.
                echo ============================================================================
                echo [!TOTAL_PLUGINS!] Building: !PLUGIN_NAME!
                echo ============================================================================
                
                REM Change to plugin directory and run FULLBUILD.bat
                pushd "!PLUGIN_DIR!"
                call FULLBUILD.bat
                set "BUILD_RESULT=!ERRORLEVEL!"
                popd
                
                if !BUILD_RESULT! equ 0 (
                    echo [SUCCESS] !PLUGIN_NAME! built successfully
                    set /a SUCCESS_COUNT+=1
                ) else (
                    echo [FAILED] !PLUGIN_NAME! build failed with error code !BUILD_RESULT!
                    set /a FAILED_COUNT+=1
                )
                
                echo.
            )
        )
    )
)

echo.
echo ============================================================================
if "%DRY_RUN%"=="1" (
    echo Dry run complete!
    echo ============================================================================
    echo Total plugins found: !TOTAL_PLUGINS!
    echo.
    echo To execute the builds, run: build_all_plugins.bat
) else (
    echo Build complete!
    echo ============================================================================
    echo Total plugins: !TOTAL_PLUGINS!
    echo Successful: !SUCCESS_COUNT!
    echo Failed: !FAILED_COUNT!
    echo.
    echo Check %FACTORY_ROOT%\COMBINEDLOG.md for error details.
)
echo.

REM Exit with error code if any builds failed
if "%DRY_RUN%"=="0" (
    if !FAILED_COUNT! gtr 0 (
        exit /b 1
    )
)

exit /b 0
