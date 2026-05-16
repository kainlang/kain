#!/usr/bin/env bash
# Compile reflection conformance tests with a hard timeout-aware harness.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_SRC="$RUNTIME_DIR/native/src"
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"

COMPILE_TIMEOUT_SEC="${REFLECTION_COMPILE_TIMEOUT_SEC:-300}"

if [[ -n "${CC:-}" ]]; then
    C_COMPILER="$CC"
elif command -v clang >/dev/null 2>&1; then
    C_COMPILER="clang"
elif command -v gcc >/dev/null 2>&1; then
    C_COMPILER="gcc"
elif command -v cc >/dev/null 2>&1; then
    C_COMPILER="cc"
else
    echo "No supported C compiler found on PATH. Set CC explicitly." >&2
    exit 1
fi

if [[ "${OSTYPE:-}" == msys || "${OSTYPE:-}" == win32 || "${OSTYPE:-}" == cygwin ]]; then
    LDFLAGS=(-lws2_32 -luser32 -lgdi32)
else
    LDFLAGS=(-lpthread -lm)
fi

CFLAGS=(
    -std=c11
    -Wall
    -Wextra
    -D_POSIX_C_SOURCE=200809L
    -I"$NATIVE_INCLUDE"
    -g
)

run_with_timeout() {
    local seconds=$1
    local label=$2
    shift 2
    python "$HELPER_SCRIPT" --seconds "$seconds" --label "$label" -- "$@"
}

OUT_DIR="$SCRIPT_DIR/bin"
mkdir -p "$OUT_DIR"

OUT_SUFFIX=""
if [[ "${OSTYPE:-}" == msys || "${OSTYPE:-}" == win32 || "${OSTYPE:-}" == cygwin ]]; then
    OUT_SUFFIX=".exe"
fi

echo "=== Compiling Reflection Conformance Tests ==="
echo "Compiler: $C_COMPILER"
echo "Output: $OUT_DIR"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo ""

RUNTIME_SOURCES=(
    "$NATIVE_SRC/core/version.c"
    "$NATIVE_SRC/core/diagnostics.c"
    "$NATIVE_SRC/core/reflection.c"
    "$NATIVE_SRC/core/scene.c"
)

compile_test() {
    local test_name=$1
    local test_file="$SCRIPT_DIR/${test_name}.c"
    local out_file="$OUT_DIR/${test_name}${OUT_SUFFIX}"

    if [[ ! -f "$test_file" ]]; then
        echo "SKIP: $test_name (file not found)"
        return 0
    fi

    echo "Compiling: $test_name"
    if run_with_timeout "$COMPILE_TIMEOUT_SEC" "reflection compilation" \
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
    "test_reflection_payload_loading"
    "test_reflection_invalid_inputs"
)

FAILED=0
for test in "${TESTS[@]}"; do
    if ! compile_test "$test"; then
        FAILED=$((FAILED + 1))
    fi
done

echo ""
if [[ $FAILED -eq 0 ]]; then
    echo "=== All reflection tests compiled successfully ==="
    echo ""
    echo "Run tests with:"
    echo "  ./run_tests.sh"
    exit 0
fi

echo "=== $FAILED reflection test(s) failed to compile ===" >&2
exit 1
