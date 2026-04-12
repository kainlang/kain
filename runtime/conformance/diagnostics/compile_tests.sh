#!/usr/bin/env bash
# Compile diagnostics conformance tests.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
NATIVE_INCLUDE="$RUNTIME_ROOT/native/include"
NATIVE_SRC="$RUNTIME_ROOT/native/src"
OUT_DIR="$SCRIPT_DIR/bin"

TEST_BINARIES=(
    "test_structured_runtime_diagnostics"
    "test_diagnostic_error_codes"
    "test_startup_failure_reporting"
)

if [[ -n "${CC:-}" ]]; then
    C_COMPILER="$CC"
elif command -v clang > /dev/null 2>&1; then
    C_COMPILER="clang"
elif command -v gcc > /dev/null 2>&1; then
    C_COMPILER="gcc"
elif command -v cc > /dev/null 2>&1; then
    C_COMPILER="cc"
else
    echo "No supported C compiler found on PATH. Set CC explicitly." >&2
    exit 1
fi

COMMON_CFLAGS=(
    -Wall
    -Wextra
    -std=c11
    -D_POSIX_C_SOURCE=200809L
    -DKAIN_RUNTIME_VENDOR_STUBS_ONLY=1
    -I"$NATIVE_INCLUDE"
)

COMMON_SOURCES=(
    "$NATIVE_SRC/core/kain_runtime_version.c"
    "$NATIVE_SRC/core/kain_runtime_diagnostics.c"
    "$NATIVE_SRC/core/kain_runtime_services.c"
    "$NATIVE_SRC/core/kain_runtime_contract.c"
    "$NATIVE_SRC/vendor/kain_runtime_vendor_lane.c"
)

if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* || "${OSTYPE:-}" == win32* ]]; then
    PLATFORM_CFLAGS=(-D_CRT_SECURE_NO_WARNINGS)
    PLATFORM_LDFLAGS=(-lws2_32 -luser32 -lgdi32 -lopengl32)
    COMMON_SOURCES+=("$NATIVE_SRC/platform/win32/kain_runtime_win32_shared.c")
else
    PLATFORM_CFLAGS=()
    PLATFORM_LDFLAGS=(-lpthread -lm)
    COMMON_SOURCES+=("$NATIVE_SRC/platform/linux/kain_runtime_linux_shared.c")
fi

mkdir -p "$OUT_DIR"

echo "Compiling diagnostics tests..."
echo "Compiler: $C_COMPILER"
echo "Output directory: $OUT_DIR"
echo ""

for test_name in "${TEST_BINARIES[@]}"; do
    source_file="$SCRIPT_DIR/$test_name.c"
    output_file="$OUT_DIR/$test_name"

    if [[ ! -f "$source_file" ]]; then
        echo "Missing diagnostics test source: $source_file" >&2
        exit 1
    fi

    echo "  -> $test_name"
    "$C_COMPILER" \
        "${COMMON_CFLAGS[@]}" \
        "${PLATFORM_CFLAGS[@]}" \
        "$source_file" \
        "${COMMON_SOURCES[@]}" \
        -o "$output_file" \
        "${PLATFORM_LDFLAGS[@]}"
done

echo ""
echo "Diagnostics tests compiled successfully."
