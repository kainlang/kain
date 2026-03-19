#!/usr/bin/env bash
# Graphics Runtime Conformance Test Runner

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"

BACKEND="${BACKEND:-all}"
COMPILE_TIMEOUT_SEC="${GRAPHICS_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${GRAPHICS_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Run graphics runtime conformance tests with hard per-step timeouts.

OPTIONS:
    --backend <name>         Backend label to report (default: all)
    --compile-timeout <sec>  Maximum compile duration (default: 300)
    --test-timeout <sec>     Maximum duration for the graphics smoke (default: 20)
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

run_with_timeout() {
    local seconds=$1
    local label=$2
    shift 2
    python "$HELPER_SCRIPT" --seconds "$seconds" --label "$label" -- "$@"
}

echo "Running graphics runtime tests"
echo "Backend: $BACKEND"
echo "Compile timeout: ${COMPILE_TIMEOUT_SEC}s"
echo "Test timeout: ${TEST_TIMEOUT_SEC}s"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

echo "Compiling graphics runtime smoke..."
set +e
run_with_timeout "$COMPILE_TIMEOUT_SEC" "graphics runtime compilation" bash "$SCRIPT_DIR/compile_tests.sh"
compile_status=$?
set -e
if [[ $compile_status -ne 0 ]]; then
    if [[ $compile_status -eq 124 ]]; then
        echo "Graphics runtime compilation timed out." >&2
    else
        echo "Graphics runtime compilation failed." >&2
    fi
    popd > /dev/null
    exit $compile_status
fi

echo ""
echo "Executing graphics runtime smoke..."

if [[ ! -x "$SCRIPT_DIR/bin/graphics_runtime_smoke.exe" ]]; then
    echo "Graphics smoke binary missing: $SCRIPT_DIR/bin/graphics_runtime_smoke.exe" >&2
    popd > /dev/null
    exit 1
fi

if [[ $VERBOSE -eq 1 ]]; then
    set +e
    run_with_timeout "$TEST_TIMEOUT_SEC" "graphics runtime smoke" "$SCRIPT_DIR/bin/graphics_runtime_smoke.exe"
    test_status=$?
    set -e
else
    set +e
    run_with_timeout "$TEST_TIMEOUT_SEC" "graphics runtime smoke" "$SCRIPT_DIR/bin/graphics_runtime_smoke.exe" > /dev/null 2>&1
    test_status=$?
    set -e
fi

if [[ $test_status -eq 0 ]]; then
    echo "[PASS] graphics runtime smoke"
else
    if [[ $test_status -eq 124 ]]; then
        echo "[TIMEOUT] graphics runtime smoke" >&2
    else
        echo "[FAIL] graphics runtime smoke" >&2
    fi
    popd > /dev/null
    exit $test_status
fi

echo ""
echo "Executing graphics binding rules..."

if [[ ! -x "$SCRIPT_DIR/bin/graphics_runtime_binding_rules.exe" ]]; then
    echo "Graphics binding rules binary missing: $SCRIPT_DIR/bin/graphics_runtime_binding_rules.exe" >&2
    popd > /dev/null
    exit 1
fi

if [[ $VERBOSE -eq 1 ]]; then
    set +e
    run_with_timeout "$TEST_TIMEOUT_SEC" "graphics binding rules" "$SCRIPT_DIR/bin/graphics_runtime_binding_rules.exe"
    rules_status=$?
    set -e
else
    set +e
    run_with_timeout "$TEST_TIMEOUT_SEC" "graphics binding rules" "$SCRIPT_DIR/bin/graphics_runtime_binding_rules.exe" > /dev/null 2>&1
    rules_status=$?
    set -e
fi

if [[ $rules_status -eq 0 ]]; then
    echo "[PASS] graphics binding rules"
else
    if [[ $rules_status -eq 124 ]]; then
        echo "[TIMEOUT] graphics binding rules" >&2
    else
        echo "[FAIL] graphics binding rules" >&2
    fi
    popd > /dev/null
    exit $rules_status
fi

popd > /dev/null

echo ""
echo "Graphics runtime summary"
echo "  total:   2"
echo "  passed:  2"
echo "  failed:  0"
echo "  timeout: 0"

exit 0
