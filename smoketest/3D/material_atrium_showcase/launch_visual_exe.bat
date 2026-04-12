@echo off
setlocal
set EXE=%~dp0native-app\material-atrium-showcase.exe
set BACKEND=%~1
if not exist "%EXE%" (
  echo Expected executable was not found at %EXE%
  echo Build it first with build_visual_exe.bat
  exit /b 1
)
if not "%BACKEND%"=="" (
  set KAIN_RUNTIME_RENDERER_BACKEND=%BACKEND%
)
start "" "%EXE%"
