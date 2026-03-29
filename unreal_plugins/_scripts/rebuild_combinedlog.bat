@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM REBUILD COMBINEDLOG - Runs FULLBUILD.bat on all plugins and logs ALL results
REM ==============================================================================

set "FACTORY_ROOT=%~dp0.."
set "COMBINED_LOG=%FACTORY_ROOT%\COMBINEDLOG.md"
set "TOTAL_PLUGINS=0"
set "SUCCESS_COUNT=0"
set "FAILED_COUNT=0"

echo ============================================================================
echo Rebuilding COMBINEDLOG.md with complete status for all plugins...
echo ============================================================================
echo.

REM Create new COMBINEDLOG.md with header
echo # Combined Build Status Log > "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"
echo **Generated**: %DATE% %TIME% >> "%COMBINED_LOG%"
echo **Purpose**: Complete build status for all Factory plugins >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"

REM Temporary file for summary table
set "TEMP_SUMMARY=%TEMP%\summary_%RANDOM%.txt"
set "TEMP_DETAILS=%TEMP%\details_%RANDOM%.txt"
echo. > "%TEMP_SUMMARY%"
echo. > "%TEMP_DETAILS%"

echo ============================================================================
echo [STEP 1] Verifying KAIN compiler...
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
echo.

REM Scan all plugin directories
for /d %%D in ("%FACTORY_ROOT%\*") do (
    set "PLUGIN_DIR=%%D"
    set "PLUGIN_NAME=%%~nxD"
    
    REM Skip _scripts, _Archive, _Docs directories
    if /i not "!PLUGIN_NAME!"=="_scripts" if /i not "!PLUGIN_NAME!"=="_Archive" if /i not "!PLUGIN_NAME!"=="_Docs" (
        REM Check if FULLBUILD.bat exists
        if exist "!PLUGIN_DIR!\FULLBUILD.bat" (
            set /a TOTAL_PLUGINS+=1
            
            echo.
            echo ============================================================================
            echo [!TOTAL_PLUGINS!] Building: !PLUGIN_NAME!
            echo ============================================================================
            
            REM Change to plugin directory and run FULLBUILD.bat
            pushd "!PLUGIN_DIR!"
            
            REM Capture output to temp file
            set "TEMP_LOG=%TEMP%\build_!PLUGIN_NAME!_%RANDOM%.log"
            call FULLBUILD.bat > "!TEMP_LOG!" 2>&1
            set "BUILD_RESULT=!ERRORLEVEL!"
            
            popd
            
            REM Append to details file
            echo. >> "%TEMP_DETAILS%"
            echo ============================================================================ >> "%TEMP_DETAILS%"
            
            if !BUILD_RESULT! equ 0 (
                echo [SUCCESS] !PLUGIN_NAME! built successfully
                echo - !PLUGIN_NAME!: SUCCESS (Complete) >> "%TEMP_SUMMARY%"
                echo ### !PLUGIN_NAME! - SUCCESS >> "%TEMP_DETAILS%"
                echo. >> "%TEMP_DETAILS%"
                echo **Status**: Build completed successfully >> "%TEMP_DETAILS%"
                echo **KAIN Compilation**: PASS >> "%TEMP_DETAILS%"
                echo **UE5 Build**: PASS >> "%TEMP_DETAILS%"
                echo. >> "%TEMP_DETAILS%"
                set /a SUCCESS_COUNT+=1
            ) else (
                echo [FAILED] !PLUGIN_NAME! build failed with error code !BUILD_RESULT!
                
                REM Determine failure stage
                findstr /i /c:"Parse error" "!TEMP_LOG!" >nul 2>&1
                if !ERRORLEVEL! equ 0 (
                    set "STAGE=KAIN Parse Error"
                    echo - !PLUGIN_NAME!: FAILED (KAIN Parse) >> "%TEMP_SUMMARY%"
                ) else (
                    findstr /i /c:"UnrealHeaderTool" "!TEMP_LOG!" >nul 2>&1
                    if !ERRORLEVEL! equ 0 (
                        set "STAGE=UE5 Build Error"
                        echo - !PLUGIN_NAME!: FAILED (UE5 Build) >> "%TEMP_SUMMARY%"
                    ) else (
                        set "STAGE=Unknown Error"
                        echo - !PLUGIN_NAME!: FAILED (Unknown) >> "%TEMP_SUMMARY%"
                    )
                )
                
                echo ### !PLUGIN_NAME! - FAILED >> "%TEMP_DETAILS%"
                echo. >> "%TEMP_DETAILS%"
                echo **Status**: Build failed >> "%TEMP_DETAILS%"
                echo **Failure Stage**: !STAGE! >> "%TEMP_DETAILS%"
                echo. >> "%TEMP_DETAILS%"
                echo #### Error Details >> "%TEMP_DETAILS%"
                echo. >> "%TEMP_DETAILS%"
                echo ```>> "%TEMP_DETAILS%"
                
                REM Extract errors from temp log
                set "ERROR_COUNT=0"
                for /f "usebackq delims=" %%L in ("!TEMP_LOG!") do (
                    set "LINE=%%L"
                    echo.!LINE! | findstr /i /c:"error" /c:"Error:" /c:"ERROR" /c:"failed" /c:"FAILED" >nul 2>&1
                    if !ERRORLEVEL! equ 0 (
                        set /a ERROR_COUNT+=1
                        if !ERROR_COUNT! lss 30 (
                            echo !LINE! >> "%TEMP_DETAILS%"
                        )
                    )
                )
                
                if !ERROR_COUNT! gtr 30 (
                    set /a REMAINING=!ERROR_COUNT! - 30
                    echo. >> "%TEMP_DETAILS%"
                    echo [TRUNCATED: !REMAINING! more errors not shown] >> "%TEMP_DETAILS%"
                )
                
                echo ```>> "%TEMP_DETAILS%"
                echo. >> "%TEMP_DETAILS%"
                echo **Total Errors**: !ERROR_COUNT! >> "%TEMP_DETAILS%"
                echo. >> "%TEMP_DETAILS%"
                
                set /a FAILED_COUNT+=1
            )
            
            REM Cleanup temp log
            del "!TEMP_LOG!" 2>nul
            
            echo ============================================================================ >> "%TEMP_DETAILS%"
        )
    )
)

REM Append summary
echo ## Summary >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"
type "%TEMP_SUMMARY%" >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"
echo **Total Plugins**: !TOTAL_PLUGINS! >> "%COMBINED_LOG%"
echo **Successful**: !SUCCESS_COUNT! >> "%COMBINED_LOG%"
echo **Failed**: !FAILED_COUNT! >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"
echo --- >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"
echo ## Detailed Results >> "%COMBINED_LOG%"
echo. >> "%COMBINED_LOG%"

REM Append details
type "%TEMP_DETAILS%" >> "%COMBINED_LOG%"

REM Cleanup
del "%TEMP_SUMMARY%" 2>nul
del "%TEMP_DETAILS%" 2>nul

echo.
echo ============================================================================
echo Rebuild complete!
echo ============================================================================
echo Total plugins: !TOTAL_PLUGINS!
echo Successful: !SUCCESS_COUNT!
echo Failed: !FAILED_COUNT!
echo.
echo COMBINEDLOG.md updated: %COMBINED_LOG%
echo.

REM Exit with error code if any builds failed
if !FAILED_COUNT! gtr 0 (
    exit /b 1
)

exit /b 0
