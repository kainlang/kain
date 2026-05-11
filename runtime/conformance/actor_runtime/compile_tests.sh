#!/bin/bash
# Compile actor runtime conformance tests

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_SRC="$RUNTIME_DIR/native/src"
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"
BIN_DIR="$SCRIPT_DIR/bin"

echo "=== Compiling Actor Runtime Tests ==="
echo ""
echo "Output: $BIN_DIR"
echo ""

mkdir -p "$BIN_DIR"

# Compiler flags
CFLAGS="-I$NATIVE_INCLUDE -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L"
LDFLAGS=""

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

# Platform-specific flags
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    LDFLAGS="-lws2_32 -luser32 -lgdi32 -lopengl32"
    CFLAGS="$CFLAGS -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -Wno-deprecated-declarations -Wno-unused-parameter"
else
    LDFLAGS="-lpthread -lm"
fi

echo "Using compiler: $C_COMPILER"

# Compile runtime sources
echo "Compiling runtime sources..."
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_core.c" -o "$BIN_DIR/kain_runtime_core.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_version.c" -o "$BIN_DIR/kain_runtime_version.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_diagnostics.c" -o "$BIN_DIR/kain_runtime_diagnostics.o"
"$C_COMPILER" $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_actor.c" -o "$BIN_DIR/kain_runtime_actor.o"

# Compile tests
echo ""
echo "Compiling test_actor_abi_contract..."
"$C_COMPILER" $CFLAGS test_actor_abi_contract.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_actor_abi_contract" $LDFLAGS

echo ""
echo "Compiling test_actor_spawn_basic..."
"$C_COMPILER" $CFLAGS test_actor_spawn_basic.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_actor_spawn_basic" $LDFLAGS

echo "Compiling test_actor_registry..."
"$C_COMPILER" $CFLAGS test_actor_registry.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_actor_registry" $LDFLAGS

echo "Compiling test_mailbox_backpressure..."
"$C_COMPILER" $CFLAGS test_mailbox_backpressure.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_mailbox_backpressure" $LDFLAGS

echo "Compiling test_actor_monitors..."
"$C_COMPILER" $CFLAGS test_actor_monitors.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_actor_monitors" $LDFLAGS

echo "Compiling test_actor_links..."
"$C_COMPILER" $CFLAGS test_actor_links.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_actor_links" $LDFLAGS

echo "Compiling test_actor_supervision..."
"$C_COMPILER" $CFLAGS test_actor_supervision.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_actor_supervision" $LDFLAGS

echo "Compiling test_actor_scheduler..."
"$C_COMPILER" $CFLAGS test_actor_scheduler.c "$BIN_DIR/kain_runtime_core.o" "$BIN_DIR/kain_runtime_version.o" "$BIN_DIR/kain_runtime_diagnostics.o" "$BIN_DIR/kain_runtime_actor.o" -o "$BIN_DIR/test_actor_scheduler" $LDFLAGS

echo ""
echo "=== Compilation Complete ==="
echo ""
echo "Run tests with:"
echo "  $BIN_DIR/test_actor_abi_contract"
echo "  $BIN_DIR/test_actor_spawn_basic"
echo "  $BIN_DIR/test_actor_registry"
echo "  $BIN_DIR/test_mailbox_backpressure"
echo "  $BIN_DIR/test_actor_monitors"
echo "  $BIN_DIR/test_actor_links"
echo "  $BIN_DIR/test_actor_supervision"
echo "  $BIN_DIR/test_actor_scheduler"
