@echo off
setlocal

REM Run the benchmark suite from the benchmark directory using the Python launcher.
pushd "%~dp0"
if errorlevel 1 (
    echo [ERROR] Failed to enter benchmark directory.
    exit /b 1
)

py run.py %*
set "EXIT_CODE=%ERRORLEVEL%"

popd
exit /b %EXIT_CODE%
