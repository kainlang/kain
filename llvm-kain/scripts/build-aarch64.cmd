@echo off
setlocal

REM Add MSVC CRT library path to LIB
set MSVC_LIB=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64
set LIB=%MSVC_LIB%;%LIB%

echo LIB=%LIB%

cd /d X:\llvm-kain
ninja -C build -j4 LLVMAArch64CodeGen %*
echo Exit code: %ERRORLEVEL%
