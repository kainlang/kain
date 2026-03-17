@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- build native-ui smoketest/UI/website_clone_signalcraft/smoke.kn --app-name ui_smoke_website_clone_signalcraft --window-title "UI Smoke - Signalcraft Landing" -o smoketest/UI/website_clone_signalcraft/native-app
set "exit_code=%errorlevel%"
popd
exit /b %exit_code%
