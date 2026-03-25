#!/usr/bin/env bash
# Run a single conformance test across backends

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
TEST_FILE=""
BACKEND="${BACKEND:-all}"
VERBOSE="${VERBOSE:-0}"

# Usage information
usage() {
    cat << EOF
Usage: $0 <test_file> [OPTIONS]

Run a single conformance test.

ARGUMENTS:
    test_file           Path to test file (relative to conformance directory)

OPTIONS:
    --backend <name>    Run test on specific backend (llvm, cpp, native, interpreter, all)
    --verbose           Enable verbose output
    --help              Show this help message

EXAMPLES:
    # Run test on all backends
    $0 abi_parity/test_pointer_field_offset.kn

    # Run test on LLVM backend only
    $0 abi_parity/test_pointer_field_offset.kn --backend llvm

    # Run with verbose output
    $0 abi_parity/test_pointer_field_offset.kn --verbose

EOF
}

# Parse command line arguments
if [[ $# -eq 0 ]]; then
    usage
    exit 1
fi

TEST_FILE="$1"
shift

while [[ $# -gt 0 ]]; do
    case $1 in
        --backend)
            BACKEND="$2"
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

# Validate test file exists
TEST_PATH="$SCRIPT_DIR/$TEST_FILE"
if [[ ! -f "$TEST_PATH" ]]; then
    echo -e "${RED}Error: Test file not found: $TEST_FILE${NC}"
    exit 1
fi

# Print header
echo -e "${BLUE}Running test: $TEST_FILE${NC}"
echo "Backend: $BACKEND"
echo ""

# Run test (placeholder - actual implementation will depend on test infrastructure)
echo -e "${YELLOW}Note: Test execution not yet implemented${NC}"
echo "This script will be extended in future phases to:"
echo "  - Compile the test program for the specified backend(s)"
echo "  - Execute the compiled test"
echo "  - Compare output against expected results"
echo "  - Report pass/fail status"
echo ""

# For now, just validate the test file exists
echo -e "${GREEN}✓${NC} Test file validated: $TEST_FILE"

exit 0
