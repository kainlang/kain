@echo off
set VULKAN_SDK=C:\VulkanSDK\1.4.350.0
set MSVC_LIB=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64
set UCRT=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64
set UM=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64
set LINKER=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe

rem Step 1: Compile LLVM IR to .obj
clang -c X:\blades\c\VULKAIN\.kain\out\x86_64-windows\dev\ll\vulkan_window_demo\compile\vulkan_window_demo.ll -o X:\blades\c\VULKAIN\.kain\out\vulkan_window_demo.obj -v
echo CLANG EXIT: %ERRORLEVEL%
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

rem Step 2: Link
set LIB=%MSVC_LIB%;%UCRT%;%UM%
"%LINKER%" /NOLOGO /SUBSYSTEM:CONSOLE /OUT:X:\blades\c\VULKAIN\vulkan_window_demo.exe ^
    X:\blades\c\VULKAIN\.kain\out\vulkan_window_demo.obj ^
    %VULKAN_SDK%\Lib\vulkan-1.lib ^
    X:\.kain\lib\kain_runtime.lib
echo LINK EXIT: %ERRORLEVEL%
