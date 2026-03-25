@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli --bin kain -- smoketest/UI/spv_ui_surface_probe/smoke.kn -t test
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
