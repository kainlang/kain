#!/bin/bash

# Compile and run the version info test

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
OUTPUT_DIR="$RUNTIME_DIR/../generated/conformance_tests"

mkdir -p "$OUTPUT_DIR"

echo "=== Compiling ABI Version Test ==="
echo "Test: test_version_info.c"
echo ""

# Compile the test
clang -o "$OUTPUT_DIR/test_version_info" \
    "$SCRIPT_DIR/test_version_info.c" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_version.c" \
    -I"$RUNTIME_DIR/native/include" \
    -std=c11 -Wall -Wextra

echo "✅ Compilation successful"
echo ""
echo "=== Running Test ==="
echo ""

# Run the test
"$OUTPUT_DIR/test_version_info"

TEST_RESULT=$?

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo "✅ Test passed"
else
    echo "❌ Test failed with exit code $TEST_RESULT"
fi

exit $TEST_RESULT
