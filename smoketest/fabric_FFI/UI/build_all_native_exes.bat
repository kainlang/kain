@echo off
setlocal
call "%~dp0theme_authoring_shell\build_native_exe.bat" || exit /b 1
call "%~dp0dock_layout_workbench\build_native_exe.bat" || exit /b 1
call "%~dp0surface_modes_gallery\build_native_exe.bat" || exit /b 1
call "%~dp0spv_ui_surface_probe\build_native_exe.bat" || exit /b 1
call "%~dp0gpu_compute_surface_probe\build_native_exe.bat" || exit /b 1
powershell -ExecutionPolicy Bypass -File "%~dp0kinetic_ui_atlas\build_native_exe.ps1" || exit /b 1
call "%~dp0website_clone_signalcraft\build_native_exe.bat" || exit /b 1
