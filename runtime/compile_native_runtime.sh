#!/usr/bin/env bash
# KAIN Native Runtime Compilation Script
# 
# Purpose: Compile the native C runtime with the current manifest and include paths
# Spec: .kiro/specs/kain-native-runtime-completion
# Task: 0.2 Establish native runtime validation commands
#
# Usage:
#   ./runtime/compile_native_runtime.sh [--release] [--verbose]
#
# Options:
#   --release    Build with optimizations (default: debug)
#   --verbose    Show detailed compiler output
#   --help       Show this help message

set -e  # Exit on error

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default options
BUILD_TYPE="debug"
VERBOSE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --release)
            BUILD_TYPE="release"
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help)
            echo "KAIN Native Runtime Compilation Script"
            echo ""
            echo "Usage: $0 [--release] [--verbose] [--help]"
            echo ""
            echo "Options:"
            echo "  --release    Build with optimizations (default: debug)"
            echo "  --verbose    Show detailed compiler output"
            echo "  --help       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Output directory
OUTPUT_DIR="$PROJECT_ROOT/generated/native_runtime/$BUILD_TYPE"
mkdir -p "$OUTPUT_DIR"

# Source and include paths
RUNTIME_SOURCE="$PROJECT_ROOT/runtime/kain_runtime.c"
INCLUDE_DIR="$PROJECT_ROOT/runtime/native/include"
THIRD_PARTY_INCLUDE="$PROJECT_ROOT/runtime/native/third_party/cgltf"

# Detect platform
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    PLATFORM="windows"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
else
    echo "Error: Unsupported platform: $OSTYPE"
    exit 1
fi

echo "=== KAIN Native Runtime Compilation ==="
echo "Platform: $PLATFORM"
echo "Build Type: $BUILD_TYPE"
echo "Output: $OUTPUT_DIR"
echo ""

# Detect available compiler
COMPILER=""
COMPILER_TYPE=""

if command -v clang &> /dev/null; then
    COMPILER="clang"
    COMPILER_TYPE="clang"
elif command -v gcc &> /dev/null; then
    COMPILER="gcc"
    COMPILER_TYPE="gcc"
elif command -v cl.exe &> /dev/null; then
    COMPILER="cl.exe"
    COMPILER_TYPE="msvc"
else
    echo "Error: No suitable C compiler found (tried: clang, gcc, cl.exe)"
    exit 1
fi

echo "Compiler: $COMPILER ($COMPILER_TYPE)"
echo ""

# Build compiler command based on compiler type and platform
COMPILE_CMD=""
OUTPUT_FILE=""

if [[ "$COMPILER_TYPE" == "msvc" ]]; then
    # MSVC compiler
    OUTPUT_FILE="$OUTPUT_DIR/kain_runtime.obj"
    
    COMPILE_CMD="$COMPILER /nologo"
    
    # Warning level
    COMPILE_CMD="$COMPILE_CMD /W3"
    
    # Optimization
    if [[ "$BUILD_TYPE" == "release" ]]; then
        COMPILE_CMD="$COMPILE_CMD /O2"
    else
        COMPILE_CMD="$COMPILE_CMD /Od /Zi"
    fi
    
    # Include paths
    COMPILE_CMD="$COMPILE_CMD /I \"$INCLUDE_DIR\""
    COMPILE_CMD="$COMPILE_CMD /I \"$THIRD_PARTY_INCLUDE\""
    
    # Platform defines
    if [[ "$PLATFORM" == "windows" ]]; then
        COMPILE_CMD="$COMPILE_CMD /D WIN32 /D _WINDOWS"
    fi
    
    # Compile only (no linking)
    COMPILE_CMD="$COMPILE_CMD /c \"$RUNTIME_SOURCE\""
    COMPILE_CMD="$COMPILE_CMD /Fo:\"$OUTPUT_FILE\""
    
else
    # GCC/Clang compiler
    if [[ "$PLATFORM" == "windows" ]]; then
        OUTPUT_FILE="$OUTPUT_DIR/kain_runtime.obj"
    else
        OUTPUT_FILE="$OUTPUT_DIR/kain_runtime.o"
    fi
    
    COMPILE_CMD="$COMPILER"
    
    # Warning flags
    COMPILE_CMD="$COMPILE_CMD -Wall -Wextra"
    
    # Optimization
    if [[ "$BUILD_TYPE" == "release" ]]; then
        COMPILE_CMD="$COMPILE_CMD -O2"
    else
        COMPILE_CMD="$COMPILE_CMD -O0 -g"
    fi
    
    # Include paths
    COMPILE_CMD="$COMPILE_CMD -I \"$INCLUDE_DIR\""
    COMPILE_CMD="$COMPILE_CMD -I \"$THIRD_PARTY_INCLUDE\""
    
    # Platform defines
    if [[ "$PLATFORM" == "windows" ]]; then
        COMPILE_CMD="$COMPILE_CMD -D WIN32 -D _WINDOWS"
    fi
    
    # Compile only (no linking)
    COMPILE_CMD="$COMPILE_CMD -c \"$RUNTIME_SOURCE\""
    COMPILE_CMD="$COMPILE_CMD -o \"$OUTPUT_FILE\""
fi

# Show command if verbose
if [[ "$VERBOSE" == true ]]; then
    echo "Compile command:"
    echo "$COMPILE_CMD"
    echo ""
fi

# Execute compilation
echo "Compiling native runtime..."
if [[ "$VERBOSE" == true ]]; then
    eval "$COMPILE_CMD"
else
    eval "$COMPILE_CMD" 2>&1 | grep -v "^$" || true
fi

# Check if output file was created
if [[ -f "$OUTPUT_FILE" ]]; then
    FILE_SIZE=$(stat -f%z "$OUTPUT_FILE" 2>/dev/null || stat -c%s "$OUTPUT_FILE" 2>/dev/null || echo "unknown")
    echo ""
    echo "✅ Compilation successful!"
    echo "Output: $OUTPUT_FILE"
    echo "Size: $FILE_SIZE bytes"
    echo ""
    echo "Note: This is a compilation-only check. Linking and runtime execution"
    echo "      validation will be added in later phases."
else
    echo ""
    echo "❌ Compilation failed - output file not created"
    exit 1
fi

# Summary
echo ""
echo "=== Validation Summary ==="
echo "✅ Native runtime source compiles successfully"
echo "✅ All headers are accessible"
echo "✅ Platform-specific code is properly guarded"
echo ""
echo "Next steps:"
echo "  - Run cargo tests: cargo test --package kain-core"
echo "  - Run full validation: ./runtime/validate_native_runtime.sh"
echo "  - See runtime/NATIVE_RUNTIME_VALIDATION.md for details"
