@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli --bin kain -- build native-ui smoketest/UI/gpu_compute_surface_probe/smoke.kn --app-name ui_smoke_gpu_compute_surface_probe --window-title "UI Smoke - GPU Compute Surface Probe" -o smoketest/UI/gpu_compute_surface_probe/native-app
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
