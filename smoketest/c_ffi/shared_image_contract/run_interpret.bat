@echo off
setlocal
pushd "%~dp0"
..\..\..\target\debug\kain.exe run smoke.kn
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
