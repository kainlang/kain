@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM ORGANIZE DOCS - Moves all .md files (except README.md) into /Docs folders
REM ==============================================================================

set "FACTORY_ROOT=%~dp0.."
set "TOTAL_MOVED=0"
set "TOTAL_PLUGINS=0"

echo ============================================================================
echo Organizing documentation files...
echo ============================================================================
echo.

REM Scan all plugin directories
for /d %%D in ("%FACTORY_ROOT%\*") do (
    set "PLUGIN_DIR=%%D"
    set "PLUGIN_NAME=%%~nxD"
    set "MOVED_COUNT=0"
    
    REM Skip _scripts and _Archive directories
    if /i not "!PLUGIN_NAME!"=="_scripts" if /i not "!PLUGIN_NAME!"=="_Archive" (
        REM Check if this is a plugin directory (has .kn or .uplugin)
        set "IS_PLUGIN=0"
        if exist "!PLUGIN_DIR!\*.kn" set "IS_PLUGIN=1"
        if exist "!PLUGIN_DIR!\*.uplugin" set "IS_PLUGIN=1"
        
        REM Check nested folders for .uplugin
        for /d %%S in ("!PLUGIN_DIR!\*") do (
            if exist "%%S\*.uplugin" set "IS_PLUGIN=1"
        )
        
        if "!IS_PLUGIN!"=="1" (
            set "DOCS_DIR=!PLUGIN_DIR!\Docs"
            set "HAS_DOCS=0"
            
            REM Find all .md files in root (excluding README.md)
            for %%F in ("!PLUGIN_DIR!\*.md") do (
                set "FILE_NAME=%%~nxF"
                set "FILE_PATH=%%F"
                set "SKIP=0"
                
                REM Skip README.md (case insensitive check)
                if /i "!FILE_NAME!"=="README.md" set "SKIP=1"
                
                if "!SKIP!"=="0" (
                    set "HAS_DOCS=1"
                    
                    REM Create Docs folder if it doesn't exist
                    if not exist "!DOCS_DIR!" mkdir "!DOCS_DIR!" 2>nul
                    
                    REM Move the file
                    move /y "!FILE_PATH!" "!DOCS_DIR!\!FILE_NAME!" >nul 2>&1
                    if !ERRORLEVEL! equ 0 (
                        set /a MOVED_COUNT+=1
                        set /a TOTAL_MOVED+=1
                    )
                )
            )
            
            REM Check nested plugin folders
            for /d %%N in ("!PLUGIN_DIR!\*") do (
                set "NESTED_DIR=%%N"
                set "NESTED_NAME=%%~nxN"
                
                REM Check if nested folder has .uplugin
                if exist "!NESTED_DIR!\*.uplugin" (
                    REM Find all .md files in nested folder (excluding README.md)
                    for %%F in ("!NESTED_DIR!\*.md") do (
                        set "FILE_NAME=%%~nxF"
                        set "FILE_PATH=%%F"
                        set "SKIP=0"
                        
                        REM Skip README.md (case insensitive check)
                        if /i "!FILE_NAME!"=="README.md" set "SKIP=1"
                        
                        if "!SKIP!"=="0" (
                            set "HAS_DOCS=1"
                            
                            REM Create Docs folder if it doesn't exist
                            if not exist "!DOCS_DIR!" mkdir "!DOCS_DIR!" 2>nul
                            
                            REM Move the file
                            move /y "!FILE_PATH!" "!DOCS_DIR!\!FILE_NAME!" >nul 2>&1
                            if !ERRORLEVEL! equ 0 (
                                set /a MOVED_COUNT+=1
                                set /a TOTAL_MOVED+=1
                            )
                        )
                    )
                )
            )
            
            REM Report if any files were moved
            if !HAS_DOCS! equ 1 (
                echo [!PLUGIN_NAME!] Moved !MOVED_COUNT! doc files to Docs folder
                set /a TOTAL_PLUGINS+=1
            )
        )
    )
)

echo.
echo ============================================================================
echo Documentation organized!
echo ============================================================================
echo Total files moved: !TOTAL_MOVED!
echo Plugins affected: !TOTAL_PLUGINS!
echo.
echo All .md files (except README.md) are now in /Docs folders.
echo.
