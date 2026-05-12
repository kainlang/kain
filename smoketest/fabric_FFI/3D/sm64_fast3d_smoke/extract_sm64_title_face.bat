@echo off
setlocal
pushd "%~dp0"
if not "%~1"=="" set "KAIN_FAST3D_SM64_ROOT=%~1"
if "%KAIN_FAST3D_SM64_ROOT%"=="" set "KAIN_FAST3D_SM64_ROOT=M:\Code\Other\Research\sm64-master\sm64-master"
cargo run --manifest-path "..\..\..\Cargo.toml" -p kain-fast3d-runtime -- --config "host_configs\title_face_extract.json"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
