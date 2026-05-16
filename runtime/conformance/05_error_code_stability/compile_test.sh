#!/bin/bash
# Compile and run error code stability tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_ROOT="$SCRIPT_DIR/../.."
NATIVE_DIR="$RUNTIME_ROOT/native"
INCLUDE_DIR="$NATIVE_DIR/include"
SRC_DIR="$NATIVE_DIR/src"

echo "Compiling error code stability tests..."

# Compile the test
gcc -o "$SCRIPT_DIR/test_error_codes" \
    "$SCRIPT_DIR/test_error_codes.c" \
    "$SRC_DIR/core/diagnostics.c" \
    "$SRC_DIR/core/version.c" \
    -I"$INCLUDE_DIR" \
    -std=c99 \
    -Wall -Wextra

echo "Running error code stability tests..."
"$SCRIPT_DIR/test_error_codes"

echo "Error code stability tests completed successfully"
