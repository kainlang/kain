#!/usr/bin/env bash
# ============================================================================
#  runtime-test-linux.sh
#  Fast-path C runtime smoke test — no Bazel, no downloads, just gcc.
#  Run this from WSL at /mnt/x
#
#  Usage:  ./scripts/runtime-test-linux.sh [test_name]
#          ./scripts/runtime-test-linux.sh              # runs all
#          ./scripts/runtime-test-linux.sh platform     # just platform tests
# ============================================================================
set -euo pipefail

REPO="/mnt/x"
OUT="/tmp/kain-linux-test"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

mkdir -p "$OUT"

# --- compile & run a single C test ---
run_test() {
    local name="$1"
    local src="$2"
    local extra_srcs="$3"

    echo -e "${CYAN}[build]${NC} $name"

    gcc -std=c11 -g -O0 \
        -D_GNU_SOURCE -D_POSIX_C_SOURCE=200112 -D_FILE_OFFSET_BITS=64 \
        $extra_srcs \
        "$src" \
        -I "$REPO/runtime/native/include" \
        -lpthread -ldl -lrt -lm \
        -o "$OUT/$name"

    echo -e "${CYAN}[run]${NC}  $name"
    "$OUT/$name" && echo -e "${GREEN}[PASS]${NC} $name" || echo -e "${RED}[FAIL]${NC} $name"
    echo ""
}

# --- common runtime core sources (everything that's not platform-specific) ---
CORE_SRCS=$(find "$REPO/runtime/native/src/core" -name '*.c' | tr '\n' ' ')
PLATFORM_SRCS="$REPO/runtime/native/src/platform/platform.c $REPO/runtime/native/src/platform/platform_library.c $REPO/runtime/native/src/platform/linux/linux_shared.c"

# --- decide what to run ---
if [ $# -eq 0 ] || [ "$1" = "all" ]; then
    echo -e "${CYAN}═══ Running all Linux runtime smoke tests ═══${NC}"
    echo ""

    # Test files in runtime/native/tests
    for test_src in "$REPO/runtime/native/tests/"test_*.c; do
        test_name=$(basename "$test_src" .c)
        run_test "$test_name" "$test_src" "$CORE_SRCS $PLATFORM_SRCS"
    done

elif [ "$1" = "platform" ]; then
    run_test "test_platform_library" \
        "$REPO/runtime/native/tests/test_platform_library.c" \
        "$CORE_SRCS $PLATFORM_SRCS"

elif [ "$1" = "fs" ]; then
    run_test "test_stdlib_abi_fs" \
        "$REPO/runtime/native/tests/test_stdlib_abi_fs.c" \
        "$CORE_SRCS $PLATFORM_SRCS"

elif [ "$1" = "actor" ]; then
    for t in test_actor_monitor_link test_actor_supervision; do
        run_test "$t" \
            "$REPO/runtime/native/tests/$t.c" \
            "$CORE_SRCS $PLATFORM_SRCS"
    done

elif [ "$1" = "ownership" ]; then
    run_test "test_ownership_memory" \
        "$REPO/runtime/native/tests/test_ownership_memory.c" \
        "$CORE_SRCS $PLATFORM_SRCS"

elif [ "$1" = "quick" ]; then
    # Fastest possible sanity — just the platform library test
    gcc -std=c11 -g -O0 \
        -D_GNU_SOURCE -D_POSIX_C_SOURCE=200112 -D_FILE_OFFSET_BITS=64 \
        "$REPO/runtime/native/src/platform/linux/linux_shared.c" \
        "$REPO/runtime/native/tests/test_platform_library.c" \
        -I "$REPO/runtime/native/include" \
        -ldl -lm \
        -o "$OUT/test_quick"
    "$OUT/test_quick"
else
    run_test "$1" \
        "$REPO/runtime/native/tests/${1}.c" \
        "$CORE_SRCS $PLATFORM_SRCS"
fi

echo -e "${GREEN}Done.${NC}"
