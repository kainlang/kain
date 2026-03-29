@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM BUILD ALL PLUGINS (EMBED MODE) - Builds with --embed flag for debugging
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
    echo Building all plugins with --embed flag...
    echo ============================================================================
    echo.
    echo ============================================================================
    echo [STEP 0] Updating KAIN compiler...
    echo ============================================================================
    if exist "%KAIN_REPO%\crates\cli" (
        pushd "%KAIN_REPO%"
        cargo install --path crates/cli --force
        set "INSTALL_RESULT=!ERRORLEVEL!"
        popd
        
        if !INSTALL_RESULT! neq 0 (
            echo [ERROR] KAIN compiler update failed!
            echo Cannot proceed with builds.
            exit /b 1
        ) else (
            echo [SUCCESS] KAIN compiler updated
        )
    ) else (
        echo [WARN] KAIN repo not found at %KAIN_REPO%, using existing compiler
    )
)
echo.

REM Scan all plugin directories
for /d %%D in ("%FACTORY_ROOT%\*") do (
    set "PLUGIN_DIR=%%D"
    set "PLUGIN_NAME=%%~nxD"
    
    REM Skip _scripts and _Archive directories
    if /i not "!PLUGIN_NAME!"=="_scripts" if /i not "!PLUGIN_NAME!"=="_Archive" (
        REM Check if plugin has .kn files or .uplugin
        set "IS_PLUGIN=0"
        if exist "!PLUGIN_DIR!\*.kn" set "IS_PLUGIN=1"
        if exist "!PLUGIN_DIR!\*.uplugin" set "IS_PLUGIN=1"
        
        REM Check nested folders for .uplugin
        for /d %%S in ("!PLUGIN_DIR!\*") do (
            if exist "%%S\*.uplugin" set "IS_PLUGIN=1"
        )
        
        if "!IS_PLUGIN!"=="1" (
            set /a TOTAL_PLUGINS+=1
            
            if "%DRY_RUN%"=="1" (
                echo [!TOTAL_PLUGINS!] Would build with --embed: !PLUGIN_NAME!
                echo     Path: !PLUGIN_DIR!
                echo.
            ) else (
                echo.
                echo ============================================================================
                echo [!TOTAL_PLUGINS!] Building with --embed: !PLUGIN_NAME!
                echo ============================================================================
                
                REM Change to plugin directory and run kain build --ue5 --embed
                pushd "!PLUGIN_DIR!"
                
                echo Regenerating C++ with embedded KAIN markers...
                kain build --ue5 --embed
                set "KAIN_RESULT=!ERRORLEVEL!"
                
                if !KAIN_RESULT! neq 0 (
                    echo [FAILED] !PLUGIN_NAME! KAIN compilation failed
                    set /a FAILED_COUNT+=1
                    popd
                ) else (
                    echo [SUCCESS] KAIN compilation complete with markers
                    echo.
                    echo Building plugin with UE5...
                    
                    REM Find .uplugin file
                    set "PLUGIN_FILE="
                    for %%F in ("*.uplugin") do set "PLUGIN_FILE=%%F"
                    if "!PLUGIN_FILE!"=="" (
                        for /d %%N in (*) do (
                            for %%F in ("%%N\*.uplugin") do set "PLUGIN_FILE=%%F"
                        )
                    )
                    
                    if "!PLUGIN_FILE!"=="" (
                        echo [FAILED] No .uplugin file found
                        set /a FAILED_COUNT+=1
                    ) else (
                        set "OUTPUT_ROOT=!PLUGIN_DIR!\_Builds"
                        call "D:\Unreal\UE_5.4\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="!PLUGIN_FILE!" -Package="!OUTPUT_ROOT!\!PLUGIN_NAME!_5.4" -Rocket -TargetPlatforms=Win64 >nul 2>&1
                        set "BUILD_RESULT=!ERRORLEVEL!"
                        
                        if !BUILD_RESULT! equ 0 (
                            echo [SUCCESS] !PLUGIN_NAME! built successfully with embedded markers
                            set /a SUCCESS_COUNT+=1
                        ) else (
                            echo [FAILED] !PLUGIN_NAME! UE5 build failed
                            set /a FAILED_COUNT+=1
                        )
                    )
                    
                    popd
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
    echo To execute the builds with --embed, run: build_all_plugins_embed.bat
) else (
    echo Build complete!
    echo ============================================================================
    echo Total plugins: !TOTAL_PLUGINS!
    echo Successful: !SUCCESS_COUNT!
    echo Failed: !FAILED_COUNT!
    echo.
    echo All generated C++ files now contain embedded KAIN source markers.
    echo Use tools/cpp_to_kain.py to extract KAIN from generated C++.
)
echo.

REM Exit with error code if any builds failed
if "%DRY_RUN%"=="0" (
    if !FAILED_COUNT! gtr 0 (
        exit /b 1
    )
)

exit /b 0
