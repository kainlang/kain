@echo off
set REPO=X:\llvm-kain

echo ====================================================================
echo           CLEANUP-FOR-GIT — DRY RUN
echo           X:\llvm-kain
echo ====================================================================
echo.

echo === BUILD DIRECTORIES ===
for %%d in (build build-wsl build2 build2-libs-backup build3 build4 build5 build6 build7 build8 build_msvc) do (
  if exist "%REPO%\%%d" (
    echo   DELETE %%d\
  ) else (
    echo   SKIP  %%d\  (not found)
  )
)
echo.

echo === BACKUP ===
if exist "%REPO%\_llvm_bak" (
  echo   DELETE _llvm_bak  ^(2.6 GB LLVM original backup^)
) else (
  echo   SKIP  _llvm_bak\  (not found)
)
echo.

echo === EMPTY DIRECTORIES ===
if exist "%REPO%\.LLVM" (
  echo   DELETE .LLVM  ^(empty directory^)
) else (
  echo   SKIP  .LLVM\  (not found)
)
echo.

echo === TEMP FILES ===
if exist "%REPO%\build.log" echo   DELETE build.log (87 bytes)
if exist "%REPO%\cmake_output.txt" echo   DELETE cmake_output.txt (2923 bytes)
if exist "%REPO%\nul" echo   DELETE nul
echo.

echo === FILES TO UPDATE ===
echo   .gitignore  - added _llvm_bak/, build.log, cmake_output.txt, nul
echo   MEMORY.tsv  - prepended cleanup record
echo.

echo ====================================================================
echo           DRY RUN COMPLETE
echo ====================================================================
echo.
echo Estimated space to free: ~5 GB (13 build dirs + 6.9 GB backup)
echo Source directories preserved: src include clang lld rt tools cmake scripts
echo.
echo To execute, run:  scripts\cleanup-for-git --execute
echo.
if "%1"=="--execute" goto :execute
goto :eof

:execute
echo === EXECUTING CLEANUP ===
echo.
echo == Deleting build directories ==
rmdir /s /q "%REPO%\build"
rmdir /s /q "%REPO%\build-wsl"
rmdir /s /q "%REPO%\build2"
rmdir /s /q "%REPO%\build2-libs-backup"
rmdir /s /q "%REPO%\build3"
rmdir /s /q "%REPO%\build4"
rmdir /s /q "%REPO%\build5"
rmdir /s /q "%REPO%\build6"
rmdir /s /q "%REPO%\build7"
rmdir /s /q "%REPO%\build8"
rmdir /s /q "%REPO%\build_msvc"
echo   Build directories DELETED.
echo.
echo == Deleting backup ==
rmdir /s /q "%REPO%\_llvm_bak"
echo   _llvm_bak DELETED.
echo.
echo == Deleting empty directory ==
rmdir /s /q "%REPO%\.LLVM" 2>nul
echo   .LLVM DELETED.
echo.
echo == Deleting temp files ==
del /q "%REPO%\build.log" 2>nul
del /q "%REPO%\cmake_output.txt" 2>nul
del /q "%REPO%\nul" 2>nul
echo   Temp files DELETED.
echo.
echo === CLEANUP COMPLETE ===
