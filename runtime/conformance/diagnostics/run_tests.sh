#!/usr/bin/env bash
# Diagnostics Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"
BACKEND="${BACKEND:-all}"
COMPILE_TIMEOUT_SEC="${DIAGNOSTICS_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${DIAGNOSTICS_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run diagnostics conformance tests with hard per-step timeouts.

OPTIONS:
    --backend <name>         Backend label to report (default: all)
    --compile-timeout <sec>  Maximum compile duration (default: 300)
    --test-timeout <sec>     Maximum duration for each diagnostics test (default: 20)
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

TEST_BINARIES=(
    "test_structured_runtime_diagnostics"
    "test_diagnostic_error_codes"
    "test_startup_failure_reporting"
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

echo "Running diagnostics tests"
echo "Backend: $BACKEND"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Per-test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

echo "Compiling diagnostics tests..."
set +e
if [[ $VERBOSE -eq 1 ]]; then
    run_with_timeout "$COMPILE_TIMEOUT_SEC" "diagnostics test compilation" bash "$SCRIPT_DIR/compile_tests.sh"
else
    run_with_timeout "$COMPILE_TIMEOUT_SEC" "diagnostics test compilation" bash "$SCRIPT_DIR/compile_tests.sh" > /dev/null 2>&1
fi
compile_status=$?
set -e

if [[ $compile_status -ne 0 ]]; then
    if [[ $compile_status -eq 124 ]]; then
        echo "[TIMEOUT] diagnostics test compilation" >&2
    else
        echo "[FAIL] diagnostics test compilation" >&2
    fi
    popd > /dev/null
    exit $compile_status
fi

echo ""
echo "Executing diagnostics tests..."

for test_name in "${TEST_BINARIES[@]}"; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [[ ! -x "$SCRIPT_DIR/bin/$test_name" ]]; then
        echo "[FAIL] $test_name (binary missing)" >&2
        FAILED_TESTS=$((FAILED_TESTS + 1))
        continue
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$SCRIPT_DIR/bin/$test_name"
        test_status=$?
        set -e
    else
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$SCRIPT_DIR/bin/$test_name" > /dev/null 2>&1
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
echo "Diagnostics summary"
echo "  total:   $TOTAL_TESTS"
echo "  passed:  $PASSED_TESTS"
echo "  failed:  $FAILED_TESTS"
echo "  timeout: $TIMED_OUT_TESTS"

if [[ $FAILED_TESTS -ne 0 || $TIMED_OUT_TESTS -ne 0 ]]; then
    exit 1
fi

exit 0
