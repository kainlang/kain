#!/bin/bash
# Validate all native runtime smoke fixtures
# Spec: .kiro/specs/kain-native-runtime-completion
# Task: 0.3 Create native runtime smoke fixtures

set -e

echo "=========================================="
echo "Native Runtime Smoke Fixtures Validation"
echo "=========================================="
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIXTURES_DIR="$SCRIPT_DIR"

# Track results
PASSED=0
FAILED=0
SKIPPED=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to validate a fixture
validate_fixture() {
    local fixture_name=$1
    local fixture_dir="$FIXTURES_DIR/$fixture_name"
    
    echo "----------------------------------------"
    echo "Validating: $fixture_name"
    echo "----------------------------------------"
    
    if [ ! -d "$fixture_dir" ]; then
        echo -e "${RED}FAILED${NC}: Directory not found: $fixture_dir"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    cd "$fixture_dir"
    
    # Check required files exist
    if [ ! -f "main.kn" ]; then
        echo -e "${RED}FAILED${NC}: main.kn not found"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    if [ ! -f "README.md" ]; then
        echo -e "${RED}FAILED${NC}: README.md not found"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    # Platform-specific checks
    if [ "$fixture_name" = "viewport_startup" ]; then
        if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "win32" ]]; then
            echo -e "${YELLOW}SKIPPED${NC}: viewport_startup requires Windows (Win32)"
            SKIPPED=$((SKIPPED + 1))
            return 0
        fi
    fi
    
    # Try to compile (this may fail if kain CLI is not available)
    if command -v kain &> /dev/null; then
        echo "Compiling $fixture_name..."
        if kain build main.kn --target rust 2>&1 | tee /tmp/kain_build_$fixture_name.log; then
            echo -e "${GREEN}PASSED${NC}: $fixture_name compiled successfully"
            PASSED=$((PASSED + 1))
        else
            echo -e "${RED}FAILED${NC}: $fixture_name compilation failed"
            echo "See /tmp/kain_build_$fixture_name.log for details"
            FAILED=$((FAILED + 1))
            return 1
        fi
    else
        echo -e "${YELLOW}SKIPPED${NC}: kain CLI not available, cannot compile"
        SKIPPED=$((SKIPPED + 1))
        return 0
    fi
    
    echo ""
}

# Validate each fixture
validate_fixture "contract_startup"
validate_fixture "realtime_startup"
validate_fixture "ui_startup"
validate_fixture "viewport_startup"

# Summary
echo "=========================================="
echo "Validation Summary"
echo "=========================================="
echo -e "${GREEN}PASSED${NC}:  $PASSED"
echo -e "${RED}FAILED${NC}:  $FAILED"
echo -e "${YELLOW}SKIPPED${NC}: $SKIPPED"
echo ""

if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Validation FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}Validation PASSED${NC}"
    exit 0
fi
