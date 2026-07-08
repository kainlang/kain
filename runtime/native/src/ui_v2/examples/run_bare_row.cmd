@echo off
echo Running from: %CD%
"%~dp0bare_row.exe"
echo Exit code: %ERRORLEVEL%
pause
