@echo off
REM Kain D:/ workstation env — VS2022 BuildTools + Scoop LLVM/Python + D:/ Bazel.
REM Usage: call dev_D.bat [bazel args...]  (defaults to build //:kain --config=dev)
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=D:\tools\bazel;C:\scoop\apps\llvm\current\bin;C:\scoop\apps\python312\current;%PATH%"
set "PYO3_PYTHON=C:\scoop\apps\python312\current\python.exe"
set "LIBCLANG_PATH=C:\scoop\apps\llvm\current\bin"
REM Keep Bazel temp + TMP on D:/_b (absolute + short — cargo build scripts need it).
set "TMP=D:\_b\tmp"
set "TEMP=D:\_b\tmp"
set "TMPDIR=D:\_b\tmp"
if "%~1"=="" (
  bazel build //:kain --config=dev
) else (
  bazel %*
)
