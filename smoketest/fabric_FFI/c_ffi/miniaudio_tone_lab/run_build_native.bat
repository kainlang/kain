@echo off
setlocal
pushd "%~dp0"
if not exist outputs mkdir outputs
clang -shared -O2 native\audio_tone_lab.c native\vendor\miniaudio.c -o native\audio_tone_lab.dll
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
