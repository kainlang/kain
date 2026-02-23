@echo off
REM UE5 Documentation Extraction - Dry Run
REM Counts HTML files without processing

cd /d M:\Code\Kain\unreal\doc_extractor

echo ========================================
echo UE5 Documentation Extraction - Dry Run
echo ========================================
echo.

C:\Users\Admin\AppData\Local\Programs\Python\Python311\python.exe extract_ue5_docs.py ^
  --input M:/Code/Research/OfficialDocs/BlueprintAPI ^
  --output ../extracted_docs ^
  --dry-run

echo.
echo ========================================
echo Dry run complete!
echo ========================================
pause
