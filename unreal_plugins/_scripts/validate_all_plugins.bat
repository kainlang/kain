@echo off
REM Full Regression Suite for All 5 Plugins
REM Phase 6 Task 28: Comprehensive validation

setlocal enabledelayedexpansion

echo ============================================
echo KAIN Plugin Compilation - Full Regression Suite
echo ============================================
echo.

set TOTAL_PLUGINS=5
set PASSED=0
set FAILED=0

REM Array of plugins to test
set PLUGINS=Materialize VoxelForgePro Cinema4DMograph TemporalBlueprint MetaFitter

echo Starting regression suite at %date% %time%
echo.

for %%P in (%PLUGINS%) do (
    echo.
    echo ============================================
    echo Testing: %%P
    echo ============================================
    
    cd /d "%~dp0..\%%P"
    
    if not exist "FULLBUILD.bat" (
        echo ERROR: FULLBUILD.bat not found for %%P
        set /a FAILED+=1
        goto :next_plugin
    )
    
    echo Running FULLBUILD.bat for %%P...
    call FULLBUILD.bat
    
    if !ERRORLEVEL! EQU 0 (
        echo [SUCCESS] %%P compiled successfully
        set /a PASSED+=1
    ) else (
        echo [FAILED] %%P compilation failed with exit code !ERRORLEVEL!
        set /a FAILED+=1
    )
    
    :next_plugin
)

cd /d "%~dp0.."

echo.
echo ============================================
echo Regression Suite Complete
echo ============================================
echo Total Plugins: %TOTAL_PLUGINS%
echo Passed: %PASSED%
echo Failed: %FAILED%
echo.

if %FAILED% EQU 0 (
    echo [SUCCESS] All plugins compiled successfully!
    exit /b 0
) else (
    echo [FAILURE] %FAILED% plugin(s) failed to compile
    exit /b 1
)
