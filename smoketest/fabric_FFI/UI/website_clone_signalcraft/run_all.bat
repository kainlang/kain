@echo off
setlocal
call "%~dp0run_test.bat" || exit /b 1
call "%~dp0run_interpret.bat"
