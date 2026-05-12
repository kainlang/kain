@echo off
setlocal
pushd "%~dp0"
if not exist outputs mkdir outputs
clang -shared -O2 native\image_fx.c -o native\image_fx.dll
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
