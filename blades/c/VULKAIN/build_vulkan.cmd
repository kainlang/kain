@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 > nul

rem Step 1: Compile LLVM IR to object file
clang -c "X:\blades\c\VULKAIN\.kain\out\x86_64-windows\dev\ll\vulkan_window_demo\compile\vulkan_window_demo.ll" -o "X:\blades\c\VULKAIN\.kain\out\vulkan_window_demo.obj"
echo CLANG EXIT: %ERRORLEVEL%

rem Step 2: Link with MSVC link.exe (reads LIB env var)
link /NOLOGO /SUBSYSTEM:CONSOLE ^
    /OUT:X:\blades\c\VULKAIN\vulkan_window_demo.exe ^
    "X:\blades\c\VULKAIN\.kain\out\vulkan_window_demo.obj" ^
    X:\.kain\lib\kain_runtime.lib ^
    C:\VulkanSDK\1.4.350.0\Lib\vulkan-1.lib ^
    user32.lib gdi32.lib shell32.lib advapi32.lib ole32.lib uuid.lib ws2_32.lib winhttp.lib
echo LINK EXIT: %ERRORLEVEL%
