@echo off
setlocal

REM Run the benchmark suite from the benchmark directory using the unified control plane.
pushd "%~dp0"
if errorlevel 1 (
    echo [ERROR] Failed to enter benchmark directory.
    exit /b 1
)

py bench.py run %*
set "EXIT_CODE=%ERRORLEVEL%"

popd
exit /b %EXIT_CODE%
