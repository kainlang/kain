@echo off
setlocal
powershell -ExecutionPolicy Bypass -File "%~dp0build_visual_exe.ps1" %*
