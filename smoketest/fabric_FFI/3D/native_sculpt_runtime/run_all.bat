@echo off
setlocal
call "%~dp0run_build_native.bat"
if errorlevel 1 exit /b %ERRORLEVEL%
call "%~dp0run_import_crate.bat"
if errorlevel 1 exit /b %ERRORLEVEL%
call "%~dp0run_test.bat"
if errorlevel 1 exit /b %ERRORLEVEL%
call "%~dp0run_interpret.bat"
exit /b %ERRORLEVEL%
