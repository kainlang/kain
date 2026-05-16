#!/bin/bash

# KAIN Runtime ABI and Startup Validation Test Compilation Script
# Task 1.6: Add ABI and startup validation tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$SCRIPT_DIR/../.."
OUTPUT_DIR="$RUNTIME_DIR/../generated/conformance/03_abi_startup_validation"

mkdir -p "$OUTPUT_DIR"

echo "=== Compiling KAIN Runtime ABI and Startup Validation Test ==="
echo ""

LDFLAGS=""
if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* || "${OSTYPE:-}" == win32* ]]; then
    LDFLAGS="-lws2_32 -lwinhttp -luser32 -lgdi32"
else
    LDFLAGS="-lpthread -lm"
fi

# Compile runtime sources
echo "Compiling runtime sources..."

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_core.o" \
    "$RUNTIME_DIR/native/src/core/core.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/runtime_version.o" \
    "$RUNTIME_DIR/native/src/core/version.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/runtime_diagnostics.o" \
    "$RUNTIME_DIR/native/src/core/diagnostics.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/runtime_services.o" \
    "$RUNTIME_DIR/native/src/core/services.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/runtime_actor.o" \
    "$RUNTIME_DIR/native/src/core/actor.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/abi_net_system.o" \
    "$RUNTIME_DIR/native/src/core/net_system.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/abi_process_system.o" \
    "$RUNTIME_DIR/native/src/core/process_system.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/runtime_contract.o" \
    "$RUNTIME_DIR/native/src/core/contract.c"

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/kain_runtime_win32_shared.o" \
    "$RUNTIME_DIR/native/src/platform/win32/win32_shared.c"

# Compile test
echo "Compiling test..."

clang -c \
    -I"$RUNTIME_DIR/native/include" \
    -o "$OUTPUT_DIR/test_abi_startup_validation.o" \
    "$SCRIPT_DIR/test_abi_startup_validation.c"

# Link test executable
echo "Linking test executable..."

clang \
    -o "$OUTPUT_DIR/test_abi_startup_validation" \
    "$OUTPUT_DIR/test_abi_startup_validation.o" \
    "$OUTPUT_DIR/kain_runtime_core.o" \
    "$OUTPUT_DIR/runtime_version.o" \
    "$OUTPUT_DIR/runtime_diagnostics.o" \
    "$OUTPUT_DIR/runtime_services.o" \
    "$OUTPUT_DIR/runtime_actor.o" \
    "$OUTPUT_DIR/abi_net_system.o" \
    "$OUTPUT_DIR/abi_process_system.o" \
    "$OUTPUT_DIR/runtime_contract.o" \
    "$OUTPUT_DIR/kain_runtime_win32_shared.o" \
    $LDFLAGS

echo ""
echo "✅ Compilation successful!"
echo "Output: $OUTPUT_DIR/test_abi_startup_validation"
echo ""

# Run the test
echo "=== Running Test ==="
echo ""
"$OUTPUT_DIR/test_abi_startup_validation"

TEST_RESULT=$?

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo "✅ All tests passed"
else
    echo "❌ Tests failed with exit code $TEST_RESULT"
fi

exit $TEST_RESULT
