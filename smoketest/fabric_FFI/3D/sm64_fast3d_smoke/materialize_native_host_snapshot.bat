@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -p kain-driver --example materialize_fast3d_native_host -- snapshot
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
