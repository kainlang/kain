@echo off
setlocal
call "%~dp0run_test.bat" || exit /b %ERRORLEVEL%
call "%~dp0run_interpret.bat" || exit /b %ERRORLEVEL%
