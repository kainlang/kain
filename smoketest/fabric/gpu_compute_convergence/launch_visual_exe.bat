@echo off
setlocal
set "exe_path="
for %%F in ("%~dp0visual-native-app\*.exe") do if not defined exe_path set "exe_path=%%~fF"
if not defined exe_path (
    call "%~dp0build_visual_exe.bat" || exit /b 1
    for %%F in ("%~dp0visual-native-app\*.exe") do if not defined exe_path set "exe_path=%%~fF"
)
if not defined exe_path (
    echo Fabric visual executable was not produced.
    exit /b 1
)
start "" "%exe_path%"
