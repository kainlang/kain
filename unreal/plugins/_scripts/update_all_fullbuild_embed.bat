@echo off
setlocal enabledelayedexpansion
REM ==============================================================================
REM Update/Create all FULLBUILD_EMBED.bat scripts
REM ==============================================================================

set "FACTORY_ROOT=%~dp0.."
set "TEMPLATE=%~dp0FULLBUILD_EMBED_TEMPLATE.bat"

if not exist "%TEMPLATE%" (
    echo [ERROR] Template not found: %TEMPLATE%
    exit /b 1
)

echo ============================================================================
echo Updating/Creating all FULLBUILD_EMBED.bat scripts...
echo ============================================================================
echo.

set "UPDATED=0"
set "CREATED=0"

for /d %%D in ("%FACTORY_ROOT%\*") do (
    REM Skip _scripts and _Builds directories
    if /i not "%%~nxD"=="_scripts" if /i not "%%~nxD"=="_Builds" if /i not "%%~nxD"=="_Archive" (
        REM Check if directory has a .kn file or .uplugin file (indicates it's a plugin)
        set "IS_PLUGIN=0"
        if exist "%%D\*.kn" set "IS_PLUGIN=1"
        if exist "%%D\*.uplugin" set "IS_PLUGIN=1"
        
        REM Check nested folders for .uplugin
        for /d %%S in ("%%D\*") do (
            if exist "%%S\*.uplugin" set "IS_PLUGIN=1"
        )
        
        if "!IS_PLUGIN!"=="1" (
            if exist "%%D\FULLBUILD_EMBED.bat" (
                echo Updating: %%~nxD\FULLBUILD_EMBED.bat
                copy /y "%TEMPLATE%" "%%D\FULLBUILD_EMBED.bat" >nul
                set /a UPDATED+=1
            ) else (
                echo Creating: %%~nxD\FULLBUILD_EMBED.bat
                copy /y "%TEMPLATE%" "%%D\FULLBUILD_EMBED.bat" >nul
                set /a CREATED+=1
            )
        )
    )
)

set /a TOTAL=UPDATED+CREATED

echo.
echo ============================================================================
echo Updated !UPDATED! existing FULLBUILD_EMBED.bat scripts
echo Created !CREATED! new FULLBUILD_EMBED.bat scripts
echo Total: !TOTAL! plugins configured
echo ============================================================================
echo.
echo All plugins now have FULLBUILD_EMBED.bat for debugging with embedded markers.
echo.
