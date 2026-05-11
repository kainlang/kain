#!/usr/bin/env bash
# Compile UI runtime conformance tests and supporting objects.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_SRC="$RUNTIME_DIR/native/src"
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"
BIN_DIR="$SCRIPT_DIR/bin"

CFLAGS="-I$NATIVE_INCLUDE -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -D_CRT_SECURE_NO_WARNINGS"
LDFLAGS=""
PLATFORM_SHARED_SOURCE="$NATIVE_SRC/platform/linux/kain_runtime_linux_shared.c"

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

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    LDFLAGS="-lws2_32 -luser32 -lgdi32 -lopengl32"
    PLATFORM_SHARED_SOURCE="$NATIVE_SRC/platform/win32/kain_runtime_win32_shared.c"
else
    LDFLAGS="-lpthread -lm"
fi

echo "=== Compiling UI Runtime Tests ==="
echo "Using compiler: $C_COMPILER"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

mkdir -p "$BIN_DIR"
rm -f "$BIN_DIR"/*.o "$BIN_DIR"/*.obj "$BIN_DIR"/*.exe "$BIN_DIR"/* 2>/dev/null || true

echo "Compiling supporting runtime objects..."
"$C_COMPILER" $CFLAGS -c "$PLATFORM_SHARED_SOURCE" -o "$BIN_DIR/kain_runtime_platform_shared.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_core.c" -o "$BIN_DIR/kain_runtime_core.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_version.c" -o "$BIN_DIR/kain_runtime_version.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_diagnostics.c" -o "$BIN_DIR/kain_runtime_diagnostics.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/kain_ui_compiled_bundle.c" -o "$BIN_DIR/kain_ui_compiled_bundle.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/kain_ui_runtime.c" -o "$BIN_DIR/kain_ui_runtime.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/kain_native_ui_system.c" -o "$BIN_DIR/kain_native_ui_system.o"

echo "Compiling overlay sources (compile-only smoke)..."
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/kain_ui_compiled_overlay.c" -o "$BIN_DIR/kain_ui_compiled_overlay.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/kain_ui_overlay.c" -o "$BIN_DIR/kain_ui_overlay.o"

echo ""
echo "Compiling test_ui_runtime_bundle..."
"$C_COMPILER" $CFLAGS test_ui_runtime_bundle.c "$BIN_DIR/kain_ui_runtime.o" "$BIN_DIR/kain_ui_compiled_bundle.o" "$BIN_DIR/kain_runtime_platform_shared.o" -o "$BIN_DIR/test_ui_runtime_bundle.exe" $LDFLAGS

echo "Compiling test_ui_runtime_focus..."
"$C_COMPILER" $CFLAGS test_ui_runtime_focus.c "$BIN_DIR/kain_ui_runtime.o" "$BIN_DIR/kain_ui_compiled_bundle.o" "$BIN_DIR/kain_runtime_platform_shared.o" -o "$BIN_DIR/test_ui_runtime_focus.exe" $LDFLAGS

echo "Compiling test_ui_runtime_parity..."
"$C_COMPILER" $CFLAGS test_ui_runtime_parity.c "$BIN_DIR/kain_ui_runtime.o" "$BIN_DIR/kain_ui_compiled_bundle.o" "$BIN_DIR/kain_runtime_platform_shared.o" -o "$BIN_DIR/test_ui_runtime_parity.exe" $LDFLAGS

echo "Compiling test_native_ui_system_kernel..."
"$C_COMPILER" $CFLAGS test_native_ui_system_kernel.c "$BIN_DIR/kain_native_ui_system.o" "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" -o "$BIN_DIR/test_native_ui_system_kernel.exe" $LDFLAGS

echo "Compiling test_native_ui_system_host_services..."
"$C_COMPILER" $CFLAGS test_native_ui_system_host_services.c "$BIN_DIR/kain_native_ui_system.o" "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" -o "$BIN_DIR/test_native_ui_system_host_services.exe" $LDFLAGS

echo ""
echo "=== Compilation Complete ==="

popd > /dev/null
