@echo off
setlocal
set SCRIPT_DIR=%~dp0
pushd "%SCRIPT_DIR%\..\..\.."
cargo run -p kain-3d --bin material_atrium_smoke -- --output-image "%SCRIPT_DIR%material_atrium_visual_example.png" --output-json "%SCRIPT_DIR%generated\material_atrium_runtime_matrix.json"
popd
