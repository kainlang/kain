@echo off
setlocal
call "%~dp0theme_authoring_shell\launch_native_exe.bat" || exit /b 1
call "%~dp0dock_layout_workbench\launch_native_exe.bat" || exit /b 1
call "%~dp0surface_modes_gallery\launch_native_exe.bat" || exit /b 1
call "%~dp0website_clone_signalcraft\launch_native_exe.bat" || exit /b 1
