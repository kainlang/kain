@echo off
setlocal enabledelayedexpansion

REM ==========================================================================
REM  Kain Native UI — RETRO WAVE 2084 Build Script
REM ==========================================================================
REM  Builds the synthwave/cyberpunk UI demo.
REM
REM  Usage: build.bat             — build retrowave.exe
REM         build.bat cosmic      — build cosmic_dashboard.exe
REM         build.bat inferno     — build font_inferno.exe
REM         build.bat run         — build + run retrowave.exe
REM         build.bat clean       — clean binaries
REM ==========================================================================

set DEMO=retrowave
if not "%1"=="" set DEMO=%1
if "%DEMO%"=="run" set DEMO=retrowave
if "%DEMO%"=="cosmic" set DEMO=cosmic_dashboard
if "%DEMO%"=="inferno" set DEMO=font_inferno

set DEMO_SRC=%DEMO%.c
set WIDGET_SRC=..\widgets\stubs.c ..\widgets\ui_widget.c
set UI_SRC=..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c
set CORE_SRC=..\..\core\component_surface.c ..\..\core\input_system.c
set ALL_SRC=%DEMO_SRC% %WIDGET_SRC% %UI_SRC% %CORE_SRC%
set INC=-I ..\..\..\include -I .. -I ..\widgets -I ..\..\core -I ..\..\..\extras\_stb-truetype
set LIBS=-luser32 -lgdi32 -lopengl32
set OUT=%DEMO%.exe

if "%DEMO%"=="clean" (
    del /q *.exe *.pdb *.obj *.o 2>nul
    echo [CLEAN] Binaries removed
    exit /b 0
)

REM Auto-detect MSVC LIBPATH for clang
set LIBPATH=
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
    for /d %%d in ("C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*") do set MSVC_VER=%%~nxd
    if defined MSVC_VER (
        set LIBPATH=-L "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\!MSVC_VER!\lib\x64"
    )
)
if exist "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0" (
    set LIBPATH=!LIBPATH! -L "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64"
    set LIBPATH=!LIBPATH! -L "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
) else if exist "C:\Program Files (x86)\Windows Kits\10\Lib" (
    for /d %%d in ("C:\Program Files (x86)\Windows Kits\10\Lib\*") do (
        if exist "%%d\ucrt\x64" (
            set LIBPATH=!LIBPATH! -L "%%d\ucrt\x64"
            set LIBPATH=!LIBPATH! -L "%%d\um\x64"
        )
    )
)

echo.
echo ╔═══════════════════════════════════════════════╗
echo ║   RETRO WAVE 2084 — Build: %DEMO%             ║
echo ╚═══════════════════════════════════════════════╝
echo.

REM Try clang first
where clang >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [clang] Compiling %DEMO%...
    clang -std=c11 -g -O0 -Wno-unused-parameter -Wno-unused-function ^
        -D_CRT_SECURE_NO_WARNINGS ^
        %ALL_SRC% %INC% %LIBS% %LIBPATH% -o %OUT%
    if !ERRORLEVEL! EQU 0 (
        echo [clang] SUCCESS — %OUT% created
        if "%1"=="run" (
            echo.
            echo Starting %DEMO%...
            %OUT%
        )
        goto :done
    ) else (
        echo [clang] FAILED — trying MSVC fallback...
    )
)

REM MSVC fallback (requires Developer Command Prompt)
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [msvc] Compiling %DEMO%...
    cl /nologo /O2 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS ^
        %ALL_SRC% ^
        /I ..\..\..\include /I .. /I ..\widgets /I ..\..\core ^
        /link user32.lib gdi32.lib opengl32.lib /OUT:%OUT%
    if !ERRORLEVEL% EQU 0 (
        echo [msvc] SUCCESS — %OUT% created
        if "%1"=="run" (
            echo.
            echo Starting %DEMO%...
            %OUT%
        )
        goto :done
    ) else (
        echo [msvc] FAILED
        exit /b 1
    )
)

echo ERROR: No working compiler found (clang or MSVC cl).
echo Install LLVM:  scoop install llvm
echo.
exit /b 1

:done
echo.
echo ╔═══════════════════════════════════════════════════════════════════╗
echo ║  %OUT%                                       ║
if "%DEMO%"=="font_inferno" (
echo ║  Controls:  SPC=pause  L/R=step  U/D=size        ║
echo ║  C=compare  F=full  H=hud  1-4=lock  ESC=exit  ║
) else (
echo ║  Controls:  G=grid  C=colors  SPACE=anim  ESC=exit  ║
echo ║  Click-drag cassette icons to throw them!  ║
)
echo ╚═══════════════════════════════════════════════╝
echo.
