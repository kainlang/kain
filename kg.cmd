@echo off
setlocal
set "KG_EXE=%~dp0kg.exe"
if not exist "%KG_EXE%" (
  >&2 echo kg.exe not found next to this launcher. Build the `blades/kg` blade first.
  exit /b 1
)
"%KG_EXE%" %*
exit /b %errorlevel%
