@echo off
REM UE5 Documentation Extraction - Blueprint API
REM Full extraction with 16 parallel workers

cd /d M:\Code\Kain\unreal\doc_extractor

echo ========================================
echo UE5 Documentation Extraction
echo Blueprint API - Full Extraction
echo ========================================
echo.
echo This will process 150,000+ HTML files
echo Estimated time: 2-3 minutes
echo.

C:\Users\Admin\AppData\Local\Programs\Python\Python311\python.exe extract_ue5_docs.py ^
  --input M:/Code/Research/OfficialDocs/BlueprintAPI ^
  --output ../extracted_docs ^
  --workers 16 ^
  --api blueprint

echo.
echo ========================================
echo Extraction complete!
echo ========================================
echo.
echo Output directory: M:\Code\Kain\unreal\extracted_docs
echo.
echo Next steps:
echo   1. Review: extracted_docs/blueprint_api_index.json
echo   2. Check types: extracted_docs/types/actors.json
echo   3. Merge into KAIN Oracle: extracted_docs/metadata/engine_knowledge_expansion_blueprint.json
echo.
pause
