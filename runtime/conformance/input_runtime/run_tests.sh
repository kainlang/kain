#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_SCRIPT="$(cd "$SCRIPT_DIR/.." && pwd)/_shared/run_with_timeout.py"
COMPILE_TIMEOUT_SEC="${INPUT_COMPILE_TIMEOUT_SEC:-300}"
TEST_TIMEOUT_SEC="${INPUT_TEST_TIMEOUT_SEC:-20}"
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose)
            VERBOSE=1
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
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

echo "Running native input runtime tests"
run_with_timeout "$COMPILE_TIMEOUT_SEC" "input runtime compilation" bash "$SCRIPT_DIR/compile_tests.sh"

if [[ $VERBOSE -eq 1 ]]; then
    run_with_timeout "$TEST_TIMEOUT_SEC" "native input system kernel" "$SCRIPT_DIR/bin/native_input_system_kernel.exe"
else
    run_with_timeout "$TEST_TIMEOUT_SEC" "native input system kernel" "$SCRIPT_DIR/bin/native_input_system_kernel.exe" > /dev/null 2>&1
fi

echo "[PASS] native input system kernel"
