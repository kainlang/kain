@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- build native-ui smoketest/UI/surface_modes_gallery/smoke.kn --app-name ui_smoke_surface_modes_gallery --window-title "UI Smoke - Surface Modes Gallery" -o smoketest/UI/surface_modes_gallery/native-app
set "exit_code=%errorlevel%"
popd
exit /b %exit_code%
