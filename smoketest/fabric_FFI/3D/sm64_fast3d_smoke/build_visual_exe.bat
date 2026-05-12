@echo off
setlocal
pushd "%~dp0"
cargo build --manifest-path "..\..\..\Cargo.toml" -p kain-fast3d-runtime --release
if errorlevel 1 (
    set "EXIT_CODE=%ERRORLEVEL%"
    popd
    exit /b %EXIT_CODE%
)
copy /Y "..\..\..\target\release\kain-fast3d-runtime.exe" "outputs\sm64_fast3d_smoke_viewer.exe" >nul
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
