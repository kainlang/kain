#!/usr/bin/env bash
# Native Runtime Conformance Test Runner
# Runs all conformance tests across all categories and backends

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$(dirname "$SCRIPT_DIR")"
WORKSPACE_ROOT="$(dirname "$RUNTIME_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
BACKEND="${BACKEND:-all}"
MODE="${MODE:-full}"
VERBOSE="${VERBOSE:-0}"

# Test categories
CATEGORIES=(
    "abi_parity"
    "actor_runtime"
    "async_runtime"
    "reflection"
    "diagnostics"
    "ui_runtime"
    "graphics_runtime"
    "hot_reload"
    "platform_parity"
)

# Test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Run native runtime conformance tests.

OPTIONS:
    --backend <name>    Run tests on specific backend (llvm, cpp, native, interpreter, all)
    --mode <mode>       Test mode: quick, full, regression (default: full)
    --category <name>   Run only specific category
    --verbose           Enable verbose output
    --help              Show this help message

EXAMPLES:
    # Run all tests on all backends
    $0

    # Run all tests on LLVM backend only
    $0 --backend llvm

    # Run quick validation (smoke tests only)
    $0 --mode quick

    # Run specific category
    $0 --category abi_parity

    # Run with verbose output
    $0 --verbose

EOF
}

# Parse command line arguments
CATEGORY_FILTER=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --backend)
            BACKEND="$2"
            shift 2
            ;;
        --mode)
            MODE="$2"
            shift 2
            ;;
        --category)
            CATEGORY_FILTER="$2"
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
            echo -e "${RED}Error: Unknown option $1${NC}"
            usage
            exit 1
            ;;
    esac
done

# Print header
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Native Runtime Conformance Test Suite${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "Backend: $BACKEND"
    echo "Mode: $MODE"
    echo "Workspace: $WORKSPACE_ROOT"
    echo ""
}

# Print category header
print_category() {
    local category=$1
    echo ""
    echo -e "${BLUE}--- Running $category tests ---${NC}"
}

# Print test result
print_result() {
    local status=$1
    local message=$2
    
    case $status in
        PASS)
            echo -e "${GREEN}✓${NC} $message"
            ((PASSED_TESTS++))
            ;;
        FAIL)
            echo -e "${RED}✗${NC} $message"
            ((FAILED_TESTS++))
            ;;
        SKIP)
            echo -e "${YELLOW}○${NC} $message"
            ((SKIPPED_TESTS++))
            ;;
    esac
    ((TOTAL_TESTS++))
}

# Run tests for a category
run_category_tests() {
    local category=$1
    local category_dir="$SCRIPT_DIR/$category"
    
    if [[ ! -d "$category_dir" ]]; then
        print_result SKIP "$category (directory not found)"
        return
    fi
    
    local test_runner="$category_dir/run_tests.sh"
    if [[ ! -f "$test_runner" ]]; then
        print_result SKIP "$category (no test runner found)"
        return
    fi
    
    print_category "$category"
    
    # Run the category test runner
    if [[ $VERBOSE -eq 1 ]]; then
        if bash "$test_runner" --backend "$BACKEND"; then
            print_result PASS "$category"
        else
            print_result FAIL "$category"
        fi
    else
        if bash "$test_runner" --backend "$BACKEND" > /dev/null 2>&1; then
            print_result PASS "$category"
        else
            print_result FAIL "$category"
        fi
    fi
}

# Print summary
print_summary() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Test Summary${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "Total tests: $TOTAL_TESTS"
    echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
    echo -e "${YELLOW}Skipped: $SKIPPED_TESTS${NC}"
    echo ""
    
    if [[ $FAILED_TESTS -eq 0 ]]; then
        echo -e "${GREEN}All tests passed!${NC}"
        return 0
    else
        echo -e "${RED}Some tests failed.${NC}"
        return 1
    fi
}

# Main execution
main() {
    print_header
    
    # Determine which categories to run
    local categories_to_run=()
    if [[ -n "$CATEGORY_FILTER" ]]; then
        categories_to_run=("$CATEGORY_FILTER")
    else
        categories_to_run=("${CATEGORIES[@]}")
    fi
    
    # Run tests for each category
    for category in "${categories_to_run[@]}"; do
        run_category_tests "$category"
    done
    
    # Print summary and exit
    print_summary
}

# Run main
main
