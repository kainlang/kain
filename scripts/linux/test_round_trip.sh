#!/bin/bash
# Test Round-Trip Compilation System
# 
# This script demonstrates the full round-trip workflow:
# 1. Compile KAIN with markers
# 2. Extract KAIN from C++
# 3. Validate round-trip

set -e  # Exit on error

repo_root="$(git rev-parse --show-toplevel)"

echo "🔄 KAIN Round-Trip Compiler Test"
echo "================================"
echo ""

# Configuration
TEST_PLUGIN="${repo_root}/unreal_plugins/VoxelForgePro/VoxelForgePro"
KAIN_BINARY="kain"
OUTPUT_DIR="${repo_root}/generated/round_trip_test"

# Check if kain binary exists
if ! command -v $KAIN_BINARY &> /dev/null; then
    echo "❌ kain binary not found in PATH"
    echo "   Build it with: cargo build --release --package cli"
    echo "   Then copy to PATH or set KAIN_BINARY environment variable"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "📁 Test Plugin: $TEST_PLUGIN"
echo "🔧 KAIN Binary: $(which $KAIN_BINARY)"
echo ""

# Step 1: Compile KAIN with markers
echo "Step 1: Compiling KAIN with embedded markers..."
echo "-----------------------------------------------"
cd "$TEST_PLUGIN"

# Note: --embed-kain flag not yet implemented in CLI
# For now, we'll test extraction on existing generated C++
echo "⚠️  Note: --embed-kain flag not yet implemented"
echo "   Testing extraction on existing C++ (without markers)"
echo ""

cd "$repo_root"

# Step 2: Extract KAIN from C++
echo "Step 2: Extracting KAIN from C++..."
echo "------------------------------------"
python3 "$repo_root/scripts/python/cpp_to_kain.py" "$TEST_PLUGIN/Source/" \
    --output "$OUTPUT_DIR/recovered.kn"

if [ $? -eq 0 ]; then
    echo "✅ Extraction succeeded"
    echo "   Output: $OUTPUT_DIR/recovered.kn"
    echo "   Lines: $(wc -l < "$OUTPUT_DIR/recovered.kn")"
else
    echo "❌ Extraction failed"
    exit 1
fi
echo ""

# Step 3: Show extracted KAIN (first 50 lines)
echo "Step 3: Preview extracted KAIN..."
echo "----------------------------------"
head -n 50 "$OUTPUT_DIR/recovered.kn"
echo ""
echo "   (showing first 50 lines, see $OUTPUT_DIR/recovered.kn for full output)"
echo ""

# Step 4: Validate round-trip (if markers exist)
echo "Step 4: Validating round-trip..."
echo "--------------------------------"
python3 "$repo_root/scripts/python/cpp_to_kain.py" "$TEST_PLUGIN/Source/" --validate

if [ $? -eq 0 ]; then
    echo "✅ Round-trip validation succeeded"
else
    echo "⚠️  Round-trip validation failed (expected without markers)"
fi
echo ""

# Summary
echo "================================"
echo "📊 Test Summary"
echo "================================"
echo "✅ Extraction tool works"
echo "⚠️  Markers not yet implemented in codegen"
echo "⚠️  Round-trip validation pending marker support"
echo ""
echo "Next steps:"
echo "1. Implement --embed-kain flag in CLI"
echo "2. Wire kain_markers module into codegen"
echo "3. Re-run this test with markers enabled"
echo ""
echo "See $repo_root/scripts/docs/ROUND_TRIP_README.md for details"
