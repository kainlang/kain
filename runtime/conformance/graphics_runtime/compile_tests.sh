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
PLATFORM_SHARED_SOURCE="$NATIVE_SRC/platform/linux/kain_runtime_linux_shared.c"
PLATFORM_GRAPHICS_SOURCE="$NATIVE_SRC/platform/linux/kain_runtime_linux_graphics.c"
BUILD_GL_HOST=0
HOST_RUNTIME_OBJECTS=()

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    PLATFORM_SHARED_SOURCE="$NATIVE_SRC/platform/win32/kain_runtime_win32_shared.c"
    PLATFORM_GRAPHICS_SOURCE="$NATIVE_SRC/platform/win32/kain_runtime_win32_graphics.c"
    BUILD_GL_HOST=1
    LDFLAGS="-lws2_32 -luser32 -lgdi32 -lopengl32"
else
    LDFLAGS="-lpthread -lm"
fi

echo "=== Compiling Graphics Runtime Smoke ==="
echo "Compiler: $C_COMPILER"
echo "Output: $OUT_DIR"
echo ""

"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_core.c" -o "$OUT_DIR/kain_runtime_core.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_version.c" -o "$OUT_DIR/kain_runtime_version.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_diagnostics.c" -o "$OUT_DIR/kain_runtime_diagnostics.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_native_graphics_system.c" -o "$OUT_DIR/kain_native_graphics_system.o"
"$C_COMPILER" $CFLAGS -c "$PLATFORM_SHARED_SOURCE" -o "$OUT_DIR/kain_runtime_platform_shared.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_realtime.c" -o "$OUT_DIR/kain_runtime_realtime.o"
if [[ -n "$PLATFORM_GRAPHICS_SOURCE" ]]; then
    "$C_COMPILER" $CFLAGS -c "$PLATFORM_GRAPHICS_SOURCE" -o "$OUT_DIR/kain_runtime_platform_graphics.o"
    HOST_RUNTIME_OBJECTS+=("$OUT_DIR/kain_runtime_platform_graphics.o")
fi
if [[ $BUILD_GL_HOST -eq 1 ]]; then
    "$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/gfx/opengl/kain_gl_win32_host.c" -o "$OUT_DIR/kain_gl_win32_host.o"
    HOST_RUNTIME_OBJECTS+=("$OUT_DIR/kain_gl_win32_host.o")
fi

"$C_COMPILER" $CFLAGS \
    "$SCRIPT_DIR/test_graphics_runtime_smoke.c" \
    "$OUT_DIR/kain_runtime_core.o" \
    "$OUT_DIR/kain_runtime_version.o" \
    "$OUT_DIR/kain_runtime_diagnostics.o" \
    "$OUT_DIR/kain_runtime_platform_shared.o" \
    "$OUT_DIR/kain_runtime_realtime.o" \
    "${HOST_RUNTIME_OBJECTS[@]}" \
    -o "$OUT_DIR/graphics_runtime_smoke.exe" \
    $LDFLAGS

echo ""
echo "=== Compiling Native Graphics System Kernel ==="

"$C_COMPILER" $CFLAGS \
    "$SCRIPT_DIR/test_native_graphics_system_kernel.c" \
    "$OUT_DIR/kain_native_graphics_system.o" \
    "$OUT_DIR/kain_runtime_core.o" \
    "$OUT_DIR/kain_runtime_version.o" \
    "$OUT_DIR/kain_runtime_diagnostics.o" \
    -o "$OUT_DIR/native_graphics_system_kernel.exe" \
    $LDFLAGS

echo ""
echo "=== Compiling Graphics Binding Rules ==="

"$C_COMPILER" $CFLAGS \
    "$SCRIPT_DIR/test_graphics_runtime_binding_rules.c" \
    "$OUT_DIR/kain_runtime_core.o" \
    "$OUT_DIR/kain_runtime_version.o" \
    "$OUT_DIR/kain_runtime_diagnostics.o" \
    "$OUT_DIR/kain_runtime_platform_shared.o" \
    "$OUT_DIR/kain_runtime_realtime.o" \
    "${HOST_RUNTIME_OBJECTS[@]}" \
    -o "$OUT_DIR/graphics_runtime_binding_rules.exe" \
    $LDFLAGS

echo ""
echo "=== Compilation Complete ==="
echo "Run tests with:"
echo "  $OUT_DIR/graphics_runtime_smoke.exe"
echo "  $OUT_DIR/native_graphics_system_kernel.exe"
echo "  $OUT_DIR/graphics_runtime_binding_rules.exe"
