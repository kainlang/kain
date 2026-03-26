@echo off
setlocal
pushd "%~dp0"
if not exist "scene_manifest_title_face.json" (
    call "%~dp0extract_sm64_title_face.bat"
    if errorlevel 1 (
        set "EXIT_CODE=%ERRORLEVEL%"
        popd
        exit /b %EXIT_CODE%
    )
)
if not exist "outputs\sm64_fast3d_smoke_viewer.exe" (
    call "%~dp0build_visual_exe.bat"
    if errorlevel 1 (
        set "EXIT_CODE=%ERRORLEVEL%"
        popd
        exit /b %EXIT_CODE%
    )
)
start "" "outputs\sm64_fast3d_smoke_viewer.exe" "scene_manifest_title_face.json"
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
