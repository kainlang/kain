@echo off
setlocal
call "%~dp0theme_authoring_shell\build_native_exe.bat" || exit /b 1
call "%~dp0dock_layout_workbench\build_native_exe.bat" || exit /b 1
call "%~dp0surface_modes_gallery\build_native_exe.bat" || exit /b 1
call "%~dp0website_clone_signalcraft\build_native_exe.bat" || exit /b 1
