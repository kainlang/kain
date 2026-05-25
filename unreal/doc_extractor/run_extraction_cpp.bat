@echo off
REM UE5 Documentation Extraction - C++ API
REM Full extraction with 16 parallel workers

setlocal
cd /d "%~dp0"
set "INPUT_ROOT=%~1"
if "%INPUT_ROOT%"=="" set "INPUT_ROOT=%UE_CPP_DOC_ROOT%"
if "%INPUT_ROOT%"=="" (
  echo ERROR: pass the UE C++ API input root as argument 1 or set UE_CPP_DOC_ROOT.
  exit /b 1
)

echo ========================================
echo UE5 Documentation Extraction
echo C++ API - Full Extraction
echo ========================================
echo.
echo This will process C++ API HTML files
echo Estimated time: 5-10 minutes
echo.

py -3 extract_ue5_docs.py ^
  --input "%INPUT_ROOT%" ^
  --output ../extracted_docs ^
  --workers 16 ^
  --api cpp

echo.
echo ========================================
echo Extraction complete!
echo ========================================
echo.
echo Output directory: %~dp0..\extracted_docs
echo.
echo Next steps:
echo   1. Review: extracted_docs/cpp_api_index.json
echo   2. Check types: extracted_docs/types/actors.json
echo   3. Merge into KAIN Oracle: extracted_docs/metadata/engine_knowledge_expansion_cpp.json
echo.
pause
