@echo off
setlocal enabledelayedexpansion

REM ==============================================================================
REM CLEAN ALL PLUGINS - Removes build artifacts for fresh compilation
REM ==============================================================================

set "FACTORY_ROOT=%~dp0.."
set "TOTAL_DELETED=0"
set "TOTAL_SIZE=0"

echo ============================================================================
echo Cleaning all plugin build artifacts...
echo ============================================================================
echo.

REM Scan all plugin directories
for /d %%D in ("%FACTORY_ROOT%\*") do (
    REM Skip _scripts and _Archive directories
    if /i not "%%~nxD"=="_scripts" if /i not "%%~nxD"=="_Archive" (
        set "PLUGIN_DIR=%%D"
        set "PLUGIN_NAME=%%~nxD"
        
        REM Check if this is a plugin directory (has .kn or .uplugin)
        set "IS_PLUGIN=0"
        if exist "!PLUGIN_DIR!\*.kn" set "IS_PLUGIN=1"
        if exist "!PLUGIN_DIR!\*.uplugin" set "IS_PLUGIN=1"
        
        REM Check nested folders for .uplugin
        for /d %%S in ("!PLUGIN_DIR!\*") do (
            if exist "%%S\*.uplugin" set "IS_PLUGIN=1"
        )
        
        if "!IS_PLUGIN!"=="1" (
            echo [!PLUGIN_NAME!]
            
            REM Clean _Builds folder
            if exist "!PLUGIN_DIR!\_Builds" (
                echo   Removing: _Builds\
                rd /s /q "!PLUGIN_DIR!\_Builds" 2>nul
                if !ERRORLEVEL! equ 0 (
                    set /a TOTAL_DELETED+=1
                    echo   [OK] _Builds deleted
                ) else (
                    echo   [WARN] Could not delete _Builds
                )
            )
            
            REM Clean Intermediate folder
            if exist "!PLUGIN_DIR!\Intermediate" (
                echo   Removing: Intermediate\
                rd /s /q "!PLUGIN_DIR!\Intermediate" 2>nul
                if !ERRORLEVEL! equ 0 (
                    set /a TOTAL_DELETED+=1
                    echo   [OK] Intermediate deleted
                ) else (
                    echo   [WARN] Could not delete Intermediate
                )
            )
            
            REM Clean .uplugin files (generated)
            for %%U in ("!PLUGIN_DIR!\*.uplugin") do (
                echo   Removing: %%~nxU
                del /f /q "%%U" 2>nul
                if !ERRORLEVEL! equ 0 (
                    set /a TOTAL_DELETED+=1
                    echo   [OK] %%~nxU deleted
                ) else (
                    echo   [WARN] Could not delete %%~nxU
                )
            )
            
            REM Clean Source folder (generated C++)
            if exist "!PLUGIN_DIR!\Source" (
                echo   Removing: Source\
                rd /s /q "!PLUGIN_DIR!\Source" 2>nul
                if !ERRORLEVEL! equ 0 (
                    set /a TOTAL_DELETED+=1
                    echo   [OK] Source deleted
                ) else (
                    echo   [WARN] Could not delete Source
                )
            )
            
            REM Clean Shaders folder (generated .usf)
            if exist "!PLUGIN_DIR!\Shaders" (
                echo   Removing: Shaders\
                rd /s /q "!PLUGIN_DIR!\Shaders" 2>nul
                if !ERRORLEVEL! equ 0 (
                    set /a TOTAL_DELETED+=1
                    echo   [OK] Shaders deleted
                ) else (
                    echo   [WARN] Could not delete Shaders
                )
            )
            
            REM Clean Content folder (generated assets)
            if exist "!PLUGIN_DIR!\Content" (
                echo   Removing: Content\
                rd /s /q "!PLUGIN_DIR!\Content" 2>nul
                if !ERRORLEVEL! equ 0 (
                    set /a TOTAL_DELETED+=1
                    echo   [OK] Content deleted
                ) else (
                    echo   [WARN] Could not delete Content
                )
            )
            
            REM Clean Config folder (generated configs)
            if exist "!PLUGIN_DIR!\Config" (
                echo   Removing: Config\
                rd /s /q "!PLUGIN_DIR!\Config" 2>nul
                if !ERRORLEVEL! equ 0 (
                    set /a TOTAL_DELETED+=1
                    echo   [OK] Config deleted
                ) else (
                    echo   [WARN] Could not delete Config
                )
            )
            
            REM Clean nested plugin folders (e.g., PluginName\PluginName\Source)
            for /d %%N in ("!PLUGIN_DIR!\*") do (
                set "NESTED_DIR=%%N"
                set "NESTED_NAME=%%~nxN"
                
                REM Skip _Builds folder (already handled)
                if /i not "!NESTED_NAME!"=="_Builds" (
                    REM Check if nested folder has .uplugin (indicates nested plugin structure)
                    if exist "!NESTED_DIR!\*.uplugin" (
                        echo   [Nested: !NESTED_NAME!]
                        
                        REM Clean nested .uplugin files
                        for %%U in ("!NESTED_DIR!\*.uplugin") do (
                            echo     Removing: !NESTED_NAME!\%%~nxU
                            del /f /q "%%U" 2>nul
                            if !ERRORLEVEL! equ 0 (
                                set /a TOTAL_DELETED+=1
                                echo     [OK] %%~nxU deleted
                            )
                        )
                        
                        REM Clean nested Intermediate
                        if exist "!NESTED_DIR!\Intermediate" (
                            echo     Removing: !NESTED_NAME!\Intermediate\
                            rd /s /q "!NESTED_DIR!\Intermediate" 2>nul
                            if !ERRORLEVEL! equ 0 (
                                set /a TOTAL_DELETED+=1
                                echo     [OK] Intermediate deleted
                            )
                        )
                        
                        REM Clean nested Source
                        if exist "!NESTED_DIR!\Source" (
                            echo     Removing: !NESTED_NAME!\Source\
                            rd /s /q "!NESTED_DIR!\Source" 2>nul
                            if !ERRORLEVEL! equ 0 (
                                set /a TOTAL_DELETED+=1
                                echo     [OK] Source deleted
                            )
                        )
                        
                        REM Clean nested Shaders
                        if exist "!NESTED_DIR!\Shaders" (
                            echo     Removing: !NESTED_NAME!\Shaders\
                            rd /s /q "!NESTED_DIR!\Shaders" 2>nul
                            if !ERRORLEVEL! equ 0 (
                                set /a TOTAL_DELETED+=1
                                echo     [OK] Shaders deleted
                            )
                        )
                        
                        REM Clean nested Content
                        if exist "!NESTED_DIR!\Content" (
                            echo     Removing: !NESTED_NAME!\Content\
                            rd /s /q "!NESTED_DIR!\Content" 2>nul
                            if !ERRORLEVEL! equ 0 (
                                set /a TOTAL_DELETED+=1
                                echo     [OK] Content deleted
                            )
                        )
                        
                        REM Clean nested Config
                        if exist "!NESTED_DIR!\Config" (
                            echo     Removing: !NESTED_NAME!\Config\
                            rd /s /q "!NESTED_DIR!\Config" 2>nul
                            if !ERRORLEVEL! equ 0 (
                                set /a TOTAL_DELETED+=1
                                echo     [OK] Config deleted
                            )
                        )
                    )
                )
            )
            
            echo.
        )
    )
)

echo ============================================================================
echo Cleanup complete!
echo ============================================================================
echo Total artifacts deleted: !TOTAL_DELETED!
echo.
echo All plugins are now in a clean state.
echo Run FULLBUILD.bat in any plugin to regenerate from KAIN source.
echo.
