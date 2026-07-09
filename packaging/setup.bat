@echo off
REM ===========================================================================
REM  Kain Setup for Windows — double-click or run from terminal
REM  Adds Kain to your PATH and sets KAIN_HOME permanently.
REM
REM  Usage:
REM    setup.bat             — install Kain into user environment
REM    setup.bat --uninstall — remove Kain from user environment
REM    setup.bat --system    — system-wide install (admin required)
REM ===========================================================================

setlocal enabledelayedexpansion

REM Detect the Kain distribution root (parent of this script)
set "KAIN_HOME=%~dp0"
if "%KAIN_HOME:~-1%"=="\" set "KAIN_HOME=%KAIN_HOME:~0,-1%"

REM Check if it's a valid Kain distribution
if not exist "%KAIN_HOME%\bin\kain.exe" (
    echo.
    echo  [ERROR] kain.exe not found in bin/
    echo  Make sure setup.bat is in the Kain distribution folder.
    echo.
    pause
    exit /b 1
)

REM Parse arguments
set "SCOPE=User"
set "MODE=install"

:parse
if "%~1"=="--uninstall" set "MODE=uninstall" & shift & goto parse
if "%~1"=="--system" set "SCOPE=Machine" & shift & goto parse
if "%~1"=="--user" set "SCOPE=User" & shift & goto parse
goto :main

:main
echo.
echo  ============================================================
echo   Kain Setup — Windows
echo   Home: %KAIN_HOME%
echo   Scope: %SCOPE%
echo   Mode: %MODE%
echo  ============================================================
echo.

if "%MODE%"=="uninstall" goto :uninstall

:install
REM Set KAIN_HOME
setx KAIN_HOME "%KAIN_HOME%" /%SCOPE% > nul 2>&1
if %errorlevel% equ 0 (
    echo  [OK] KAIN_HOME set to %KAIN_HOME%
) else (
    echo  [WARN] Could not set KAIN_HOME — try running as Administrator
)

REM Add bin\ to PATH
set "BIN_DIR=%KAIN_HOME%\bin"
for /f "skip=2 tokens=3*" %%a in ('reg query "HKEY_CURRENT_USER\Environment" /v PATH 2^>nul') do set "USER_PATH=%%a%%b"
if "%USER_PATH%"=="" for /f "skip=2 tokens=3*" %%a in ('reg query "HKEY_CURRENT_USER\Environment" /v PATH 2^>nul') do set "USER_PATH=%%a%%b"

if not "%USER_PATH%"=="" (
    echo "%USER_PATH%" | find /i "%BIN_DIR%" > nul
    if !errorlevel! equ 0 (
        echo  [OK] %BIN_DIR% already in PATH
    ) else (
        setx PATH "%USER_PATH%;%BIN_DIR%" /%SCOPE% > nul 2>&1
        if !errorlevel! equ 0 (
            echo  [OK] Added %BIN_DIR% to PATH
        ) else (
            echo  [WARN] Could not update PATH — try running as Administrator
        )
    )
) else (
    setx PATH "%BIN_DIR%" /%SCOPE% > nul 2>&1
    if !errorlevel! equ 0 (
        echo  [OK] Added %BIN_DIR% to PATH
    ) else (
        echo  [WARN] Could not update PATH — try running as Administrator
    )
)

REM Add toolchain\llvm\bin to PATH
set "LLVM_DIR=%KAIN_HOME%\toolchain\llvm\bin"
if exist "%LLVM_DIR%\clang.exe" (
    echo "%USER_PATH%" | find /i "%LLVM_DIR%" > nul
    if !errorlevel! neq 0 (
        setx PATH "%USER_PATH%;%LLVM_DIR%" /%SCOPE% > nul 2>&1
        echo  [OK] Added LLVM toolchain to PATH
    ) else (
        echo  [OK] LLVM toolchain already in PATH
    )
)

echo.
echo  [OK] Kain setup complete!
echo.
echo  Next steps:
echo    1. Close and reopen your terminal
echo    2. Run: kain doctor
echo    3. Start coding: kain run your_file.kn
echo.
pause
goto :eof

:uninstall
REM Remove KAIN_HOME
setx KAIN_HOME "" /%SCOPE% > nul 2>&1
echo  [OK] KAIN_HOME removed

REM Remove bin\ from PATH
set "BIN_DIR=%KAIN_HOME%\bin"
for /f "skip=2 tokens=3*" %%a in ('reg query "HKEY_CURRENT_USER\Environment" /v PATH 2^>nul') do set "USER_PATH=%%a%%b"
if "%USER_PATH%"=="" for /f "skip=2 tokens=3*" %%a in ('reg query "HKEY_CURRENT_USER\Environment" /v PATH 2^>nul') do set "USER_PATH=%%a%%b"

if not "%USER_PATH%"=="" (
    set "NEW_PATH="
    for %%p in ("%USER_PATH:;=";"%") do (
        if /i "%%~p" neq "%BIN_DIR%" (
            if /i "%%~p" neq "%KAIN_HOME%\toolchain\llvm\bin" (
                if defined NEW_PATH (
                    set "NEW_PATH=!NEW_PATH!;%%~p"
                ) else (
                    set "NEW_PATH=%%~p"
                )
            )
        )
    )
    setx PATH "!NEW_PATH!" /%SCOPE% > nul 2>&1
    echo  [OK] Removed Kain directories from PATH
)

echo.
echo  [OK] Kain has been removed from your environment.
echo.
pause
goto :eof
