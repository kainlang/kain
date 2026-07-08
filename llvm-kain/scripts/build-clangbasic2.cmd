@echo off
setlocal
set MSVC_LIB=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64
set LIB=%MSVC_LIB%;%LIB%
cd /d X:\llvm-kain
ninja -C build -j4 clangBasic > X:\llvm-kain\build\clangbasic_out.txt 2>&1
echo EXIT: %ERRORLEVEL%
