@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ASSET_DIR=%SCRIPT_DIR%assets"
set "TARGET_FONT=%ASSET_DIR%\ui_smoke_default.ttf"

if not exist "%ASSET_DIR%" mkdir "%ASSET_DIR%"

if not exist "%TARGET_FONT%" (
    for %%F in (
        "C:\Windows\Fonts\segoeui.ttf"
        "C:\Windows\Fonts\arial.ttf"
        "C:\Windows\Fonts\calibri.ttf"
    ) do (
        if not exist "%TARGET_FONT%" if exist "%%~fF" (
            copy /Y "%%~fF" "%TARGET_FONT%" >nul
        )
    )
)

if not exist "%TARGET_FONT%" (
    echo Failed to stage ui_smoke_default.ttf from the Windows font directory.
    exit /b 1
)

pushd "%~dp0..\..\.."
cargo run -q -p cli --bin kain -- build native-ui smoketest/UI/spv_ui_surface_probe/smoke.kn --app-name ui_smoke_spv_ui_surface_probe --window-title "UI Smoke - SPV UI Surface Probe" -o smoketest/UI/spv_ui_surface_probe/native-app
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
