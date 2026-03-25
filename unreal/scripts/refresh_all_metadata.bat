@echo off
REM Batch script to refresh all UE5 metadata files using the config file
REM This script runs all extraction scripts for all configured UE5 versions

echo ============================================================
echo KAIN Metadata Refresh - Multi-Version UE5 Support
echo ============================================================
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    exit /b 1
)

REM Check if config file exists
if not exist "ue5_paths_config.json" (
    echo ERROR: ue5_paths_config.json not found
    echo Please create the config file with your UE5 installation paths
    exit /b 1
)

echo Step 1: Scanning UE5 installations for engine types...
echo ============================================================
python ue5_scanner.py --config ue5_paths_config.json
if errorlevel 1 (
    echo ERROR: Engine scanning failed
    exit /b 1
)
echo.

echo Step 2: Extracting module dependency graphs...
echo ============================================================
python module_graph_extractor.py --config ue5_paths_config.json
if errorlevel 1 (
    echo ERROR: Module graph extraction failed
    exit /b 1
)
echo.

echo Step 3: Verifying metadata completeness...
echo ============================================================
python verify_scan.py
if errorlevel 1 (
    echo WARNING: Metadata verification found issues
)
echo.

echo ============================================================
echo Metadata refresh complete!
echo ============================================================
echo.
echo Generated files are in: ../metadata/
echo.
echo Next steps:
echo   1. Review the generated files for completeness
echo   2. Rebuild the KAIN compiler to use the new metadata
echo   3. Test with your plugins
echo.

pause
