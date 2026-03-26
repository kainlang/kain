@echo off
setlocal
pushd "%~dp0"
if not exist "outputs\sm64_fast3d_smoke_viewer.exe" (
    call "%~dp0build_visual_exe.bat"
    if errorlevel 1 (
        set "EXIT_CODE=%ERRORLEVEL%"
        popd
        exit /b %EXIT_CODE%
    )
)
start "" "outputs\sm64_fast3d_smoke_viewer.exe" "scene_manifest.json"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
