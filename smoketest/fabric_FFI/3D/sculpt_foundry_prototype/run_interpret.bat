@echo off
setlocal
pushd "%~dp0"
cargo run --manifest-path "..\..\..\Cargo.toml" -q -p cli -- smoke.kn -t interpret
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
