#!/bin/bash
# Compile actor runtime conformance tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
NATIVE_SRC="$RUNTIME_DIR/native/src"
NATIVE_INCLUDE="$RUNTIME_DIR/native/include"

echo "=== Compiling Actor Runtime Tests ==="
echo ""

# Compiler flags
CFLAGS="-I$NATIVE_INCLUDE -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L"
LDFLAGS=""

# Platform-specific flags
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    LDFLAGS="-lws2_32 -luser32 -lgdi32 -lopengl32"
else
    LDFLAGS="-lpthread -lm"
fi

# Compile runtime sources
echo "Compiling runtime sources..."
gcc $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_core.c" -o kain_runtime_core.o
gcc $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_version.c" -o kain_runtime_version.o
gcc $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_diagnostics.c" -o kain_runtime_diagnostics.o
gcc $CFLAGS -c "$NATIVE_SRC/core/kain_runtime_actor.c" -o kain_runtime_actor.o

# Compile tests
echo ""
echo "Compiling test_actor_spawn_basic..."
gcc $CFLAGS test_actor_spawn_basic.c kain_runtime_core.o kain_runtime_version.o kain_runtime_diagnostics.o kain_runtime_actor.o -o test_actor_spawn_basic $LDFLAGS

echo "Compiling test_actor_registry..."
gcc $CFLAGS test_actor_registry.c kain_runtime_core.o kain_runtime_version.o kain_runtime_diagnostics.o kain_runtime_actor.o -o test_actor_registry $LDFLAGS

echo "Compiling test_mailbox_backpressure..."
gcc $CFLAGS test_mailbox_backpressure.c kain_runtime_core.o kain_runtime_version.o kain_runtime_diagnostics.o kain_runtime_actor.o -o test_mailbox_backpressure $LDFLAGS

echo "Compiling test_actor_monitors..."
gcc $CFLAGS test_actor_monitors.c kain_runtime_core.o kain_runtime_version.o kain_runtime_diagnostics.o kain_runtime_actor.o -o test_actor_monitors $LDFLAGS

echo "Compiling test_actor_links..."
gcc $CFLAGS test_actor_links.c kain_runtime_core.o kain_runtime_version.o kain_runtime_diagnostics.o kain_runtime_actor.o -o test_actor_links $LDFLAGS

echo "Compiling test_actor_supervision..."
gcc $CFLAGS test_actor_supervision.c kain_runtime_core.o kain_runtime_version.o kain_runtime_diagnostics.o kain_runtime_actor.o -o test_actor_supervision $LDFLAGS

echo "Compiling test_actor_scheduler..."
gcc $CFLAGS test_actor_scheduler.c kain_runtime_core.o kain_runtime_version.o kain_runtime_diagnostics.o kain_runtime_actor.o -o test_actor_scheduler $LDFLAGS

echo ""
echo "=== Compilation Complete ==="
echo ""
echo "Run tests with:"
echo "  ./test_actor_spawn_basic"
echo "  ./test_actor_registry"
echo "  ./test_mailbox_backpressure"
echo "  ./test_actor_monitors"
echo "  ./test_actor_links"
echo "  ./test_actor_supervision"
echo "  ./test_actor_scheduler"
