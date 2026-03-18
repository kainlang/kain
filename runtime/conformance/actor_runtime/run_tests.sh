#!/usr/bin/env bash
# Actor Runtime Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"

BACKEND="${BACKEND:-all}"
COMPILE_TIMEOUT_SEC="${ACTOR_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${ACTOR_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run actor runtime conformance tests with hard per-step timeouts.

OPTIONS:
    --backend <name>         Backend label to report (default: all)
    --compile-timeout <sec>  Maximum compile duration (default: 300)
    --test-timeout <sec>     Maximum duration for each actor test (default: 20)
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
    "test_actor_spawn_basic"
    "test_actor_registry"
    "test_mailbox_backpressure"
    "test_actor_monitors"
    "test_actor_links"
    "test_actor_supervision"
    "test_actor_scheduler"
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

echo "Running actor runtime tests"
echo "Backend: $BACKEND"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Per-test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

echo "Compiling actor runtime tests..."
set +e
run_with_timeout "$COMPILE_TIMEOUT_SEC" "actor test compilation" bash "$SCRIPT_DIR/compile_tests.sh"
compile_status=$?
set -e
if [[ $compile_status -ne 0 ]]; then
    if [[ $compile_status -eq 124 ]]; then
        echo "Actor test compilation timed out." >&2
    else
        echo "Actor test compilation failed." >&2
    fi
    popd > /dev/null
    exit $compile_status
fi

echo ""
echo "Executing actor runtime tests..."

for test_name in "${TEST_BINARIES[@]}"; do
    ((TOTAL_TESTS+=1))

    if [[ ! -x "$SCRIPT_DIR/$test_name" ]]; then
        echo "[FAIL] $test_name (binary missing)" >&2
        ((FAILED_TESTS+=1))
        continue
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        set +e
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$SCRIPT_DIR/$test_name"
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
        run_with_timeout "$TEST_TIMEOUT_SEC" "$test_name" "$SCRIPT_DIR/$test_name" > /dev/null 2>&1
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
echo "Actor runtime summary"
echo "  total:   $TOTAL_TESTS"
echo "  passed:  $PASSED_TESTS"
echo "  failed:  $FAILED_TESTS"
echo "  timeout: $TIMED_OUT_TESTS"

if [[ $FAILED_TESTS -ne 0 || $TIMED_OUT_TESTS -ne 0 ]]; then
    exit 1
fi

exit 0
