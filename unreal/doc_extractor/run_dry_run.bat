@echo off
REM UE5 Documentation Extraction - Dry Run
REM Counts HTML files without processing

setlocal
cd /d "%~dp0"
set "INPUT_ROOT=%~1"
if "%INPUT_ROOT%"=="" set "INPUT_ROOT=%UE_BLUEPRINT_DOC_ROOT%"
if "%INPUT_ROOT%"=="" (
  echo ERROR: pass the Blueprint API input root as argument 1 or set UE_BLUEPRINT_DOC_ROOT.
  exit /b 1
)

echo ========================================
echo UE5 Documentation Extraction - Dry Run
echo ========================================
echo.

py -3 extract_ue5_docs.py ^
  --input "%INPUT_ROOT%" ^
  --output ../extracted_docs ^
  --dry-run

echo.
echo ========================================
echo Dry run complete!
echo ========================================
pause
