@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- build native-ui smoketest/UI/dock_layout_workbench/smoke.kn --app-name ui_smoke_dock_layout_workbench --window-title "UI Smoke - Dock Layout Workbench" -o smoketest/UI/dock_layout_workbench/native-app
set "exit_code=%errorlevel%"
popd
exit /b %exit_code%
