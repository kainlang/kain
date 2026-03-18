#!/bin/bash
# Compile ABI Parity Conformance Tests
#
# This script compiles all ABI parity tests for the KAIN native runtime.
# Tests validate that low-level memory helpers behave consistently across backends.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_DIR="$RUNTIME_DIR/native"

# Compiler settings
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -I$NATIVE_DIR/include -I$RUNTIME_DIR -g"
LDFLAGS="-lm"

# Output directory
OUT_DIR="$SCRIPT_DIR/bin"
mkdir -p "$OUT_DIR"

echo "=== Compiling KAIN Runtime ABI Parity Tests ==="
echo "Compiler: $CC"
echo "Flags: $CFLAGS"
echo "Output: $OUT_DIR"
echo ""

# Compile each test
compile_test() {
    local test_name=$1
    local test_file="$SCRIPT_DIR/${test_name}.c"
    local out_file="$OUT_DIR/${test_name}"
    
    if [ ! -f "$test_file" ]; then
        echo "⚠️  SKIP: $test_name (file not found)"
        return
    fi
    
    echo "Compiling: $test_name"
    if $CC $CFLAGS "$test_file" -o "$out_file" $LDFLAGS 2>&1 | grep -v "warning:"; then
        echo "  ✅ SUCCESS: $out_file"
    else
        echo "  ❌ FAILED: $test_name"
        return 1
    fi
}

# List of tests to compile
TESTS=(
    "test_pointer_operations"
    "test_load_store_operations"
    "test_union_operations"
    "test_bitfield_operations"
)

# Compile all tests
FAILED=0
for test in "${TESTS[@]}"; do
    if ! compile_test "$test"; then
        FAILED=$((FAILED + 1))
    fi
done

echo ""
if [ $FAILED -eq 0 ]; then
    echo "=== All tests compiled successfully ==="
    echo ""
    echo "Run tests with:"
    echo "  ./run_tests.sh"
    exit 0
else
    echo "=== $FAILED test(s) failed to compile ==="
    exit 1
fi
