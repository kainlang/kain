#!/bin/bash
# Shell script to refresh all UE5 metadata files using the config file
# This script runs all extraction scripts for all configured UE5 versions

echo "============================================================"
echo "KAIN Metadata Refresh - Multi-Version UE5 Support"
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

echo "Step 1: Scanning UE5 installations for engine types..."
echo "============================================================"
python3 ue5_scanner.py --config ue5_paths_config.json
if [ $? -ne 0 ]; then
    echo "ERROR: Engine scanning failed"
    exit 1
fi
echo ""

echo "Step 2: Extracting module dependency graphs..."
echo "============================================================"
python3 module_graph_extractor.py --config ue5_paths_config.json
if [ $? -ne 0 ]; then
    echo "ERROR: Module graph extraction failed"
    exit 1
fi
echo ""

echo "Step 3: Verifying metadata completeness..."
echo "============================================================"
python3 verify_scan.py
if [ $? -ne 0 ]; then
    echo "WARNING: Metadata verification found issues"
fi
echo ""

echo "============================================================"
echo "Metadata refresh complete!"
echo "============================================================"
echo ""
echo "Generated files are in: ../metadata/"
echo ""
echo "Next steps:"
echo "  1. Review the generated files for completeness"
echo "  2. Rebuild the KAIN compiler to use the new metadata"
echo "  3. Test with your plugins"
echo ""
