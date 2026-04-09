#!/usr/bin/env bash
# KAIN Native Runtime Full Validation Suite
#
# Purpose: Run all canonical validation commands for the native runtime
# Spec: .kiro/specs/kain-native-runtime-completion
# Task: 0.2 Establish native runtime validation commands
#
# Usage:
#   ./runtime/validate_native_runtime.sh [--verbose] [--continue-on-error]
#
# Options:
#   --verbose            Show detailed output from all commands
#   --continue-on-error  Continue running tests even if one fails
#   --help               Show this help message

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default options
VERBOSE=false
CONTINUE_ON_ERROR=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose)
            VERBOSE=true
            shift
            ;;
        --continue-on-error)
            CONTINUE_ON_ERROR=true
            set +e  # Disable exit on error
            shift
            ;;
        --help)
            echo "KAIN Native Runtime Full Validation Suite"
            echo ""
            echo "Usage: $0 [--verbose] [--continue-on-error] [--help]"
            echo ""
            echo "Options:"
            echo "  --verbose            Show detailed output from all commands"
            echo "  --continue-on-error  Continue running tests even if one fails"
            echo "  --help               Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Change to project root
cd "$PROJECT_ROOT"

# Track results
TOTAL_STEPS=4
PASSED_STEPS=0
FAILED_STEPS=0
STEP_RESULTS=()

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     KAIN Native Runtime Full Validation Suite                 ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "Running $TOTAL_STEPS validation steps..."
echo ""

# Helper function to run a validation step
run_step() {
    local step_num=$1
    local step_name=$2
    local step_cmd=$3
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Step $step_num/$TOTAL_STEPS: $step_name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    
    if [[ "$VERBOSE" == true ]]; then
        echo "Command: $step_cmd"
        echo ""
    fi
    
    # Run the command
    local start_time=$(date +%s)
    local exit_code=0
    
    if [[ "$VERBOSE" == true ]]; then
        eval "$step_cmd" || exit_code=$?
    else
        eval "$step_cmd" > /dev/null 2>&1 || exit_code=$?
    fi
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Record result
    if [[ $exit_code -eq 0 ]]; then
        echo "✅ PASSED ($duration seconds)"
        PASSED_STEPS=$((PASSED_STEPS + 1))
        STEP_RESULTS+=("✅ $step_name")
    else
        echo "❌ FAILED (exit code: $exit_code, duration: $duration seconds)"
        FAILED_STEPS=$((FAILED_STEPS + 1))
        STEP_RESULTS+=("❌ $step_name")
        
        if [[ "$CONTINUE_ON_ERROR" == false ]]; then
            echo ""
            echo "Validation failed at step $step_num. Use --continue-on-error to run all steps."
            exit $exit_code
        fi
    fi
    
    echo ""
}

# Step 1: CLI build
run_step 1 "CLI build" "cargo build -p cli"

# Step 2: Native runtime compilation
run_step 2 "Native runtime compilation" "./runtime/compile_native_runtime.sh"

# Step 3: LLVM/raw-native fixtures
run_step 3 "LLVM and raw-native fixtures" "./runtime/fixtures/validate_all.sh"

# Step 4: Native runtime conformance
run_step 4 "Native runtime conformance" "./runtime/conformance/run_all.sh"

# Print summary
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║                    Validation Summary                          ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

for result in "${STEP_RESULTS[@]}"; do
    echo "$result"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Total: $TOTAL_STEPS steps"
echo "Passed: $PASSED_STEPS"
echo "Failed: $FAILED_STEPS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [[ $FAILED_STEPS -eq 0 ]]; then
    echo "🎉 All validation steps passed!"
    echo ""
    echo "The Linux native runtime, LLVM lane, and raw-native conformance surface are validated."
    echo "See runtime/NATIVE_RUNTIME_VALIDATION.md for details."
    exit 0
else
    echo "⚠️  $FAILED_STEPS validation step(s) failed."
    echo ""
    echo "Please fix the failures before proceeding with runtime work."
    echo "See runtime/NATIVE_RUNTIME_VALIDATION.md for troubleshooting."
    exit 1
fi
