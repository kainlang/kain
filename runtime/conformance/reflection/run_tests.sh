#!/usr/bin/env bash
# Reflection Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"

BACKEND="${BACKEND:-all}"
COMPILE_TIMEOUT_SEC="${REFLECTION_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${REFLECTION_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run reflection runtime conformance tests with hard per-step timeouts.

OPTIONS:
    --backend <name>         Backend label to report (default: all)
    --compile-timeout <sec>  Maximum compile duration (default: 300)
    --test-timeout <sec>     Maximum duration for each reflection test (default: 20)
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
    "test_reflection_payload_loading"
    "test_reflection_invalid_inputs"
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

echo "Running reflection runtime tests"
echo "Backend: $BACKEND"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Per-test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

echo "Compiling reflection runtime tests..."
set +e
run_with_timeout "$COMPILE_TIMEOUT_SEC" "reflection test compilation" bash "$SCRIPT_DIR/compile_tests.sh"
compile_status=$?
set -e
if [[ $compile_status -ne 0 ]]; then
    if [[ $compile_status -eq 124 ]]; then
        echo "Reflection test compilation timed out." >&2
    else
        echo "Reflection test compilation failed." >&2
    fi
    popd > /dev/null
    exit $compile_status
fi

echo ""
echo "Executing reflection runtime tests..."

for test_name in "${TEST_BINARIES[@]}"; do
    ((TOTAL_TESTS+=1))

    if [[ ! -x "$SCRIPT_DIR/bin/$test_name" && ! -x "$SCRIPT_DIR/bin/$test_name.exe" ]]; then
        echo "[FAIL] $test_name (binary missing)" >&2
        ((FAILED_TESTS+=1))
        continue
    fi

    local_test_path="$SCRIPT_DIR/bin/$test_name"
    if [[ -x "$SCRIPT_DIR/bin/$test_name.exe" ]]; then
        local_test_path="$SCRIPT_DIR/bin/$test_name.exe"
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$local_test_path"
        test_status=$?
        set -e
        if [[ $test_status -eq 0 ]]; then
            echo "[PASS] $test_name"
            ((PASSED_TESTS+=1))
        else
            if [[ $test_status -eq 124 ]]; then
                echo "[TIMEOUT] $test_name" >&2
                ((TIMED_OUT_TESTS+=1))
            else
                echo "[FAIL] $test_name" >&2
                ((FAILED_TESTS+=1))
            fi
        fi
    else
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$local_test_path" > /dev/null 2>&1
        test_status=$?
        set -e
        if [[ $test_status -eq 0 ]]; then
            echo "[PASS] $test_name"
            ((PASSED_TESTS+=1))
        else
            if [[ $test_status -eq 124 ]]; then
                echo "[TIMEOUT] $test_name" >&2
                ((TIMED_OUT_TESTS+=1))
            else
                echo "[FAIL] $test_name" >&2
                ((FAILED_TESTS+=1))
            fi
        fi
    fi
done

popd > /dev/null

echo ""
echo "Reflection runtime summary"
echo "  total:   $TOTAL_TESTS"
echo "  passed:  $PASSED_TESTS"
echo "  failed:  $FAILED_TESTS"
echo "  timeout: $TIMED_OUT_TESTS"

if [[ $FAILED_TESTS -ne 0 || $TIMED_OUT_TESTS -ne 0 ]]; then
    exit 1
fi

exit 0
