@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM CLEAN BUILD LOGS - Deletes all BUILD_LOG.md files from plugin folders
REM ==============================================================================

set "FACTORY_ROOT=M:\Code\Factory"
set "DELETED_COUNT=0"

echo.
echo ============================================================================
echo Cleaning build logs from all plugin folders...
echo ============================================================================
echo.

REM Iterate through all subdirectories in Factory root
for /d %%D in ("%FACTORY_ROOT%\*") do (
    set "PLUGIN_DIR=%%D"
    set "BUILD_LOG=%%D\BUILD_LOG.md"
    set "CODEGEN_LOG=%%D\codegen_debug.log"
    set "EMBED_BAT=%%D\FULLBUILD_EMBED.bat"
    
    REM Check if BUILD_LOG.md exists in this directory
    if exist "!BUILD_LOG!" (
        echo [DELETE] !BUILD_LOG!
        del "!BUILD_LOG!" 2>nul
        if !ERRORLEVEL! equ 0 (
            set /a DELETED_COUNT+=1
        ) else (
            echo [ERROR] Failed to delete !BUILD_LOG!
        )
    )
    
    REM Check if codegen_debug.log exists in this directory
    if exist "!CODEGEN_LOG!" (
        echo [DELETE] !CODEGEN_LOG!
        del "!CODEGEN_LOG!" 2>nul
        if !ERRORLEVEL! equ 0 (
            set /a DELETED_COUNT+=1
        ) else (
            echo [ERROR] Failed to delete !CODEGEN_LOG!
        )
    )
    
    REM Check if FULLBUILD_EMBED.bat exists in this directory
    if exist "!EMBED_BAT!" (
        echo [DELETE] !EMBED_BAT!
        del "!EMBED_BAT!" 2>nul
        if !ERRORLEVEL! equ 0 (
            set /a DELETED_COUNT+=1
        ) else (
            echo [ERROR] Failed to delete !EMBED_BAT!
        )
    )
)

echo.
echo ============================================================================
echo Cleanup complete!
echo ============================================================================
echo Total files deleted: !DELETED_COUNT!
echo   - BUILD_LOG.md
echo   - codegen_debug.log
echo   - FULLBUILD_EMBED.bat
echo.

exit /b 0
