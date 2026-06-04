@echo off
REM ============================================================================
REM   bazel_off.bat — cleanly shut down the Bazel server
REM ============================================================================
REM
REM  Shuts down the Bazel server to free JVM memory and file locks. Run this
REM  at the end of an agent session (or before a workspace rebuild).
REM
REM  Usage:  bazel_off           (uses repo at %CD%)
REM          bazel_off X:\        (explicit repo root)
REM          bazel_off X:\ dev    (explicit config)
REM
REM ============================================================================

setlocal enabledelayedexpansion

set "BAZEL_REPO=%~1"
set "BAZEL_CONFIG=%~2"

if "%BAZEL_REPO%"=="" set "BAZEL_REPO=%CD%"
if "%BAZEL_CONFIG%"=="" set "BAZEL_CONFIG=dev"
set "BAZEL_REPO_ARG=%BAZEL_REPO:\=/%"

cd /d "%BAZEL_REPO%" 2>nul
if errorlevel 1 (
    echo [bazel_off] ERROR: cannot cd to %BAZEL_REPO%
    exit /b 1
)

REM --- Step 1: Get the server PID before shutdown ---------------------------
for /f "tokens=*" %%P in ('bazel info server_pid --config=%BAZEL_CONFIG% 2^>nul ^| findstr /R "^[0-9]"') do set "OLD_PID=%%P"

if "!OLD_PID!"=="" (
    echo [bazel_off] No Bazel server running. Nothing to do.
    exit /b 0
)

echo [bazel_off] Shutting down Bazel server (PID: !OLD_PID!)...

REM --- Step 2: Shutdown -----------------------------------------------------
bazel shutdown --config=%BAZEL_CONFIG% 2>nul

REM --- Step 3: Verify shutdown succeeded ------------------------------------
set "RESULT=0"
for /f "tokens=*" %%P in ('bazel info server_pid --config=%BAZEL_CONFIG% 2^>nul ^| findstr /R "^[0-9]"') do set "RESULT=1"

if "!RESULT!"=="0" (
    echo [bazel_off] Bazel server shut down cleanly.
) else (
    echo [bazel_off] WARNING: Bazel server may still be running. Killing directly...
    taskkill /F /PID !OLD_PID! 2>nul
    if errorlevel 1 (
        echo [bazel_off] WARNING: could not kill PID !OLD_PID!. It may already be gone.
    ) else (
        echo [bazel_off] Bazel server forcefully killed.
    )
)

REM --- Step 4: Check for orphaned Bazel/Java processes ----------------------
echo [bazel_off] Checking for orphaned java.exe / bazel.exe / bazelisk.exe...
for /f "tokens=1,2" %%A in ('
    tasklist /NH /FI "IMAGENAME eq java.exe" /FI "WINDOWTITLE eq bazel*" 2^>nul ^| findstr /I java
') do (
    echo [bazel_off]   WARNING: possible orphan: java.exe (%%B)
)

REM --- Step 5: Prune Bazel storage back under the cap ----------------------
set "PYTHON_EXE="
set "PYTHON_ARGS="
for /f "delims=" %%P in ('where py 2^>nul') do if not defined PYTHON_EXE set "PYTHON_EXE=%%P"
if defined PYTHON_EXE (
    set "PYTHON_ARGS=-3"
) else (
    for /f "delims=" %%P in ('where python 2^>nul') do if not defined PYTHON_EXE set "PYTHON_EXE=%%P"
)
if defined PYTHON_EXE (
    echo [bazel_off] Pruning Bazel storage to keep the Z drive under the cap...
    "%PYTHON_EXE%" %PYTHON_ARGS% scripts\python\kain_bazel_sync.py --repo-root "%BAZEL_REPO_ARG%" prune-storage
    if errorlevel 1 (
        echo [bazel_off] WARNING: storage prune helper returned an error.
    )
) else (
    echo [bazel_off] WARNING: Python 3 was not found. Skipping storage prune.
)
echo [bazel_off] Done.

exit /b 0
