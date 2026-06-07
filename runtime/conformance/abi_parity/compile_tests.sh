#!/usr/bin/env bash
# Compile ABI Parity Conformance Tests
#
# This script compiles all ABI parity tests for the KAIN native runtime.
# Tests validate that low-level memory helpers behave consistently across backends.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_DIR="$RUNTIME_DIR/native"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"

COMPILE_TIMEOUT_SEC="${ABI_COMPILE_TIMEOUT_SEC:-300}"

if [[ -n "${CC:-}" ]]; then
    C_COMPILER="$CC"
elif command -v clang > /dev/null 2>&1; then
    C_COMPILER="clang"
elif command -v gcc > /dev/null 2>&1; then
    C_COMPILER="gcc"
elif command -v cc > /dev/null 2>&1; then
    C_COMPILER="cc"
else
    echo "No supported C compiler found on PATH. Set CC explicitly." >&2
    exit 1
fi

SECURE_CRT_DEFINE=""
if [[ "${OSTYPE:-}" == "msys" || "${OSTYPE:-}" == "cygwin" || "${OSTYPE:-}" == "win32" ]]; then
    SECURE_CRT_DEFINE="-D_CRT_SECURE_NO_WARNINGS"
fi

CFLAGS=(
    -std=c11
    -Wall
    -Wextra
    -D_POSIX_C_SOURCE=200809L
    -I"$NATIVE_DIR/include"
    -I"$RUNTIME_DIR"
    -g
)
if [[ -n "$SECURE_CRT_DEFINE" ]]; then
    CFLAGS+=("$SECURE_CRT_DEFINE")
fi

source "/../_shared/runtime_helpers.sh"
RUNTIME_SOURCES=( "${ALL_RUNTIME_SOURCES[@]}" )

LDFLAGS=()

run_with_timeout() {
    local seconds=$1
    local label=$2
    shift 2
    python "$HELPER_SCRIPT" --seconds "$seconds" --label "$label" -- "$@"
}

OUT_DIR="$SCRIPT_DIR/bin"
mkdir -p "$OUT_DIR"

echo "=== Compiling KAIN Runtime ABI Parity Tests ==="
echo "Compiler: $C_COMPILER"
echo "Output: $OUT_DIR"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo ""

compile_test() {
    local test_name=$1
    local test_file="$SCRIPT_DIR/${test_name}.c"
    local out_file="$OUT_DIR/${test_name}"

    if [[ ! -f "$test_file" ]]; then
        echo "SKIP: $test_name (file not found)"
        return 0
    fi

    echo "Compiling: $test_name"
    if run_with_timeout "$COMPILE_TIMEOUT_SEC" "abi parity compilation" \
        "$C_COMPILER" \
        "${CFLAGS[@]}" \
        "${RUNTIME_SOURCES[@]}" \
        "$test_file" \
        -o "$out_file" \
        "${LDFLAGS[@]}"; then
        echo "  PASS: $out_file"
    else
        echo "  FAIL: $test_name" >&2
        return 1
    fi
}

TESTS=(
    "test_pointer_operations"
    "test_load_store_operations"
    "test_union_operations"
    "test_bitfield_operations"
)

FAILED=0
for test in "${TESTS[@]}"; do
    if ! compile_test "$test"; then
        FAILED=$((FAILED + 1))
    fi
done

echo ""
if [[ $FAILED -eq 0 ]]; then
    echo "=== All tests compiled successfully ==="
    echo ""
    echo "Run tests with:"
    echo "  ./run_tests.sh"
    exit 0
fi

echo "=== $FAILED test(s) failed to compile ===" >&2
exit 1
