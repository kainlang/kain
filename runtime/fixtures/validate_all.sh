#!/usr/bin/env bash
# Validate native runtime smoke fixtures and execute LLVM-target fixtures.

set -euo pipefail

echo "=========================================="
echo "Native Runtime Smoke Fixtures Validation"
echo "=========================================="
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIXTURES_DIR="$SCRIPT_DIR"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

PASSED=0
FAILED=0
SKIPPED=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

FIXTURE_NAMES=(
    contract_startup
    realtime_startup
    ui_startup
    viewport_startup
    llvm_heap_memory
    llvm_actor_message
    llvm_world_pipeline
)

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
        contract_startup|realtime_startup|llvm_heap_memory|llvm_actor_message|llvm_world_pipeline)
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

expected_exit_code_for_fixture() {
    case "$1" in
        llvm_world_pipeline)
            printf '%s\n' "10"
            ;;
        *)
            printf '%s\n' "0"
            ;;
    esac
}

llvm_ir_patterns_for_fixture() {
    case "$1" in
        llvm_heap_memory)
            cat <<'EOF'
call i8* @__kain_alloc(
call i8* @__kain_realloc(
call void @__kain_mem_store(
call void @__kain_mem_load(
EOF
            ;;
        llvm_actor_message)
            cat <<'EOF'
%KainActorMessage = type { i64, i8*, i64, i64 }
%KainActorSpawnConfig = type { i32 (i64, i8*, i8*)*, i8*, i64, i32, i32, i64, [128 x i8] }
define i32 @Printer_run(i64 %actor_id, i8* %mailbox, i8* %user_data)
call void @kain_actor_spawn_config_init(
call i64 @kain_actor_spawn(
call i32 @kain_actor_receive(
call i32 @kain_actor_send(
EOF
            ;;
        llvm_world_pipeline)
            cat <<'EOF'
define void @__kain_init_world_Studio()
call void @__kain_init_world_Studio()
call i64 @choose_value(i64
call i64 @stage_bias(i64
EOF
            ;;
    esac
}

validate_llvm_ir_patterns() {
    local fixture_name=$1
    local llvm_ir_path=$2
    local pattern
    local missing=0

    while IFS= read -r pattern; do
        [ -z "$pattern" ] && continue
        if ! grep -Fq "$pattern" "$llvm_ir_path"; then
            echo -e "${RED}FAILED${NC}: missing LLVM evidence in $fixture_name: $pattern"
            missing=1
        fi
    done < <(llvm_ir_patterns_for_fixture "$fixture_name")

    return $missing
}

run_llvm_fixture_binary() {
    local fixture_name=$1
    local artifact_path=$2
    local expected_exit_code=$3
    local run_log="/tmp/kain_run_${fixture_name}.log"
    local exit_code=0

    if [ ! -x "$artifact_path" ]; then
        echo -e "${RED}FAILED${NC}: LLVM artifact is not executable: $artifact_path"
        return 1
    fi

    set +e
    if command -v timeout >/dev/null 2>&1; then
        timeout 15s "$artifact_path" >"$run_log" 2>&1
        exit_code=$?
    else
        "$artifact_path" >"$run_log" 2>&1
        exit_code=$?
    fi
    set -e

    if [ "$exit_code" -eq 124 ]; then
        echo -e "${RED}FAILED${NC}: $fixture_name timed out during execution"
        echo "See $run_log for details"
        return 1
    fi

    if [ "$exit_code" -ne "$expected_exit_code" ]; then
        echo -e "${RED}FAILED${NC}: $fixture_name exited with $exit_code (expected $expected_exit_code)"
        echo "See $run_log for details"
        return 1
    fi

    return 0
}

validate_fixture() {
    local fixture_name=$1
    local fixture_dir="$FIXTURES_DIR/$fixture_name"
    local kain_bin
    local target
    local output_path
    local expected_artifact
    local expected_exit_code
    local build_log="/tmp/kain_build_${fixture_name}.log"

    echo "----------------------------------------"
    echo "Validating: $fixture_name"
    echo "----------------------------------------"

    if [ ! -d "$fixture_dir" ]; then
        echo -e "${RED}FAILED${NC}: Directory not found: $fixture_dir"
        FAILED=$((FAILED + 1))
        return 1
    fi

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
    expected_exit_code=$(expected_exit_code_for_fixture "$fixture_name")

    if (
        cd "$fixture_dir"
        echo "Compiling $fixture_name with target $target..."
        if "$kain_bin" build main.kn --target "$target" --output "$output_path" 2>&1 | tee "$build_log"; then
            if [ ! -f "$expected_artifact" ]; then
                echo -e "${RED}FAILED${NC}: expected artifact missing: $expected_artifact"
                exit 1
            fi

            if [ "$target" = "llvm" ]; then
                if ! validate_llvm_ir_patterns "$fixture_name" "$output_path"; then
                    exit 1
                fi

                if ! run_llvm_fixture_binary "$fixture_name" "$expected_artifact" "$expected_exit_code"; then
                    exit 1
                fi
            fi

            echo -e "${GREEN}PASSED${NC}: $fixture_name validated successfully"
            exit 0
        fi

        echo -e "${RED}FAILED${NC}: $fixture_name compilation failed"
        echo "See $build_log for details"
        exit 1
    ); then
        PASSED=$((PASSED + 1))
    else
        FAILED=$((FAILED + 1))
        return 1
    fi

    echo ""
}

for fixture_name in "${FIXTURE_NAMES[@]}"; do
    validate_fixture "$fixture_name"
done

echo "=========================================="
echo "Validation Summary"
echo "=========================================="
echo -e "${GREEN}PASSED${NC}:  $PASSED"
echo -e "${RED}FAILED${NC}:  $FAILED"
echo -e "${YELLOW}SKIPPED${NC}: $SKIPPED"
echo ""

if [ "$FAILED" -gt 0 ]; then
    echo -e "${RED}Validation FAILED${NC}"
    exit 1
fi

echo -e "${GREEN}Validation PASSED${NC}"
exit 0
