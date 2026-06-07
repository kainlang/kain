#!/usr/bin/env bash
# Compile UI runtime conformance tests and supporting objects.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_SRC="$RUNTIME_DIR/native/src"
NATIVE_INCLUDE=

source "$SCRIPT_DIR/../_shared/runtime_helpers.sh"
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"
BIN_DIR="$SCRIPT_DIR/bin"

CFLAGS="-I$NATIVE_INCLUDE -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -D_CRT_SECURE_NO_WARNINGS"
LDFLAGS=""
PLATFORM_SHARED_SOURCE="$NATIVE_SRC/platform/linux/linux_shared.c"
PLATFORM_OBJECTS=()

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
    LDFLAGS="-lws2_32 -luser32 -lgdi32 -lshell32"
    PLATFORM_SHARED_SOURCE="$NATIVE_SRC/platform/win32/win32_shared.c"
else
    LDFLAGS="$RUNTIME_LDFLAGS"
fi

echo "=== Compiling UI Runtime Tests ==="
echo "Using compiler: $C_COMPILER"
echo ""

pushd "$SCRIPT_DIR" > /dev/null

mkdir -p "$BIN_DIR"
rm -f "$BIN_DIR"/*.o "$BIN_DIR"/*.obj "$BIN_DIR"/*.exe "$BIN_DIR"/* 2>/dev/null || true

echo "Compiling supporting runtime objects..."
"$C_COMPILER" $CFLAGS -c "$PLATFORM_SHARED_SOURCE" -o "$BIN_DIR/kain_runtime_platform_shared.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/core.c" -o "$BIN_DIR/kain_runtime_core.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/version.c" -o "$BIN_DIR/runtime_version.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/diagnostics.c" -o "$BIN_DIR/runtime_diagnostics.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/ui_compiled_bundle.c" -o "$BIN_DIR/kain_ui_compiled_bundle.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/ui_runtime.c" -o "$BIN_DIR/kain_ui_runtime.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/ui_hot_reload.c" -o "$BIN_DIR/kain_ui_hot_reload.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/ui_host_adapter.c" -o "$BIN_DIR/abi_ui_host_adapter.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/ui/ui_system.c" -o "$BIN_DIR/abi_ui_system.o"

echo ""
echo "Compiling test_ui_runtime_bundle..."
"$C_COMPILER" $CFLAGS test_ui_runtime_bundle.c "$BIN_DIR/kain_ui_runtime.o" "$BIN_DIR/kain_ui_compiled_bundle.o" "$BIN_DIR/kain_runtime_platform_shared.o" -o "$BIN_DIR/test_ui_runtime_bundle.exe" $LDFLAGS

echo "Compiling test_ui_runtime_focus..."
"$C_COMPILER" $CFLAGS test_ui_runtime_focus.c "$BIN_DIR/kain_ui_runtime.o" "$BIN_DIR/kain_ui_compiled_bundle.o" "$BIN_DIR/kain_runtime_platform_shared.o" -o "$BIN_DIR/test_ui_runtime_focus.exe" $LDFLAGS

echo "Compiling test_ui_runtime_parity..."
"$C_COMPILER" $CFLAGS test_ui_runtime_parity.c "$BIN_DIR/kain_ui_runtime.o" "$BIN_DIR/kain_ui_compiled_bundle.o" "$BIN_DIR/kain_runtime_platform_shared.o" -o "$BIN_DIR/test_ui_runtime_parity.exe" $LDFLAGS

echo "Compiling test_ui_runtime_reload..."
"$C_COMPILER" $CFLAGS test_ui_runtime_reload.c "$BIN_DIR/kain_ui_runtime.o" "$BIN_DIR/kain_ui_compiled_bundle.o" "$BIN_DIR/kain_ui_hot_reload.o" "$BIN_DIR/kain_runtime_platform_shared.o" -o "$BIN_DIR/test_ui_runtime_reload.exe" $LDFLAGS

echo "Compiling test_native_ui_system_kernel..."
"$C_COMPILER" $CFLAGS test_native_ui_system_kernel.c "$BIN_DIR/abi_ui_system.o" "$BIN_DIR/abi_ui_host_adapter.o" "${PLATFORM_OBJECTS[@]}" "$BIN_DIR/kain_runtime_platform_shared.o" "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/runtime_version.o" "$BIN_DIR/runtime_diagnostics.o" -o "$BIN_DIR/test_native_ui_system_kernel.exe" $LDFLAGS

echo "Compiling test_native_ui_system_host_services..."
"$C_COMPILER" $CFLAGS test_native_ui_system_host_services.c "$BIN_DIR/abi_ui_system.o" "$BIN_DIR/abi_ui_host_adapter.o" "${PLATFORM_OBJECTS[@]}" "$BIN_DIR/kain_runtime_platform_shared.o" "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/runtime_version.o" "$BIN_DIR/runtime_diagnostics.o" -o "$BIN_DIR/test_native_ui_system_host_services.exe" $LDFLAGS

echo ""
echo "=== Compilation Complete ==="

popd > /dev/null
