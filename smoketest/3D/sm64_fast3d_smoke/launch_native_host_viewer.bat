@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -p kain-driver --example materialize_fast3d_native_host -- viewer
if errorlevel 1 (
    set "EXIT_CODE=%ERRORLEVEL%"
    popd
    exit /b %EXIT_CODE%
)
"outputs\native_host\sm64-fast3d-native-host-viewer.exe"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
