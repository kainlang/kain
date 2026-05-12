@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- smoketest/python/pygame_poster/smoke.kn -t test
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
