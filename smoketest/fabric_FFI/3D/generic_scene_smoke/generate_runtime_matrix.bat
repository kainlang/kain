@echo off
setlocal
set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%\..\..\.."
cargo run -p kain-3d --bin generic_scene_smoke -- --output-image "%SCRIPT_DIR%generic_scene_visual_reference.png" --output-json "%SCRIPT_DIR%generated\generic_scene_runtime_report.json"
popd
