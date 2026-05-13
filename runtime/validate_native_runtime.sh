#!/usr/bin/env bash
# Aggregate native runtime validation entrypoint.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RELEASE=0
VERBOSE=0
SKIP_CLI_BUILD=0
SKIP_RUNTIME_BUILD=0
SKIP_FIXTURES=0
SKIP_CONFORMANCE=0

usage() {
    cat <<'EOF'
Usage: ./runtime/validate_native_runtime.sh [OPTIONS]

Run the aggregate native runtime validation lane.

Options:
  --release              Build the standalone runtime bundle in release mode
  --verbose              Forward verbose output to compile/conformance scripts
  --skip-cli-build       Skip `cargo build -p cli`
  --skip-runtime-build   Skip `./runtime/compile_native_runtime.sh`
  --skip-fixtures        Skip `./runtime/fixtures/validate_all.sh`
  --skip-conformance     Skip `./runtime/conformance/run_all.sh`
  --help                 Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release)
            RELEASE=1
            shift
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --skip-cli-build)
            SKIP_CLI_BUILD=1
            shift
            ;;
        --skip-runtime-build)
            SKIP_RUNTIME_BUILD=1
            shift
            ;;
        --skip-fixtures)
            SKIP_FIXTURES=1
            shift
            ;;
        --skip-conformance)
            SKIP_CONFORMANCE=1
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done

run_step() {
    local label=$1
    shift
    echo "==> $label"
    "$@"
    echo
}

compile_args=()
if [[ $RELEASE -eq 1 ]]; then
    compile_args+=(--release)
fi
if [[ $VERBOSE -eq 1 ]]; then
    compile_args+=(--verbose)
fi

conformance_args=()
if [[ $VERBOSE -eq 1 ]]; then
    conformance_args+=(--verbose)
fi

if [[ $SKIP_CLI_BUILD -eq 0 ]]; then
    run_step "Building CLI compiler host" cargo build -p cli
fi

if [[ $SKIP_RUNTIME_BUILD -eq 0 ]]; then
    run_step "Compiling manifest-driven native runtime bundle" bash "$SCRIPT_DIR/compile_native_runtime.sh" "${compile_args[@]}"
fi

if [[ $SKIP_FIXTURES -eq 0 ]]; then
    run_step "Running native fixture suite" bash "$SCRIPT_DIR/fixtures/validate_all.sh"
fi

if [[ $SKIP_CONFORMANCE -eq 0 ]]; then
    run_step "Running native conformance suite" bash "$SCRIPT_DIR/conformance/run_all.sh" "${conformance_args[@]}"
fi

if [[ $SKIP_CLI_BUILD -eq 1 && $SKIP_RUNTIME_BUILD -eq 1 && $SKIP_FIXTURES -eq 1 && $SKIP_CONFORMANCE -eq 1 ]]; then
    echo "No validation steps selected."
    exit 0
fi

echo "Native runtime validation completed successfully."
