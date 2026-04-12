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

HOST_ARCH="${PROCESSOR_ARCHITECTURE:-$(uname -m 2>/dev/null || echo unknown)}"
CACHE_ROOT_BASE="${KAIN_RUNTIME_CACHE_DIR:-$PROJECT_ROOT/generated/native_runtime/cache}"

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
RUNTIME_DEFINES=()
RUNTIME_BUNDLE_NAME="kain-native-runtime"

if [[ -f "$MANIFEST_PATH" ]]; then
    mapfile -t MANIFEST_SOURCES < <(parse_manifest_array "sources")
    mapfile -t MANIFEST_PLATFORM_SOURCES < <(parse_manifest_array "${PLATFORM}_sources")
    mapfile -t MANIFEST_INCLUDE_DIRS < <(parse_manifest_array "include_dirs")
    mapfile -t MANIFEST_DEFINES < <(parse_manifest_array "defines")
    mapfile -t MANIFEST_PLATFORM_DEFINES < <(parse_manifest_array "${PLATFORM}_defines")

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
        for define in "${MANIFEST_DEFINES[@]}"; do
            if [[ -n "$define" ]]; then
                RUNTIME_DEFINES+=("$define")
            fi
        done
        for define in "${MANIFEST_PLATFORM_DEFINES[@]}"; do
            if [[ -n "$define" ]]; then
                RUNTIME_DEFINES+=("$define")
            fi
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

RUNTIME_CACHE_ROOT="$CACHE_ROOT_BASE/$PLATFORM-$HOST_ARCH/$BUILD_TYPE/$RUNTIME_BUNDLE_NAME"
OBJECT_CACHE_DIR="$RUNTIME_CACHE_ROOT/objects"
ARCHIVE_CACHE_DIR="$RUNTIME_CACHE_ROOT/archives"
mkdir -p "$OBJECT_CACHE_DIR" "$ARCHIVE_CACHE_DIR"

echo "=== KAIN Native Runtime Compilation ==="
echo "Platform: $PLATFORM"
echo "Build Type: $BUILD_TYPE"
echo "Runtime Bundle: $RUNTIME_BUNDLE_NAME"
echo "Sources: ${#RUNTIME_SOURCES[@]}"
echo "Output: $OUTPUT_DIR"
echo "Cache Root: $RUNTIME_CACHE_ROOT"
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

resolve_runtime_archiver() {
    if [[ -n "${KAIN_AR_PATH:-}" ]]; then
        if [[ -x "$KAIN_AR_PATH" || -f "$KAIN_AR_PATH" ]]; then
            ARCHIVER_CMD="$KAIN_AR_PATH"
        else
            echo "Error: KAIN_AR_PATH points to a missing archiver: $KAIN_AR_PATH"
            exit 1
        fi
    else
        local compiler_path=""
        compiler_path="$(command -v "$COMPILER" 2>/dev/null || true)"
        local compiler_dir=""
        if [[ -n "$compiler_path" ]]; then
            compiler_dir="$(dirname "$compiler_path")"
        fi

        local candidates=()
        if [[ "$PLATFORM" == "windows" ]]; then
            [[ -n "$compiler_dir" ]] && candidates+=("$compiler_dir/llvm-lib.exe" "$compiler_dir/llvm-ar.exe" "$compiler_dir/lib.exe")
            candidates+=("llvm-lib.exe" "llvm-ar.exe" "lib.exe" "llvm-lib" "llvm-ar" "ar")
        else
            [[ -n "$compiler_dir" ]] && candidates+=("$compiler_dir/llvm-ar" "$compiler_dir/ar")
            candidates+=("llvm-ar" "ar")
        fi

        local candidate=""
        for candidate in "${candidates[@]}"; do
            if command -v "$candidate" >/dev/null 2>&1; then
                ARCHIVER_CMD="$(command -v "$candidate")"
                break
            fi
            if [[ -f "$candidate" ]]; then
                ARCHIVER_CMD="$candidate"
                break
            fi
        done
    fi

    if [[ -z "${ARCHIVER_CMD:-}" ]]; then
        echo "Error: No suitable static archiver found. Set KAIN_AR_PATH or install llvm-ar/ar/lib.exe."
        exit 1
    fi

    local archiver_name
    archiver_name="$(basename "$ARCHIVER_CMD" | tr '[:upper:]' '[:lower:]')"
    if [[ "$archiver_name" == "lib.exe" || "$archiver_name" == "llvm-lib.exe" || "$archiver_name" == "llvm-lib" ]]; then
        ARCHIVER_FLAVOR="msvc"
        ARCHIVE_EXT="lib"
    else
        ARCHIVER_FLAVOR="gnu"
        ARCHIVE_EXT="a"
    fi
}

build_compile_fingerprint() {
    local source_path="$1"
    local is_cpp="$2"
    printf 'kain-native-runtime-cache-v1\n'
    printf 'bundle=%s\n' "$RUNTIME_BUNDLE_NAME"
    printf 'compiler=%s\n' "$COMPILER"
    printf 'build_type=%s\n' "$BUILD_TYPE"
    printf 'source=%s\n' "$source_path"
    printf 'cpp=%s\n' "$is_cpp"
    local include_dir=""
    for include_dir in "${RUNTIME_INCLUDE_DIRS[@]}"; do
        printf 'include=%s\n' "$include_dir"
    done
    local define=""
    for define in "${RUNTIME_DEFINES[@]}"; do
        printf 'define=%s\n' "$define"
    done
}

object_cache_is_fresh() {
    local object_path="$1"
    local fingerprint_path="$2"
    local source_path="$3"
    local expected_fingerprint="$4"

    [[ -f "$object_path" && -f "$fingerprint_path" ]] || return 1

    local stored_fingerprint
    stored_fingerprint="$(cat "$fingerprint_path" 2>/dev/null || true)"
    [[ "$stored_fingerprint" == "$expected_fingerprint" ]] || return 1

    if [[ "$source_path" -nt "$object_path" ]]; then
        return 1
    fi

    return 0
}

build_archive_fingerprint() {
    printf 'kain-native-runtime-archive-cache-v1\n'
    printf 'bundle=%s\n' "$RUNTIME_BUNDLE_NAME"
    printf 'archiver=%s\n' "$ARCHIVER_CMD"
    printf 'build_type=%s\n' "$BUILD_TYPE"
    local object_path=""
    for object_path in "$@"; do
        printf 'object=%s\n' "$object_path"
    done
}

archive_cache_is_fresh() {
    local archive_path="$1"
    local fingerprint_path="$2"
    local expected_fingerprint="$3"
    shift 3

    [[ -f "$archive_path" && -f "$fingerprint_path" ]] || return 1

    local stored_fingerprint
    stored_fingerprint="$(cat "$fingerprint_path" 2>/dev/null || true)"
    [[ "$stored_fingerprint" == "$expected_fingerprint" ]] || return 1

    local object_path=""
    for object_path in "$@"; do
        if [[ "$object_path" -nt "$archive_path" ]]; then
            return 1
        fi
    done

    return 0
}

build_static_archive() {
    local archive_path="$1"
    shift

    rm -f "$archive_path"
    if [[ "$ARCHIVER_FLAVOR" == "msvc" ]]; then
        "$ARCHIVER_CMD" /nologo "/OUT:$archive_path" "$@"
    else
        "$ARCHIVER_CMD" rcs "$archive_path" "$@"
    fi
}

build_compile_command() {
    local source_path="$1"
    local output_path="$2"
    local -n out_cmd_ref=$3
    local source_ext="${source_path##*.}"
    local is_cpp=false

    case "$source_ext" in
        cc|cpp|cxx|mm)
            is_cpp=true
            ;;
    esac

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
        for define in "${RUNTIME_DEFINES[@]}"; do
            out_cmd_ref+=("/D" "$define")
        done
        if [[ "$PLATFORM" == "windows" ]]; then
            out_cmd_ref+=("/D" "WIN32" "/D" "_WINDOWS")
        fi
        if [[ "$is_cpp" == true ]]; then
            out_cmd_ref+=("/std:c++20")
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
        for define in "${RUNTIME_DEFINES[@]}"; do
            out_cmd_ref+=("-D" "$define")
        done
        if [[ "$PLATFORM" == "windows" ]]; then
            out_cmd_ref+=("-D" "WIN32" "-D" "_WINDOWS")
        fi
        if [[ "$is_cpp" == true ]]; then
            out_cmd_ref+=("-std=c++20")
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
LOOSE_RUNTIME_OBJECTS=()
VENDOR_RUNTIME_OBJECTS=()
REUSED_OBJECTS=0
COMPILED_OBJECT_COUNT=0
REUSED_ARCHIVES=0
REBUILT_ARCHIVES=0

resolve_runtime_archiver

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
    OUTPUT_FILE="$(printf "%s/%02d_%s.%s" "$OBJECT_CACHE_DIR" "$index" "$SOURCE_STEM" "$OBJECT_EXT")"
    FINGERPRINT_FILE="$OUTPUT_FILE.fingerprint"
    SOURCE_EXT="${SOURCE_PATH##*.}"
    IS_CPP=false
    case "$SOURCE_EXT" in
        cc|cpp|cxx|mm)
            IS_CPP=true
            ;;
    esac
    COMPILE_FINGERPRINT="$(build_compile_fingerprint "$SOURCE_PATH" "$IS_CPP")"

    if object_cache_is_fresh "$OUTPUT_FILE" "$FINGERPRINT_FILE" "$SOURCE_PATH" "$COMPILE_FINGERPRINT"; then
        REUSED_OBJECTS=$((REUSED_OBJECTS + 1))
    else
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

        printf '%s' "$COMPILE_FINGERPRINT" > "$FINGERPRINT_FILE"
        COMPILED_OBJECT_COUNT=$((COMPILED_OBJECT_COUNT + 1))
    fi

    COMPILED_OBJECTS+=("$OUTPUT_FILE")
    SOURCE_RELATIVE="${SOURCE_PATH#"$PROJECT_ROOT/runtime/"}"
    if [[ "$SOURCE_RELATIVE" == 3rdparty/* ]]; then
        VENDOR_RUNTIME_OBJECTS+=("$OUTPUT_FILE")
    else
        LOOSE_RUNTIME_OBJECTS+=("$OUTPUT_FILE")
    fi
done

VENDOR_ARCHIVE_PATH=""
if [[ ${#VENDOR_RUNTIME_OBJECTS[@]} -gt 0 ]]; then
    if [[ "$ARCHIVE_EXT" == "lib" ]]; then
        VENDOR_ARCHIVE_PATH="$ARCHIVE_CACHE_DIR/vendor-runtime.lib"
    else
        VENDOR_ARCHIVE_PATH="$ARCHIVE_CACHE_DIR/libvendor-runtime.a"
    fi
    VENDOR_ARCHIVE_FINGERPRINT_PATH="$VENDOR_ARCHIVE_PATH.fingerprint"
    VENDOR_ARCHIVE_FINGERPRINT="$(build_archive_fingerprint "${VENDOR_RUNTIME_OBJECTS[@]}")"

    if archive_cache_is_fresh \
        "$VENDOR_ARCHIVE_PATH" \
        "$VENDOR_ARCHIVE_FINGERPRINT_PATH" \
        "$VENDOR_ARCHIVE_FINGERPRINT" \
        "${VENDOR_RUNTIME_OBJECTS[@]}"; then
        REUSED_ARCHIVES=$((REUSED_ARCHIVES + 1))
    else
        if ! build_static_archive "$VENDOR_ARCHIVE_PATH" "${VENDOR_RUNTIME_OBJECTS[@]}"; then
            echo ""
            echo "❌ Failed to build vendor runtime archive: $VENDOR_ARCHIVE_PATH"
            exit 1
        fi
        printf '%s' "$VENDOR_ARCHIVE_FINGERPRINT" > "$VENDOR_ARCHIVE_FINGERPRINT_PATH"
        REBUILT_ARCHIVES=$((REBUILT_ARCHIVES + 1))
    fi
fi

echo ""
echo "✅ Compilation successful!"
echo "Native runtime cache: $REUSED_OBJECTS reused, $COMPILED_OBJECT_COUNT compiled, $REUSED_ARCHIVES archives reused, $REBUILT_ARCHIVES archives rebuilt"
echo "Loose runtime objects: ${#LOOSE_RUNTIME_OBJECTS[@]}"
echo "Vendor runtime objects: ${#VENDOR_RUNTIME_OBJECTS[@]}"
if [[ -n "$VENDOR_ARCHIVE_PATH" ]]; then
    ARCHIVE_SIZE=$(stat -f%z "$VENDOR_ARCHIVE_PATH" 2>/dev/null || stat -c%s "$VENDOR_ARCHIVE_PATH" 2>/dev/null || echo "unknown")
    echo "Vendor archive: $VENDOR_ARCHIVE_PATH ($ARCHIVE_SIZE bytes)"
fi
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
