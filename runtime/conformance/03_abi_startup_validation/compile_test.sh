#!/bin/bash

# KAIN Runtime ABI and Startup Validation Test Compilation Script
# Task 1.6: Add ABI and startup validation tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
OUTPUT_DIR="$RUNTIME_DIR/../generated/conformance/03_abi_startup_validation"

mkdir -p "$OUTPUT_DIR"

echo "=== Compiling KAIN Runtime ABI and Startup Validation Test ==="
echo ""

# Compile runtime sources
echo "Compiling runtime sources..."

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_version.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_version.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_diagnostics.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_diagnostics.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_services.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_services.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_contract.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_contract.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_win32_shared.o" \
    "$RUNTIME_DIR/native/src/platform/win32/kain_runtime_win32_shared.c"

# Compile test
echo "Compiling test..."

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/test_abi_startup_validation.o" \
    "$SCRIPT_DIR/test_abi_startup_validation.c"

# Link test executable
echo "Linking test executable..."

clang \
    -o "$OUTPUT_DIR/test_abi_startup_validation" \
    "$OUTPUT_DIR/test_abi_startup_validation.o" \
    "$OUTPUT_DIR/kain_runtime_version.o" \
    "$OUTPUT_DIR/kain_runtime_diagnostics.o" \
    "$OUTPUT_DIR/kain_runtime_services.o" \
    "$OUTPUT_DIR/kain_runtime_contract.o" \
    "$OUTPUT_DIR/kain_runtime_win32_shared.o" \
    -lopengl32

echo ""
echo "✅ Compilation successful!"
echo "Output: $OUTPUT_DIR/test_abi_startup_validation"
echo ""

# Run the test
echo "=== Running Test ==="
echo ""
"$OUTPUT_DIR/test_abi_startup_validation"

TEST_RESULT=$?

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo "✅ All tests passed"
else
    echo "❌ Tests failed with exit code $TEST_RESULT"
fi

exit $TEST_RESULT
