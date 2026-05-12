@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -q -p cli -- import-crate py_cargo_canvas --mode both --output outputs/generated --report-json outputs/generated/py_cargo_canvas_report_override.json
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
