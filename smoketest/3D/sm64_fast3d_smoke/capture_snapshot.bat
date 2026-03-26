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
"outputs\sm64_fast3d_smoke_viewer.exe" "scene_manifest.json" --snapshot "outputs\sm64_fast3d_snapshot.png" --time-seconds 1.8
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
