@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- build native-ui smoketest/UI/theme_authoring_shell/smoke.kn --app-name ui_smoke_theme_authoring_shell --window-title "UI Smoke - Theme Authoring Shell" -o smoketest/UI/theme_authoring_shell/native-app
set "exit_code=%errorlevel%"
popd
exit /b %exit_code%
