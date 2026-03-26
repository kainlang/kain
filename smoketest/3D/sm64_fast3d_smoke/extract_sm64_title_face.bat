@echo off
setlocal
pushd "%~dp0"
set "SM64_ROOT=%~1"
if "%SM64_ROOT%"=="" set "SM64_ROOT=M:\Code\Other\Research\sm64-master\sm64-master"
cargo run --manifest-path "..\..\..\Cargo.toml" -p kain-fast3d-runtime -- --extract-sm64-title-face "%SM64_ROOT%" --manifest-out "scene_manifest_title_face.json"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
