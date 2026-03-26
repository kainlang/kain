@echo off
setlocal
set "exe_path=%~dp0visual-native-app\fabric-studio-3d-editor.exe"
if exist "%exe_path%" goto launch
set "exe_path="
for %%F in ("%~dp0visual-native-app\*.exe") do if not defined exe_path set "exe_path=%%~fF"
if not defined exe_path (
    call "%~dp0build_visual_exe.bat" || exit /b 1
    if exist "%~dp0visual-native-app\fabric-studio-3d-editor.exe" set "exe_path=%~dp0visual-native-app\fabric-studio-3d-editor.exe"
    for %%F in ("%~dp0visual-native-app\*.exe") do if not defined exe_path set "exe_path=%%~fF"
)
if not defined exe_path (
    echo Fabric visual executable was not produced.
    exit /b 1
)
:launch
start "" "%exe_path%"
