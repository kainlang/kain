@echo off
setlocal
pushd "%~dp0"
if not exist outputs mkdir outputs
clang -shared -O2 -D_CRT_SECURE_NO_WARNINGS native\cgltf_scene_probe.c -o native\cgltf_scene_probe.dll
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
