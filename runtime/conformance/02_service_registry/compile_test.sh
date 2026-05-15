#!/bin/bash

# KAIN Runtime Service Registry Conformance Test Compilation Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
OUTPUT_DIR="$RUNTIME_DIR/../generated/conformance/02_service_registry"

mkdir -p "$OUTPUT_DIR"

echo "Compiling KAIN Runtime Service Registry conformance test..."

LDFLAGS=""
if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* || "${OSTYPE:-}" == win32* ]]; then
    LDFLAGS="-lws2_32 -lwinhttp -luser32 -lgdi32"
else
    LDFLAGS="-lpthread -lm"
fi

# Compile all required runtime sources
clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_core.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_core.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_version.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_version.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_diagnostics.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_diagnostics.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_services.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_services.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_actor.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_actor.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_native_net_system.o" \
    "$RUNTIME_DIR/native/src/core/kain_native_net_system.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_native_process_system.o" \
    "$RUNTIME_DIR/native/src/core/kain_native_process_system.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_contract.o" \
    "$RUNTIME_DIR/native/src/core/kain_runtime_contract.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_win32_shared.o" \
    "$RUNTIME_DIR/native/src/platform/win32/kain_runtime_win32_shared.c"

# Compile test
clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/test_service_registry.o" \
    "$SCRIPT_DIR/test_service_registry.c"

# Link test executable
clang \
    -o "$OUTPUT_DIR/test_service_registry" \
    "$OUTPUT_DIR/test_service_registry.o" \
    "$OUTPUT_DIR/kain_runtime_core.o" \
    "$OUTPUT_DIR/kain_runtime_version.o" \
    "$OUTPUT_DIR/kain_runtime_diagnostics.o" \
    "$OUTPUT_DIR/kain_runtime_services.o" \
    "$OUTPUT_DIR/kain_runtime_actor.o" \
    "$OUTPUT_DIR/kain_native_net_system.o" \
    "$OUTPUT_DIR/kain_native_process_system.o" \
    "$OUTPUT_DIR/kain_runtime_contract.o" \
    "$OUTPUT_DIR/kain_runtime_win32_shared.o" \
    $LDFLAGS

echo "✅ Compilation successful!"
echo "Output: $OUTPUT_DIR/test_service_registry"

# Run the test
echo ""
echo "Running test..."
"$OUTPUT_DIR/test_service_registry"

