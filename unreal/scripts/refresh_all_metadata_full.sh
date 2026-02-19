#!/bin/bash
# Full metadata refresh script - includes all optional extraction steps
# This script runs ALL extraction scripts for complete metadata coverage

echo "============================================================"
echo "KAIN Full Metadata Refresh - All Extraction Scripts"
echo "============================================================"
echo ""

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python 3 is not installed or not in PATH"
    exit 1
fi

# Check if config file exists
if [ ! -f "ue5_paths_config.json" ]; then
    echo "ERROR: ue5_paths_config.json not found"
    echo "Please create the config file with your UE5 installation paths"
    exit 1
fi

echo "Step 1/7: Scanning UE5 installations for engine types..."
echo "============================================================"
python3 ue5_scanner.py --config ue5_paths_config.json
if [ $? -ne 0 ]; then
    echo "ERROR: Engine scanning failed"
    exit 1
fi
echo "[OK] Engine type scanning complete"
echo ""

echo "Step 2/7: Extracting module dependency graphs..."
echo "============================================================"
python3 module_graph_extractor.py --config ue5_paths_config.json
if [ $? -ne 0 ]; then
    echo "ERROR: Module graph extraction failed"
    exit 1
fi
echo "[OK] Module dependency graphs extracted"
echo ""

echo "Step 3/7: Extracting UHT validation rules..."
echo "============================================================"
# Try to find UHT source in common locations
UHT_PATH=""
for path in "/opt/UnrealEngine/UE_5.7/Engine/Source/Programs/UnrealHeaderTool" \
            "$HOME/UnrealEngine/UE_5.7/Engine/Source/Programs/UnrealHeaderTool" \
            "/usr/local/UnrealEngine/UE_5.7/Engine/Source/Programs/UnrealHeaderTool"; do
    if [ -d "$path" ]; then
        UHT_PATH="$path"
        break
    fi
done

if [ -n "$UHT_PATH" ]; then
    python3 uht_extractor.py "$UHT_PATH"
    if [ $? -ne 0 ]; then
        echo "WARNING: UHT extraction failed, continuing..."
    else
        echo "[OK] UHT validation rules extracted"
    fi
else
    echo "[SKIP] UHT source not found, skipping..."
fi
echo ""

echo "Step 4/7: Extracting shader knowledge..."
echo "============================================================"
# Try to find Shaders directory in common locations
SHADER_PATH=""
for path in "/opt/UnrealEngine/UE_5.7/Engine/Shaders" \
            "$HOME/UnrealEngine/UE_5.7/Engine/Shaders" \
            "/usr/local/UnrealEngine/UE_5.7/Engine/Shaders"; do
    if [ -d "$path" ]; then
        SHADER_PATH="$path"
        break
    fi
done

if [ -n "$SHADER_PATH" ]; then
    python3 shader_extractor.py "$SHADER_PATH"
    if [ $? -ne 0 ]; then
        echo "WARNING: Shader extraction failed, continuing..."
    else
        echo "[OK] Shader knowledge extracted"
    fi
else
    echo "[SKIP] Shaders directory not found, skipping..."
fi
echo ""

echo "Step 5/7: Extracting editor attributes..."
echo "============================================================"
# Try to find Engine Source in common locations
ENGINE_SOURCE=""
for path in "/opt/UnrealEngine/UE_5.7/Engine/Source" \
            "$HOME/UnrealEngine/UE_5.7/Engine/Source" \
            "/usr/local/UnrealEngine/UE_5.7/Engine/Source"; do
    if [ -d "$path" ]; then
        ENGINE_SOURCE="$path"
        break
    fi
done

if [ -n "$ENGINE_SOURCE" ]; then
    python3 editor_attributes_extractor.py "$ENGINE_SOURCE"
    if [ $? -ne 0 ]; then
        echo "WARNING: Editor attributes extraction failed, continuing..."
    else
        echo "[OK] Editor attributes extracted"
    fi
else
    echo "[SKIP] Engine source not found, skipping..."
fi
echo ""

echo "Step 6/7: Extracting virtual function obligations..."
echo "============================================================"
if [ -n "$ENGINE_SOURCE" ]; then
    python3 virtual_obligations_extractor.py "$ENGINE_SOURCE"
    if [ $? -ne 0 ]; then
        echo "WARNING: Virtual obligations extraction failed, continuing..."
    else
        echo "[OK] Virtual function obligations extracted"
    fi
else
    echo "[SKIP] Engine source not found, skipping..."
fi
echo ""

echo "Step 7/7: Verifying metadata completeness..."
echo "============================================================"
python3 verify_scan.py
if [ $? -ne 0 ]; then
    echo "WARNING: Metadata verification found issues"
    echo "Please review the warnings above"
else
    echo "[OK] All metadata verified successfully"
fi
echo ""

echo "============================================================"
echo "Full metadata refresh complete!"
echo "============================================================"
echo ""
echo "Generated files are in: ../metadata/"
echo ""
echo "Summary:"
echo "  - Core metadata: engine types, module graphs"
echo "  - Optional metadata: UHT rules, shader knowledge, editor attributes"
echo ""
echo "Next steps:"
echo "  1. Review the generated files for completeness"
echo "  2. Rebuild the KAIN compiler: cd ../../kain && cargo build --release"
echo "  3. Test with your plugins: cd ../testing/Phase3/SlateTest4 && kain build --ue5"
echo ""
