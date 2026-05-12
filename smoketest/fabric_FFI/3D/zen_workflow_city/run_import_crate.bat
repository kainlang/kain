@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -q -p cli -- import-crate workflow_city_lab --crate-path local_crate --mode both --output outputs/generated --report-json outputs/generated/workflow_city_lab_report_override.json
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
