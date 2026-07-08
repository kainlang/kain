@echo off
setlocal
set INC_DIR=X:\llvm-kain\clang\include\clang\Basic

REM Create forwarding .inc files for all Diagnostic* .inc files from ../basic/
for %%f in (X:\llvm-kain\clang\include\basic\Diagnostic*.inc) do (
    echo // llvm-kain forwarding header > "%INC_DIR%\%%~nxf"
    echo #pragma once >> "%INC_DIR%\%%~nxf"
    echo #include "../basic/%%~nxf" >> "%INC_DIR%\%%~nxf"
)

echo Created forwarding .inc files in %INC_DIR%
dir /b "%INC_DIR%\*.inc"
