#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"
NATIVE_SRC="$RUNTIME_DIR/native/src"
OUT_DIR="$SCRIPT_DIR/bin"

mkdir -p "$OUT_DIR"

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

CFLAGS="-I$NATIVE_INCLUDE -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS"
LDFLAGS=""

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    LDFLAGS="-lws2_32"
else
    LDFLAGS="-lm"
fi

"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_core.c" -o "$OUT_DIR/kain_runtime_core.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_version.c" -o "$OUT_DIR/kain_runtime_version.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_diagnostics.c" -o "$OUT_DIR/kain_runtime_diagnostics.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_native_input_system.c" -o "$OUT_DIR/kain_native_input_system.o"

"$C_COMPILER" $CFLAGS \
    "$SCRIPT_DIR/test_native_input_system_kernel.c" \
    "$OUT_DIR/kain_runtime_core.o" \
    "$OUT_DIR/kain_runtime_version.o" \
    "$OUT_DIR/kain_runtime_diagnostics.o" \
    "$OUT_DIR/kain_native_input_system.o" \
    -o "$OUT_DIR/native_input_system_kernel.exe" \
    $LDFLAGS
