@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -q -p cli -- import-crate quad_prism_lab --crate-path local_crate --mode both --output outputs/generated --report-json outputs/generated/quad_prism_lab_report_override.json
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
