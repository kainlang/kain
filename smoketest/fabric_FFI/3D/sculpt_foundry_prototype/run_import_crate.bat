@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -q -p cli -- import-crate sculpt_foundry_backend --crate-path local_crate --mode both --output outputs/generated --report-json outputs/generated/sculpt_foundry_backend_report_override.json
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
