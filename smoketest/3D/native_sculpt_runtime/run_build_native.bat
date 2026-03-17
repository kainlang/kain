@echo off
setlocal
pushd "%~dp0"
if not exist outputs mkdir outputs
clang -shared -O2 -D_CRT_SECURE_NO_WARNINGS native\native_sculpt_host.c -lgdi32 -luser32 -o native\native_sculpt_host.dll
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
