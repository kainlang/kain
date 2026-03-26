@echo off
setlocal
pushd "%~dp0"
cargo build --manifest-path "local_crate\Cargo.toml" --release
if errorlevel 1 (
    set "EXIT_CODE=%ERRORLEVEL%"
    popd
    exit /b %EXIT_CODE%
)
copy /Y "local_crate\target\release\sm64_fast3d_smoke_viewer.exe" "outputs\sm64_fast3d_smoke_viewer.exe" >nul
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
