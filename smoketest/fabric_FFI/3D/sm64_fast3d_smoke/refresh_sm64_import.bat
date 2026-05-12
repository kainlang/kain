@echo off
setlocal
set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%refresh_sm64_import.ps1" %*
exit /b %ERRORLEVEL%
