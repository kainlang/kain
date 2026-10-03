@echo off
REM Sync built kain.exe into .kain\bin (managed layout) after a successful build.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set "PATH=D:\tools\bazel;C:\scoop\apps\llvm\current\bin;C:\scoop\apps\python312\current;%PATH%"
set "PYO3_PYTHON=C:\scoop\apps\python312\current\python.exe"
set "LIBCLANG_PATH=C:\scoop\apps\llvm\current\bin"
bazel build //:kain --config=dev
if errorlevel 1 exit /b 1
for /f "delims=" %%i in ('bazel cquery --config=dev --output=files //:kain 2^>nul') do set "KAIN_BIN=%%i"
echo Built: %KAIN_BIN%
if not exist ".kain\bin" mkdir ".kain\bin"
copy /y "%KAIN_BIN%" ".kain\bin\kain.exe"
".kain\bin\kain.exe" doctor
