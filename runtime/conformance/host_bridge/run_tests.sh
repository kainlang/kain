#!/usr/bin/env bash
# Host Bridge Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"
RUNTIME_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
NATIVE_SRC="$RUNTIME_DIR/native/src"
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"

BACKEND="all"
COMPILE_TIMEOUT_SEC="${HOST_BRIDGE_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${HOST_BRIDGE_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run host bridge and foreign runtime conformance tests with hard per-step timeouts.

OPTIONS:
    --backend <name>       Backend filter (accepted for conformance parity)
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

run_with_timeout() {
    local seconds=$1
    local label=$2
    shift 2
    python "$HELPER_SCRIPT" --seconds "$seconds" --label "$label" -- "$@"
}

TEST_BINARIES=(
    "test_host_bridge_registration"
    "test_host_bridge_failures"
)

COMMON_SOURCES=(
    "$NATIVE_SRC/core/core.c"
    "$NATIVE_SRC/core/version.c"
    "$NATIVE_SRC/core/diagnostics.c"
    "$NATIVE_SRC/core/services.c"
    "$NATIVE_SRC/core/actor.c"
    "$NATIVE_SRC/core/net_system.c"
    "$NATIVE_SRC/core/process_system.c"
    "$NATIVE_SRC/core/host_bridge.c"
)

COMMON_LDFLAGS=()
if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* || "${OSTYPE:-}" == win32* ]]; then
    COMMON_LDFLAGS=(-lws2_32 -lwinhttp)
else
    COMMON_LDFLAGS=(-lpthread -lm)
fi

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
TIMED_OUT_TESTS=0

echo "Running host bridge tests"
echo "Backend filter: $BACKEND"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Per-test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

for binary in "${TEST_BINARIES[@]}"; do
    set +e
    run_with_timeout "$COMPILE_TIMEOUT_SEC" "host bridge compilation" \
        "$C_COMPILER" \
        -I"$NATIVE_INCLUDE" \
        -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L \
        "${COMMON_SOURCES[@]}" \
        "$SCRIPT_DIR/${binary}.c" \
        -o "$SCRIPT_DIR/$binary" \
        "${COMMON_LDFLAGS[@]}"
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
    else
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$binary" "$SCRIPT_DIR/$binary" > /dev/null 2>&1
        test_status=$?
        set -e
    fi

    if [[ $test_status -eq 0 ]]; then
        echo "[PASS] $binary"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    elif [[ $test_status -eq 124 ]]; then
        echo "[TIMEOUT] $binary" >&2
        TIMED_OUT_TESTS=$((TIMED_OUT_TESTS + 1))
    else
        echo "[FAIL] $binary" >&2
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
done

popd > /dev/null

echo ""
echo "Host bridge summary"
echo "  total:   $TOTAL_TESTS"
echo "  passed:  $PASSED_TESTS"
echo "  failed:  $FAILED_TESTS"
echo "  timeout: $TIMED_OUT_TESTS"

if [[ $FAILED_TESTS -ne 0 || $TIMED_OUT_TESTS -ne 0 ]]; then
    exit 1
fi

exit 0
