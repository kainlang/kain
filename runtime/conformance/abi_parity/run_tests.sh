#!/usr/bin/env bash
# ABI Parity Conformance Test Runner
#
# This script runs all compiled ABI parity tests for the KAIN native runtime.
# Tests validate that low-level memory helpers behave consistently across backends.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$SCRIPT_DIR/bin"
BACKEND="${1:-native}"

# Check if tests are compiled
if [ ! -d "$BIN_DIR" ]; then
    echo "Tests not compiled. Run ./compile_tests.sh first."
    exit 1
fi

echo "=== Running KAIN Runtime ABI Parity Tests ==="
echo "Backend: $BACKEND"
echo ""

# List of tests to run
TESTS=(
    "test_pointer_operations"
    "test_load_store_operations"
    "test_union_operations"
    "test_bitfield_operations"
)

# Run each test
PASSED=0
FAILED=0
TOTAL=0

for test in "${TESTS[@]}"; do
    test_bin="$BIN_DIR/$test"
    
    if [ ! -f "$test_bin" ]; then
        echo "⚠️  SKIP: $test (not compiled)"
        continue
    fi
    
    TOTAL=$((TOTAL + 1))
    echo "Running: $test"
    echo "----------------------------------------"
    
    if "$test_bin"; then
        echo ""
        echo "✅ PASSED: $test"
        PASSED=$((PASSED + 1))
    else
        echo ""
        echo "❌ FAILED: $test"
        FAILED=$((FAILED + 1))
    fi
    
    echo ""
done

echo "========================================"
echo "Test Results: $PASSED/$TOTAL passed"
echo "========================================"

if [ $FAILED -eq 0 ]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "❌ $FAILED test(s) failed"
    exit 1
fi
