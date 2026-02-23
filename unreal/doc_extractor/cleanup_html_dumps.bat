@echo off
REM Cleanup Script - Delete HTML dumps after extraction
REM Run this AFTER you've verified the extracted JSON is correct

echo ========================================
echo UE5 Documentation Cleanup
echo ========================================
echo.
echo This will DELETE the following directories:
echo   1. M:\Code\Research\OfficialDocs\BlueprintAPI  (~2-3 GB)
echo   2. M:\Code\Kain\unreal\UE_API                  (~5-10 GB)
echo.
echo Total space to free: ~7-13 GB
echo.
echo IMPORTANT: Make sure you've verified the extracted JSON first!
echo   - Check: M:\Code\Kain\unreal\extracted_docs\blueprint_api_index.json
echo   - Check: M:\Code\Kain\unreal\extracted_docs\metadata\engine_knowledge_expansion_blueprint.json
echo.
pause
echo.

echo Deleting Blueprint API HTML files...
if exist "M:\Code\Research\OfficialDocs\BlueprintAPI" (
    rmdir /s /q "M:\Code\Research\OfficialDocs\BlueprintAPI"
    echo   ✓ Deleted M:\Code\Research\OfficialDocs\BlueprintAPI
) else (
    echo   ⚠ Directory not found: M:\Code\Research\OfficialDocs\BlueprintAPI
)

echo.
echo Deleting C++ API HTML files...
if exist "M:\Code\Kain\unreal\UE_API" (
    rmdir /s /q "M:\Code\Kain\unreal\UE_API"
    echo   ✓ Deleted M:\Code\Kain\unreal\UE_API
) else (
    echo   ⚠ Directory not found: M:\Code\Kain\unreal\UE_API
)

echo.
echo ========================================
echo Cleanup complete!
echo ========================================
echo.
echo Freed up ~7-13 GB of disk space
echo.
echo Extracted JSON files are safe at:
echo   M:\Code\Kain\unreal\extracted_docs\
echo.
pause
