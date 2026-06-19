@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 > nul
clang X:\blades\c\VULKAIN\test_vulkan.c C:\VulkanSDK\1.4.350.0\Lib\vulkan-1.lib -IC:\VulkanSDK\1.4.350.0\Include -o X:\blades\c\VULKAIN\test_vulkan.exe
echo CLANG EXIT: %ERRORLEVEL%
X:\blades\c\VULKAIN\test_vulkan.exe
echo EXE EXIT: %ERRORLEVEL%
