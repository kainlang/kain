@echo off
set VC_INSTALL_DIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
call "%VC_INSTALL_DIR%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
cd /d X:\llvm-kain\build2
cmake -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER=clang-cl -DLLVM_TABLEGEN_EXE=..\build\bin\dummy-tblgen.exe -DCLANG_TABLEGEN_EXE=..\build\bin\dummy-tblgen.exe ..
if errorlevel 1 exit /b 1
echo ---
echo Configuration successful!
