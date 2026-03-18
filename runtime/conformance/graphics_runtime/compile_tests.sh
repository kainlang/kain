#!/usr/bin/env bash
# Compile graphics runtime conformance smoke tests

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
    LDFLAGS="-lws2_32 -luser32 -lgdi32 -lopengl32"
else
    LDFLAGS="-lm"
fi

echo "=== Compiling Graphics Runtime Smoke ==="
echo "Compiler: $C_COMPILER"
echo "Output: $OUT_DIR"
echo ""

"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_core.c" -o "$OUT_DIR/kain_runtime_core.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_version.c" -o "$OUT_DIR/kain_runtime_version.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_diagnostics.c" -o "$OUT_DIR/kain_runtime_diagnostics.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/platform/win32/kain_runtime_win32_shared.c" -o "$OUT_DIR/kain_runtime_win32_shared.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_realtime.c" -o "$OUT_DIR/kain_runtime_realtime.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/gfx/opengl/kain_gl_win32_host.c" -o "$OUT_DIR/kain_gl_win32_host.o"

"$C_COMPILER" $CFLAGS \
    "$SCRIPT_DIR/test_graphics_runtime_smoke.c" \
    "$OUT_DIR/kain_runtime_core.o" \
    "$OUT_DIR/kain_runtime_version.o" \
    "$OUT_DIR/kain_runtime_diagnostics.o" \
    "$OUT_DIR/kain_runtime_win32_shared.o" \
    "$OUT_DIR/kain_runtime_realtime.o" \
    "$OUT_DIR/kain_gl_win32_host.o" \
    -o "$OUT_DIR/graphics_runtime_smoke.exe" \
    $LDFLAGS

echo ""
echo "=== Compilation Complete ==="
echo "Run tests with:"
echo "  $OUT_DIR/graphics_runtime_smoke.exe"
