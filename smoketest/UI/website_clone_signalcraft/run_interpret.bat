@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- smoketest/UI/website_clone_signalcraft/smoke.kn -t interpret
set "exit_code=%errorlevel%"
popd
exit /b %exit_code%
