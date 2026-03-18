#!/usr/bin/env bash
# ABI Parity Conformance Test Runner
#
# This script runs all compiled ABI parity tests for the KAIN native runtime.
# Tests validate that low-level memory helpers behave consistently across backends.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$SCRIPT_DIR/bin"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"

BACKEND="${BACKEND:-native}"
COMPILE_TIMEOUT_SEC="${ABI_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${ABI_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run ABI parity conformance tests with automatic compilation and per-test timeouts.

OPTIONS:
    --backend <name>       Backend label for reporting only
    --compile-timeout <s>  Maximum compile duration (default: 300)
    --test-timeout <s>     Maximum duration for each test (default: 20)
    --verbose              Show test output while running
    --help                 Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend)
            BACKEND="$2"
            shift 2
            ;;
        --compile-timeout)
            COMPILE_TIMEOUT_SEC="$2"
            shift 2
            ;;
        --test-timeout)
            TEST_TIMEOUT_SEC="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

run_with_timeout() {
    local seconds=$1
    local label=$2
    shift 2
    python "$HELPER_SCRIPT" --seconds "$seconds" --label "$label" -- "$@"
}

echo "=== Running KAIN Runtime ABI Parity Tests ==="
echo "Backend: $BACKEND"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Per-test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

echo "Compiling ABI parity tests..."
if ! ABI_COMPILE_TIMEOUT_SEC="$COMPILE_TIMEOUT_SEC" bash "$SCRIPT_DIR/compile_tests.sh"; then
    exit 1
fi

TESTS=(
    "test_pointer_operations"
    "test_load_store_operations"
    "test_union_operations"
    "test_bitfield_operations"
)

PASSED=0
FAILED=0
TOTAL=0
TIMED_OUT=0

resolve_binary_path() {
    local base="$BIN_DIR/$1"
    if [[ -f "$base" ]]; then
        printf "%s" "$base"
        return 0
    fi
    if [[ -f "${base}.exe" ]]; then
        printf "%s" "${base}.exe"
        return 0
    fi
    return 1
}

for test in "${TESTS[@]}"; do
    if ! test_bin="$(resolve_binary_path "$test")"; then
        echo "[FAIL] $test (not compiled)" >&2
        FAILED=$((FAILED + 1))
        TOTAL=$((TOTAL + 1))
        continue
    fi

    TOTAL=$((TOTAL + 1))
    if [[ $VERBOSE -eq 1 ]]; then
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test" "$test_bin"
        test_status=$?
        set -e
    else
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test" "$test_bin" > /dev/null 2>&1
        test_status=$?
        set -e
    fi

    if [[ $test_status -eq 0 ]]; then
        echo "[PASS] $test"
        PASSED=$((PASSED + 1))
    elif [[ $test_status -eq 124 ]]; then
        echo "[TIMEOUT] $test" >&2
        TIMED_OUT=$((TIMED_OUT + 1))
    else
        echo "[FAIL] $test" >&2
        FAILED=$((FAILED + 1))
    fi
done

echo "========================================"
echo "ABI parity summary"
echo "  total:   $TOTAL"
echo "  passed:  $PASSED"
echo "  failed:  $FAILED"
echo "  timeout: $TIMED_OUT"
echo "========================================"

if [[ $FAILED -eq 0 && $TIMED_OUT -eq 0 ]]; then
    echo "All ABI parity tests passed."
    exit 0
fi

exit 1
