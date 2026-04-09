#!/usr/bin/env bash
# Validate all native runtime smoke fixtures
# Spec: .kiro/specs/kain-native-runtime-completion
# Task: 0.3 Create native runtime smoke fixtures

set -euo pipefail

echo "=========================================="
echo "Native Runtime Smoke Fixtures Validation"
echo "=========================================="
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIXTURES_DIR="$SCRIPT_DIR"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Track results
PASSED=0
FAILED=0
SKIPPED=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Resolve a usable kain binary from PATH or the local repo target dir.
resolve_kain_bin() {
    if [ -x "$PROJECT_ROOT/target/debug/kain" ]; then
        printf '%s\n' "$PROJECT_ROOT/target/debug/kain"
        return 0
    fi

    if [ -x "$PROJECT_ROOT/target/release/kain" ]; then
        printf '%s\n' "$PROJECT_ROOT/target/release/kain"
        return 0
    fi

    if command -v kain >/dev/null 2>&1; then
        command -v kain
        return 0
    fi

    return 1
}

build_target_for_fixture() {
    case "$1" in
        contract_startup|realtime_startup)
            printf '%s\n' "llvm"
            ;;
        *)
            printf '%s\n' "rust"
            ;;
    esac
}

build_output_for_fixture() {
    local fixture_name=$1
    local target=$2

    mkdir -p generated
    case "$target" in
        llvm)
            printf '%s\n' "generated/${fixture_name}.ll"
            ;;
        rust)
            printf '%s\n' "generated/${fixture_name}.rs"
            ;;
        *)
            printf '%s\n' "generated/${fixture_name}.out"
            ;;
    esac
}

expected_artifact_for_fixture() {
    local fixture_name=$1
    local target=$2

    case "$target" in
        llvm)
            if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "msys2" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
                printf '%s\n' "generated/${fixture_name}.exe"
            else
                printf '%s\n' "generated/${fixture_name}"
            fi
            ;;
        rust)
            printf '%s\n' "generated/${fixture_name}.rs"
            ;;
        *)
            printf '%s\n' "generated/${fixture_name}.out"
            ;;
    esac
}

# Function to validate a fixture
validate_fixture() {
    local fixture_name=$1
    local fixture_dir="$FIXTURES_DIR/$fixture_name"
    local kain_bin
    local target
    local output_path
    local expected_artifact

    echo "----------------------------------------"
    echo "Validating: $fixture_name"
    echo "----------------------------------------"

    if [ ! -d "$fixture_dir" ]; then
        echo -e "${RED}FAILED${NC}: Directory not found: $fixture_dir"
        FAILED=$((FAILED + 1))
        return 1
    fi

    # Check required files exist
    if [ ! -f "$fixture_dir/main.kn" ]; then
        echo -e "${RED}FAILED${NC}: main.kn not found"
        FAILED=$((FAILED + 1))
        return 1
    fi

    if [ ! -f "$fixture_dir/README.md" ]; then
        echo -e "${RED}FAILED${NC}: README.md not found"
        FAILED=$((FAILED + 1))
        return 1
    fi

    # Platform-specific checks
    if [ "$fixture_name" = "viewport_startup" ]; then
        if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "msys2" && "$OSTYPE" != "win32" && "$OSTYPE" != "cygwin" ]]; then
            echo -e "${YELLOW}SKIPPED${NC}: viewport_startup requires Windows (Win32)"
            SKIPPED=$((SKIPPED + 1))
            return 0
        fi
    fi

    if ! kain_bin=$(resolve_kain_bin); then
        echo -e "${YELLOW}SKIPPED${NC}: kain CLI not available, cannot compile"
        SKIPPED=$((SKIPPED + 1))
        return 0
    fi

    target=$(build_target_for_fixture "$fixture_name")
    output_path=$(build_output_for_fixture "$fixture_name" "$target")
    expected_artifact=$(expected_artifact_for_fixture "$fixture_name" "$target")

    if (
        cd "$fixture_dir"
        echo "Compiling $fixture_name with target $target..."
        if "$kain_bin" build main.kn --target "$target" --output "$output_path" 2>&1 | tee "/tmp/kain_build_${fixture_name}.log"; then
            if [ ! -f "$expected_artifact" ]; then
                echo -e "${RED}FAILED${NC}: expected artifact missing: $expected_artifact"
                exit 1
            fi
            echo -e "${GREEN}PASSED${NC}: $fixture_name compiled successfully"
            exit 0
        fi
        echo -e "${RED}FAILED${NC}: $fixture_name compilation failed"
        echo "See /tmp/kain_build_${fixture_name}.log for details"
        exit 1
    ); then
        PASSED=$((PASSED + 1))
    else
        FAILED=$((FAILED + 1))
        return 1
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
