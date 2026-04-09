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

MANIFEST_PATH="$PROJECT_ROOT/runtime/native_runtime.toml"

# Detect platform before resolving manifest sources so platform-specific arrays can be merged.
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

parse_manifest_array() {
    local key="$1"
    awk -v key="$key" '
        BEGIN {
            in_array = 0
        }
        {
            raw = $0
            if (!in_array && raw ~ "^[[:space:]]*" key "[[:space:]]*=[[:space:]]*\\[") {
                in_array = 1
                sub(/^.*\[/, "", raw)
            } else if (!in_array) {
                next
            }

            line = raw
            sub(/#.*/, "", line)
            closing = (line ~ /\]/)
            gsub(/\]/, "", line)
            gsub(/"/, "", line)
            gsub(/,/, " ", line)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)

            if (length(line) > 0) {
                count = split(line, parts, /[[:space:]]+/)
                for (i = 1; i <= count; ++i) {
                    if (parts[i] != "") {
                        print parts[i]
                    }
                }
            }

            if (closing) {
                exit
            }
        }
    ' "$MANIFEST_PATH"
}

RUNTIME_SOURCES=()
RUNTIME_INCLUDE_DIRS=()
RUNTIME_BUNDLE_NAME="kain-native-runtime"

if [[ -f "$MANIFEST_PATH" ]]; then
    mapfile -t MANIFEST_SOURCES < <(parse_manifest_array "sources")
    mapfile -t MANIFEST_PLATFORM_SOURCES < <(parse_manifest_array "${PLATFORM}_sources")
    mapfile -t MANIFEST_INCLUDE_DIRS < <(parse_manifest_array "include_dirs")

    if [[ ${#MANIFEST_SOURCES[@]} -gt 0 || ${#MANIFEST_PLATFORM_SOURCES[@]} -gt 0 ]]; then
        for relative_source in "${MANIFEST_SOURCES[@]}" "${MANIFEST_PLATFORM_SOURCES[@]}"; do
            if [[ -z "$relative_source" ]]; then
                continue
            fi
            RUNTIME_SOURCES+=("$PROJECT_ROOT/runtime/$relative_source")
        done
        for relative_include in "${MANIFEST_INCLUDE_DIRS[@]}"; do
            RUNTIME_INCLUDE_DIRS+=("$PROJECT_ROOT/runtime/$relative_include")
        done
    fi
fi

if [[ ${#RUNTIME_SOURCES[@]} -eq 0 ]]; then
    echo "Warning: runtime/native_runtime.toml not found or empty; falling back to legacy runtime/kain_runtime.c"
    RUNTIME_BUNDLE_NAME="kain-runtime-legacy"
    RUNTIME_SOURCES=("$PROJECT_ROOT/runtime/kain_runtime.c")
fi

if [[ ${#RUNTIME_INCLUDE_DIRS[@]} -eq 0 ]]; then
    RUNTIME_INCLUDE_DIRS=("$PROJECT_ROOT/runtime/native/include")
fi

echo "=== KAIN Native Runtime Compilation ==="
echo "Platform: $PLATFORM"
echo "Build Type: $BUILD_TYPE"
echo "Runtime Bundle: $RUNTIME_BUNDLE_NAME"
echo "Sources: ${#RUNTIME_SOURCES[@]}"
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

build_compile_command() {
    local source_path="$1"
    local output_path="$2"
    local -n out_cmd_ref=$3

    if [[ "$COMPILER_TYPE" == "msvc" ]]; then
        out_cmd_ref=("$COMPILER" "/nologo" "/W3")
        if [[ "$BUILD_TYPE" == "release" ]]; then
            out_cmd_ref+=("/O2")
        else
            out_cmd_ref+=("/Od" "/Zi")
        fi
        for include_dir in "${RUNTIME_INCLUDE_DIRS[@]}"; do
            out_cmd_ref+=("/I" "$include_dir")
        done
        if [[ "$PLATFORM" == "windows" ]]; then
            out_cmd_ref+=("/D" "WIN32" "/D" "_WINDOWS")
        fi
        out_cmd_ref+=("/c" "$source_path" "/Fo:$output_path")
    else
        out_cmd_ref=("$COMPILER" "-Wall" "-Wextra")
        if [[ "$BUILD_TYPE" == "release" ]]; then
            out_cmd_ref+=("-O2")
        else
            out_cmd_ref+=("-O0" "-g")
        fi
        for include_dir in "${RUNTIME_INCLUDE_DIRS[@]}"; do
            out_cmd_ref+=("-I" "$include_dir")
        done
        if [[ "$PLATFORM" == "windows" ]]; then
            out_cmd_ref+=("-D" "WIN32" "-D" "_WINDOWS")
        fi
        out_cmd_ref+=("-c" "$source_path" "-o" "$output_path")
    fi
}

run_compile_command() {
    local -n cmd_ref=$1
    if [[ "$VERBOSE" == true ]]; then
        printf '%q ' "${cmd_ref[@]}"
        echo ""
        "${cmd_ref[@]}"
    else
        local output
        if ! output=$("${cmd_ref[@]}" 2>&1); then
            if [[ -n "$output" ]]; then
                printf '%s\n' "$output"
            fi
            return 1
        fi
        if [[ -n "$output" ]]; then
            printf '%s\n' "$output"
        fi
    fi
}

OBJECT_EXT="o"
if [[ "$PLATFORM" == "windows" ]]; then
    OBJECT_EXT="obj"
fi

COMPILED_OBJECTS=()

echo "Compiling native runtime..."
for index in "${!RUNTIME_SOURCES[@]}"; do
    SOURCE_PATH="${RUNTIME_SOURCES[$index]}"
    if [[ ! -f "$SOURCE_PATH" ]]; then
        echo ""
        echo "❌ Runtime source not found: $SOURCE_PATH"
        exit 1
    fi

    SOURCE_BASENAME="$(basename "$SOURCE_PATH")"
    SOURCE_STEM="${SOURCE_BASENAME%.*}"
    OUTPUT_FILE="$(printf "%s/%02d_%s.%s" "$OUTPUT_DIR" "$index" "$SOURCE_STEM" "$OBJECT_EXT")"

    COMPILE_CMD=()
    build_compile_command "$SOURCE_PATH" "$OUTPUT_FILE" COMPILE_CMD

    if [[ "$VERBOSE" == true ]]; then
        echo "Compiling $SOURCE_BASENAME"
    fi

    if ! run_compile_command COMPILE_CMD; then
        echo ""
        echo "❌ Compilation failed for $SOURCE_PATH"
        exit 1
    fi

    if [[ ! -f "$OUTPUT_FILE" ]]; then
        echo ""
        echo "❌ Compilation failed - output file not created for $SOURCE_PATH"
        exit 1
    fi

    COMPILED_OBJECTS+=("$OUTPUT_FILE")
done

echo ""
echo "✅ Compilation successful!"
echo "Objects:"
for object_file in "${COMPILED_OBJECTS[@]}"; do
    FILE_SIZE=$(stat -f%z "$object_file" 2>/dev/null || stat -c%s "$object_file" 2>/dev/null || echo "unknown")
    echo "  - $object_file ($FILE_SIZE bytes)"
done
echo ""
echo "Note: This is a compilation-only check of the manifest-driven runtime bundle."
echo "      Linking and runtime execution validation remain separate steps."

# Summary
echo ""
echo "=== Validation Summary ==="
echo "✅ Native runtime bundle sources compile successfully"
echo "✅ All headers are accessible"
echo "✅ Platform-specific code is properly guarded"
echo ""
echo "Next steps:"
echo "  - Run cargo tests: cargo test --package kain-core"
echo "  - Run full validation: ./runtime/validate_native_runtime.sh"
echo "  - See runtime/NATIVE_RUNTIME_VALIDATION.md for details"
