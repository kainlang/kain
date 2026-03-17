@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- smoketest/UI/dock_layout_workbench/smoke.kn -t interpret
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
