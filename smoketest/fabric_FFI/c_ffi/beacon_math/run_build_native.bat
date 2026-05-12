@echo off
setlocal
pushd "%~dp0"
clang -shared -O2 native\beacon_math.c -o native\beacon_math.dll
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
