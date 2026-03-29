@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%"

echo [1/3] Regenerating shell and native bundle inputs...
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%scripts\build-native-ui.ps1"
if errorlevel 1 goto :fail

echo [2/3] Building native-app crate...
pushd "%SCRIPT_DIR%native-app"
cargo build
if errorlevel 1 goto :fail_native

echo [3/3] Syncing fresh exe to native-app root...
copy /Y "%SCRIPT_DIR%native-app\target\debug\kain-fabric-dcc-suite.exe" "%SCRIPT_DIR%native-app\kain-fabric-dcc-suite.exe" >nul
if errorlevel 1 goto :fail_native

echo.
echo Build complete:
echo   %SCRIPT_DIR%native-app\kain-fabric-dcc-suite.exe
echo.
popd
popd
exit /b 0

:fail_native
echo.
echo Native app build failed.
popd
popd
exit /b 1

:fail
echo.
echo Shell/native bundle generation failed.
popd
exit /b 1
