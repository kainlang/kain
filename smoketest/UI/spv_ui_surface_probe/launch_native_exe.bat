@echo off
setlocal
set "exe_path="
for %%F in ("%~dp0native-app\*.exe") do if not defined exe_path set "exe_path=%%~fF"
if not defined exe_path (
    call "%~dp0build_native_exe.bat" || exit /b 1
    for %%F in ("%~dp0native-app\*.exe") do if not defined exe_path set "exe_path=%%~fF"
)
if not defined exe_path (
    echo Native UI executable was not produced.
    exit /b 1
)
start "" "%exe_path%"
