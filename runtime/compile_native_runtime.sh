#!/usr/bin/env bash
# Compile the manifest-declared native runtime sources as a standalone bundle.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_TYPE="debug"
VERBOSE=0
MANIFEST_PATH="$SCRIPT_DIR/native_core_runtime.toml"

usage() {
    cat <<'EOF'
Usage: ./runtime/compile_native_runtime.sh [OPTIONS]

Compile the manifest-driven native runtime bundle.

Options:
  --release          Use optimized object compilation
  --verbose          Print each compiler command
  --manifest PATH    Override runtime manifest path
  --help             Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release)
            BUILD_TYPE="release"
            shift
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --manifest)
            if [[ $# -lt 2 ]]; then
                echo "error: --manifest requires a path" >&2
                exit 1
            fi
            MANIFEST_PATH="$2"
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ ! -f "$MANIFEST_PATH" ]]; then
    echo "error: runtime manifest not found: $MANIFEST_PATH" >&2
    exit 1
fi

case "${OSTYPE:-}" in
    msys*|win32*|cygwin*)
        PLATFORM="windows"
        ;;
    linux-gnu*)
        PLATFORM="linux"
        ;;
    darwin*)
        PLATFORM="macos"
        ;;
    *)
        echo "error: unsupported platform: ${OSTYPE:-unknown}" >&2
        exit 1
        ;;
esac

parse_manifest_array() {
    local key="$1"
    awk -v key="$key" '
        BEGIN { in_array = 0 }
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

mapfile -t BASE_SOURCES < <(parse_manifest_array "sources")
mapfile -t PLATFORM_SOURCES < <(parse_manifest_array "${PLATFORM}_sources")
mapfile -t INCLUDE_DIRS < <(parse_manifest_array "include_dirs")
mapfile -t BASE_DEFINES < <(parse_manifest_array "defines")
mapfile -t PLATFORM_DEFINES < <(parse_manifest_array "${PLATFORM}_defines")

if [[ ${#BASE_SOURCES[@]} -eq 0 && ${#PLATFORM_SOURCES[@]} -eq 0 ]]; then
    echo "error: manifest has no runtime sources: $MANIFEST_PATH" >&2
    exit 1
fi

if [[ ${#INCLUDE_DIRS[@]} -eq 0 ]]; then
    INCLUDE_DIRS=("native/include")
fi

if command -v clang >/dev/null 2>&1; then
    COMPILER="$(command -v clang)"
elif command -v gcc >/dev/null 2>&1; then
    COMPILER="$(command -v gcc)"
else
    echo "error: no C compiler found; install clang or gcc" >&2
    exit 1
fi

OBJECT_EXT="o"
if [[ "$PLATFORM" == "windows" ]]; then
    OBJECT_EXT="obj"
fi

HOST_ARCH="${PROCESSOR_ARCHITECTURE:-$(uname -m 2>/dev/null || echo unknown)}"
OUTPUT_DIR="${KAIN_RUNTIME_OUTPUT_DIR:-$PROJECT_ROOT/generated/native_runtime/$BUILD_TYPE}"
OBJECT_DIR="${KAIN_RUNTIME_CACHE_DIR:-$PROJECT_ROOT/generated/native_runtime/cache}/$PLATFORM-$HOST_ARCH/$BUILD_TYPE/objects"
mkdir -p "$OUTPUT_DIR" "$OBJECT_DIR"

COMPILE_FLAGS=("-Wall" "-Wextra")
if [[ "$BUILD_TYPE" == "release" ]]; then
    COMPILE_FLAGS+=("-O2" "-DNDEBUG")
else
    COMPILE_FLAGS+=("-O0" "-g")
fi

if [[ "$PLATFORM" == "windows" ]]; then
    COMPILE_FLAGS+=("-DWIN32" "-D_WINDOWS")
fi

for include_dir in "${INCLUDE_DIRS[@]}"; do
    COMPILE_FLAGS+=("-I" "$SCRIPT_DIR/$include_dir")
done

for define in "${BASE_DEFINES[@]}" "${PLATFORM_DEFINES[@]}"; do
    if [[ -n "$define" ]]; then
        COMPILE_FLAGS+=("-D$define")
    fi
done

echo "=== KAIN Native Runtime Compilation ==="
echo "Manifest: $MANIFEST_PATH"
echo "Platform: $PLATFORM"
echo "Build Type: $BUILD_TYPE"
echo "Compiler: $COMPILER"
echo "Output: $OUTPUT_DIR"
echo "Object Cache: $OBJECT_DIR"
echo ""

compiled=0
for index in "${!BASE_SOURCES[@]}"; do
    relative_source="${BASE_SOURCES[$index]}"
    source_path="$SCRIPT_DIR/$relative_source"
    if [[ ! -f "$source_path" ]]; then
        echo "error: runtime source not found: $source_path" >&2
        exit 1
    fi
    source_name="$(basename "$source_path")"
    object_path="$(printf "%s/%03d_%s.%s" "$OBJECT_DIR" "$index" "${source_name%.*}" "$OBJECT_EXT")"
    cmd=("$COMPILER" "${COMPILE_FLAGS[@]}" "-c" "$source_path" "-o" "$object_path")
    if [[ "$VERBOSE" -eq 1 ]]; then
        printf '%q ' "${cmd[@]}"
        echo ""
    fi
    "${cmd[@]}"
    compiled=$((compiled + 1))
done

base_count=${#BASE_SOURCES[@]}
for platform_index in "${!PLATFORM_SOURCES[@]}"; do
    relative_source="${PLATFORM_SOURCES[$platform_index]}"
    source_path="$SCRIPT_DIR/$relative_source"
    if [[ ! -f "$source_path" ]]; then
        echo "error: platform runtime source not found: $source_path" >&2
        exit 1
    fi
    source_name="$(basename "$source_path")"
    object_index=$((base_count + platform_index))
    object_path="$(printf "%s/%03d_%s.%s" "$OBJECT_DIR" "$object_index" "${source_name%.*}" "$OBJECT_EXT")"
    cmd=("$COMPILER" "${COMPILE_FLAGS[@]}" "-c" "$source_path" "-o" "$object_path")
    if [[ "$VERBOSE" -eq 1 ]]; then
        printf '%q ' "${cmd[@]}"
        echo ""
    fi
    "${cmd[@]}"
    compiled=$((compiled + 1))
done

echo ""
echo "Compilation successful."
echo "Compiled runtime objects: $compiled"
echo "Standalone runtime bundle objects live under: $OBJECT_DIR"
