@echo off
REM Full metadata refresh script - includes all optional extraction steps
REM This script runs ALL extraction scripts for complete metadata coverage

echo ============================================================
echo KAIN Full Metadata Refresh - All Extraction Scripts
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

echo Step 1/7: Scanning UE5 installations for engine types...
echo ============================================================
python ue5_scanner.py --config ue5_paths_config.json
if errorlevel 1 (
    echo ERROR: Engine scanning failed
    exit /b 1
)
echo [OK] Engine type scanning complete
echo.

echo Step 2/7: Extracting module dependency graphs...
echo ============================================================
python module_graph_extractor.py --config ue5_paths_config.json
if errorlevel 1 (
    echo ERROR: Module graph extraction failed
    exit /b 1
)
echo [OK] Module dependency graphs extracted
echo.

echo Step 3/7: Extracting UHT validation rules...
echo ============================================================
REM Try to find UHT source in common locations
set UHT_PATH=
for %%P in ("C:\Program Files\Epic Games\UE_5.7\Engine\Source\Programs\UnrealHeaderTool" "D:\UE_5.7\Engine\Source\Programs\UnrealHeaderTool" "M:\UnrealEngine\UE_5.7\Engine\Source\Programs\UnrealHeaderTool") do (
    if exist %%P (
        set UHT_PATH=%%P
        goto :found_uht
    )
)
:found_uht
if defined UHT_PATH (
    python uht_extractor.py %UHT_PATH%
    if errorlevel 1 (
        echo WARNING: UHT extraction failed, continuing...
    ) else (
        echo [OK] UHT validation rules extracted
    )
) else (
    echo [SKIP] UHT source not found, skipping...
)
echo.

echo Step 4/7: Extracting shader knowledge...
echo ============================================================
REM Try to find Shaders directory in common locations
set SHADER_PATH=
for %%P in ("C:\Program Files\Epic Games\UE_5.7\Engine\Shaders" "D:\UE_5.7\Engine\Shaders" "M:\UnrealEngine\UE_5.7\Engine\Shaders") do (
    if exist %%P (
        set SHADER_PATH=%%P
        goto :found_shaders
    )
)
:found_shaders
if defined SHADER_PATH (
    python shader_extractor.py %SHADER_PATH%
    if errorlevel 1 (
        echo WARNING: Shader extraction failed, continuing...
    ) else (
        echo [OK] Shader knowledge extracted
    )
) else (
    echo [SKIP] Shaders directory not found, skipping...
)
echo.

echo Step 5/7: Extracting editor attributes...
echo ============================================================
REM Try to find Engine Source in common locations
set ENGINE_SOURCE=
for %%P in ("C:\Program Files\Epic Games\UE_5.7\Engine\Source" "D:\UE_5.7\Engine\Source" "M:\UnrealEngine\UE_5.7\Engine\Source") do (
    if exist %%P (
        set ENGINE_SOURCE=%%P
        goto :found_engine
    )
)
:found_engine
if defined ENGINE_SOURCE (
    python editor_attributes_extractor.py %ENGINE_SOURCE%
    if errorlevel 1 (
        echo WARNING: Editor attributes extraction failed, continuing...
    ) else (
        echo [OK] Editor attributes extracted
    )
) else (
    echo [SKIP] Engine source not found, skipping...
)
echo.

echo Step 6/7: Extracting virtual function obligations...
echo ============================================================
if defined ENGINE_SOURCE (
    python virtual_obligations_extractor.py %ENGINE_SOURCE%
    if errorlevel 1 (
        echo WARNING: Virtual obligations extraction failed, continuing...
    ) else (
        echo [OK] Virtual function obligations extracted
    )
) else (
    echo [SKIP] Engine source not found, skipping...
)
echo.

echo Step 7/7: Verifying metadata completeness...
echo ============================================================
python verify_scan.py
if errorlevel 1 (
    echo WARNING: Metadata verification found issues
    echo Please review the warnings above
) else (
    echo [OK] All metadata verified successfully
)
echo.

echo ============================================================
echo Full metadata refresh complete!
echo ============================================================
echo.
echo Generated files are in: ../metadata/
echo.
echo Summary:
echo   - Core metadata: engine types, module graphs
echo   - Optional metadata: UHT rules, shader knowledge, editor attributes
echo.
echo Next steps:
echo   1. Review the generated files for completeness
echo   2. Rebuild the KAIN compiler: cd ../../kain ^&^& cargo build --release
echo   3. Test with your plugins: cd ../testing/Phase3/SlateTest4 ^&^& kain build --ue5
echo.

pause
