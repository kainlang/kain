#!/usr/bin/env bash
# Async Runtime Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"
RUNTIME_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
NATIVE_DIR="$RUNTIME_DIR/native"
NATIVE_INCLUDE="$NATIVE_DIR/include"
NATIVE_SRC="$NATIVE_DIR/src"
OUT_DIR="$SCRIPT_DIR/bin"

BACKEND="${BACKEND:-all}"
COMPILE_TIMEOUT_SEC="${ASYNC_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${ASYNC_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run async runtime conformance tests with hard per-step timeouts.

OPTIONS:
    --backend <name>         Backend label to report (default: all)
    --compile-timeout <sec>  Maximum compile duration (default: 300)
    --test-timeout <sec>     Maximum duration for each async test (default: 20)
    --verbose                Show test output while running
    --help                   Show this help message
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

COMMON_CFLAGS=(
    -Wall
    -Wextra
    -std=c11
    -I"$NATIVE_INCLUDE"
)

COMMON_SOURCES=(
    "$NATIVE_SRC/core/async.c"
    "$NATIVE_SRC/core/diagnostics.c"
    "$NATIVE_SRC/core/version.c"
)

if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* || "${OSTYPE:-}" == win32* ]]; then
    PLATFORM_CFLAGS=(-D_CRT_SECURE_NO_WARNINGS)
    PLATFORM_LDFLAGS=(-lws2_32 -luser32 -lgdi32)
else
    PLATFORM_CFLAGS=(-D_POSIX_C_SOURCE=200809L)
    PLATFORM_LDFLAGS=(-pthread -lm)
fi

TEST_BINARIES=(
    "test_task_spawn_basic"
    "test_task_wake_poll"
    "test_timer_cancel"
    "test_task_cancel"
    "test_async_sleep"
)

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

compile_test() {
    local test_name=$1
    local source_file="$SCRIPT_DIR/${test_name}.c"
    local output_file="$OUT_DIR/$test_name"

    if [[ ! -f "$source_file" ]]; then
        echo "[SKIP] $test_name (source file missing)" >&2
        return 1
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        run_with_timeout "$COMPILE_TIMEOUT_SEC" "$test_name compilation" \
            "$C_COMPILER" \
            "${COMMON_CFLAGS[@]}" \
            "${PLATFORM_CFLAGS[@]}" \
            "$source_file" \
            "${COMMON_SOURCES[@]}" \
            -o "$output_file" \
            "${PLATFORM_LDFLAGS[@]}"
    else
        run_with_timeout "$COMPILE_TIMEOUT_SEC" "$test_name compilation" \
            "$C_COMPILER" \
            "${COMMON_CFLAGS[@]}" \
            "${PLATFORM_CFLAGS[@]}" \
            "$source_file" \
            "${COMMON_SOURCES[@]}" \
            -o "$output_file" \
            "${PLATFORM_LDFLAGS[@]}" \
            > /dev/null 2>&1
    fi
}

echo "Running async runtime tests"
echo "Backend: $BACKEND"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Per-test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

mkdir -p "$OUT_DIR"
pushd "$SCRIPT_DIR" > /dev/null

for test_name in "${TEST_BINARIES[@]}"; do
    echo "Compiling $test_name..."
    if compile_test "$test_name"; then
        :
    else
        compile_status=$?
        if [[ $compile_status -eq 124 ]]; then
            echo "[TIMEOUT] $test_name compilation" >&2
        else
            echo "[FAIL] $test_name compilation" >&2
        fi
        popd > /dev/null
        exit 1
    fi
done

echo ""
echo "Executing async runtime tests..."

for test_name in "${TEST_BINARIES[@]}"; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [[ ! -x "$OUT_DIR/$test_name" ]]; then
        echo "[FAIL] $test_name (binary missing)" >&2
        FAILED_TESTS=$((FAILED_TESTS + 1))
        continue
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$OUT_DIR/$test_name"
        test_status=$?
        set -e
    else
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$OUT_DIR/$test_name" > /dev/null 2>&1
        test_status=$?
        set -e
    fi

    if [[ $test_status -eq 0 ]]; then
        echo "[PASS] $test_name"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    elif [[ $test_status -eq 124 ]]; then
        echo "[TIMEOUT] $test_name" >&2
        TIMED_OUT_TESTS=$((TIMED_OUT_TESTS + 1))
    else
        echo "[FAIL] $test_name" >&2
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
done

popd > /dev/null

echo ""
echo "Async runtime summary"
echo "  total:   $TOTAL_TESTS"
echo "  passed:  $PASSED_TESTS"
echo "  failed:  $FAILED_TESTS"
echo "  timeout: $TIMED_OUT_TESTS"

if [[ $FAILED_TESTS -ne 0 || $TIMED_OUT_TESTS -ne 0 ]]; then
    exit 1
fi

exit 0
