#!/usr/bin/env bash
# Platform Parity Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"
RUNTIME_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
NATIVE_SRC="$RUNTIME_DIR/native/src"
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"

PLATFORM_FILTER="${PLATFORM_FILTER:-all}"
COMPILE_TIMEOUT_SEC="${PLATFORM_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${PLATFORM_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run platform parity conformance tests with hard per-step timeouts.

OPTIONS:
    --platform <name>     Platform filter: all, current, stubs
    --compile-timeout <s>  Maximum compile duration (default: 300)
    --test-timeout <s>     Maximum duration for each test (default: 20)
    --verbose             Show test output while running
    --help                Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend)
            if [[ "$2" == "all" ]]; then
                PLATFORM_FILTER="all"
            else
                PLATFORM_FILTER="current"
            fi
            shift 2
            ;;
        --platform)
            PLATFORM_FILTER="$2"
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

if [[ ! -f "$HELPER_SCRIPT" ]]; then
    echo "Timeout helper not found: $HELPER_SCRIPT" >&2
    exit 1
fi

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

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    LDFLAGS="-lws2_32 -luser32 -lgdi32"
    SECURE_CRT_DEFINE="-D_CRT_SECURE_NO_WARNINGS"
else
    LDFLAGS="-lpthread -lm"
    SECURE_CRT_DEFINE=""
fi

TEST_BINARIES=()
if [[ "$PLATFORM_FILTER" == "all" || "$PLATFORM_FILTER" == "current" ]]; then
    TEST_BINARIES+=("test_platform_descriptor")
fi
if [[ "$PLATFORM_FILTER" == "all" || "$PLATFORM_FILTER" == "stubs" ]]; then
    TEST_BINARIES+=("test_platform_stubs")
fi

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
TIMED_OUT_TESTS=0

run_with_timeout() {
    local seconds=$1
    local label=$2
    shift 2
    python "$HELPER_SCRIPT" --seconds "$seconds" --label "$label" -- "$@"
}

echo "Running platform parity tests"
echo "Platform filter: $PLATFORM_FILTER"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Per-test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

compile_sources=(
    "$NATIVE_SRC/core/kain_runtime_version.c"
    "$NATIVE_SRC/core/kain_runtime_diagnostics.c"
    "$NATIVE_SRC/platform/kain_runtime_platform.c"
)

for binary in "${TEST_BINARIES[@]}"; do
    set +e
    run_with_timeout "$COMPILE_TIMEOUT_SEC" "platform parity compilation" \
        "$C_COMPILER" \
        -I"$NATIVE_INCLUDE" \
        -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L \
        $SECURE_CRT_DEFINE \
        "${compile_sources[@]}" \
        "$SCRIPT_DIR/${binary}.c" \
        -o "$SCRIPT_DIR/$binary" \
        $LDFLAGS
    compile_status=$?
    set -e
    if [[ $compile_status -ne 0 ]]; then
        echo "Compilation failed for $binary" >&2
        popd > /dev/null
        exit $compile_status
    fi
done

for binary in "${TEST_BINARIES[@]}"; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if [[ $VERBOSE -eq 1 ]]; then
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$binary" "$SCRIPT_DIR/$binary"
        test_status=$?
        set -e
        if [[ $test_status -eq 0 ]]; then
            echo "[PASS] $binary"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            if [[ $test_status -eq 124 ]]; then
                echo "[TIMEOUT] $binary" >&2
                TIMED_OUT_TESTS=$((TIMED_OUT_TESTS + 1))
            else
                echo "[FAIL] $binary" >&2
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        fi
    else
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$binary" "$SCRIPT_DIR/$binary" > /dev/null 2>&1
        test_status=$?
        set -e
        if [[ $test_status -eq 0 ]]; then
            echo "[PASS] $binary"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            if [[ $test_status -eq 124 ]]; then
                echo "[TIMEOUT] $binary" >&2
                TIMED_OUT_TESTS=$((TIMED_OUT_TESTS + 1))
            else
                echo "[FAIL] $binary" >&2
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        fi
    fi
done

popd > /dev/null

echo ""
echo "Platform parity summary"
echo "  total:   $TOTAL_TESTS"
echo "  passed:  $PASSED_TESTS"
echo "  failed:  $FAILED_TESTS"
echo "  timeout: $TIMED_OUT_TESTS"

if [[ $FAILED_TESTS -ne 0 || $TIMED_OUT_TESTS -ne 0 ]]; then
    exit 1
fi

exit 0
