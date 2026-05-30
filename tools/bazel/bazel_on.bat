@echo off
REM ============================================================================
REM   bazel_on.bat — ensure the Bazel server is running and warm
REM ============================================================================
REM
REM  Every kain command dispatches through Bazel. If the Bazel server is cold,
REM  the first command pays the full JVM boot + repository loading cost (30-90s).
REM  This script pre-starts the server so subsequent kain commands are snappy.
REM
REM  Usage:  bazel_on           (uses repo at %CD%)
REM          bazel_on X:\        (explicit repo root)
REM          bazel_on X:\ dev    (explicit config)
REM
REM  After running this, agents should check server health:
REM    bazel info server_pid --config=dev
REM    (returns a PID when the server is alive)
REM
REM ============================================================================

setlocal enabledelayedexpansion

set "BAZEL_REPO=%~1"
set "BAZEL_CONFIG=%~2"

if "%BAZEL_REPO%"=="" set "BAZEL_REPO=%CD%"
if "%BAZEL_CONFIG%"=="" set "BAZEL_CONFIG=dev"

echo [bazel_on] repo: %BAZEL_REPO%
echo [bazel_on] config: --config=%BAZEL_CONFIG%

REM --- Step 1: Check if server is already running ---------------------------
cd /d "%BAZEL_REPO%" 2>nul
if errorlevel 1 (
    echo [bazel_on] ERROR: cannot cd to %BAZEL_REPO%
    exit /b 1
)

for /f "tokens=*" %%P in ('bazel info server_pid --config=%BAZEL_CONFIG% 2^>nul ^| findstr /R "^[0-9]"') do set "EXISTING_PID=%%P"

if not "!EXISTING_PID!"=="" (
    echo [bazel_on] Bazel server already running (PID: !EXISTING_PID!)
) else (
    echo [bazel_on] Starting Bazel server (cold boot)...
    call bazel info server_pid --config=%BAZEL_CONFIG% >nul 2>&1
    if errorlevel 1 (
        echo [bazel_on] ERROR: failed to start Bazel server
        exit /b 1
    )
    for /f "tokens=*" %%P in ('bazel info server_pid --config=%BAZEL_CONFIG% 2^>nul ^| findstr /R "^[0-9]"') do set "NEW_PID=%%P"
    echo [bazel_on] Bazel server started (PID: !NEW_PID!)
)

REM --- Step 2: Output base path for debugging -------------------------------
for /f "tokens=*" %%B in ('bazel info output_base --config=%BAZEL_CONFIG% 2^>nul ^| findstr /R "^[A-Z]:/"') do set "OB=%%B"
echo [bazel_on] output_base: !OB!

REM --- Step 3: Quick repository cache touch to confirm repos are loaded -----
echo [bazel_on] Touching server to confirm repository state...
bazel info repository_cache --config=%BAZEL_CONFIG% >nul 2>&1
if errorlevel 1 (
    echo [bazel_on] WARNING: repository cache check failed
)

REM --- Step 4: Show server process info for process-check -------------------
echo [bazel_on] Bazel server status:
for /f "tokens=*" %%P in ('bazel info server_pid --config=%BAZEL_CONFIG% 2^>nul ^| findstr /R "^[0-9]"') do set "FINAL_PID=%%P"
echo [bazel_on]   PID:         !FINAL_PID!
for /f "tokens=*" %%B in ('bazel info output_base --config=%BAZEL_CONFIG% 2^>nul ^| findstr /R "^[A-Z]:/"') do set "FINAL_OB=%%B"
echo [bazel_on]   output_base: !FINAL_OB!
echo [bazel_on]   ready:       true
echo [bazel_on] Done. Bazel server is warm. kain commands will now reuse it.

exit /b 0
