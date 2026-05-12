@echo off
setlocal
pushd "%~dp0..\..\.."
if not exist generated mkdir generated
cargo run -q -p cli --bin kain -- gpu-artifacts smoketest/UI/spv_ui_surface_probe/smoke.kn -o generated/spv_ui_surface_probe
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
