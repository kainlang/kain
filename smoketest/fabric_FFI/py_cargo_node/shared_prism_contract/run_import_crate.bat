@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -q -p cli -- import-crate shared_prism_lab --mode both --output outputs/generated --report-json outputs/generated/shared_prism_lab_report_override.json
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
