#!/usr/bin/env bash
#
# build-target-runtime.sh — Cross-compile Kain's native C runtime for a target
#                           triple different from the host.
#
# Usage:
#   ./scripts/build-target-runtime.sh <target-triple>
#
# Examples:
#   ./scripts/build-target-runtime.sh x86_64-unknown-linux-gnu
#   ./scripts/build-target-runtime.sh aarch64-apple-darwin
#   ./scripts/build-target-runtime.sh x86_64-pc-windows-msvc
#
# The compiled static library is installed to:
#   ~/.kain/lib/<target-triple>/<runtime-lib-name>
#
# Requires: clang (must support -target <triple>), make, ar or llvm-ar
#
set -euo pipefail

# ── Help ────────────────────────────────────────────────────────────
if [ $# -lt 1 ]; then
    echo "Usage: $0 <target-triple>"
    echo ""
    echo "Examples:"
    echo "  $0 x86_64-unknown-linux-gnu"
    echo "  $0 aarch64-apple-darwin"
    echo "  $0 x86_64-pc-windows-msvc"
    exit 1
fi

TARGET="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUNTIME_DIR="$REPO_ROOT/runtime/native"
KAIN_HOME="${HOME}/.kain"

# ── Map target triple to platform parameters ────────────────────────
case "$TARGET" in
    *linux*)
        TARGET_OS="linux"
        TARGET_LIBS="-lpthread -ldl -lm"
        OUTPUT_NAME="libkain_runtime.a"
        ;;
    *windows*|*mingw*|*msvc*)
        TARGET_OS="windows"
        TARGET_LIBS="-luser32 -lgdi32 -lws2_32 -lwinhttp -ladvapi32 -lole32 -lshell32 -lwinmm"
        OUTPUT_NAME="kain_runtime.lib"
        ;;
    *darwin*|*apple*)
        TARGET_OS="darwin"
        TARGET_LIBS=""
        OUTPUT_NAME="libkain_runtime.a"
        ;;
    *)
        echo "Error: unknown or unsupported target triple: $TARGET"
        echo "Supported patterns: *linux*, *windows*, *darwin*, *apple*"
        exit 1
        ;;
esac

# ── Detect host triple ──────────────────────────────────────────────
detect_host_triple() {
    local os arch
    case "$(uname -s)" in
        Linux)  os="unknown-linux-gnu" ;;
        Darwin) os="apple-darwin" ;;
        MINGW*|MSYS*|CYGWIN*) os="pc-windows-msvc" ;;
        *)      echo "Error: cannot detect host OS"; exit 1 ;;
    esac
    case "$(uname -m)" in
        x86_64|amd64) arch="x86_64" ;;
        aarch64|arm64) arch="aarch64" ;;
        *)      echo "Error: cannot detect host architecture"; exit 1 ;;
    esac
    echo "${arch}-${os}"
}

HOST_TRIPLE="$(detect_host_triple)"

echo "═══════════════════════════════════════════════════════════════"
echo "  Kain Native Runtime — Cross-Compilation"
echo "═══════════════════════════════════════════════════════════════"
echo "  Host:   ${HOST_TRIPLE}"
echo "  Target: ${TARGET}"
echo "  OS:     ${TARGET_OS}"
echo "  Libs:   ${TARGET_LIBS}"
echo "  Output: ${OUTPUT_NAME}"
echo ""

# ── Build the static library ───────────────────────────────────────
cd "$RUNTIME_DIR"

echo "  [1/3] Cleaning previous build..."
make clean 2>/dev/null || true

echo "  [2/3] Cross-compiling for ${TARGET}..."
make cross-lib \
    TARGET="${TARGET}" \
    TARGET_OS="${TARGET_OS}" \
    TARGET_LIBS="${TARGET_LIBS}" \
    -j"$(nproc 2>/dev/null || echo 4)"

echo "  [3/3] Installing to ~/.kain/lib/${TARGET}/..."

# Locate the built library
STATIC_EXT=".a"
if [ "${TARGET_OS}" = "windows" ]; then
    STATIC_EXT=".lib"
fi

BUILT_LIB="_build/lib/libkain_runtime${STATIC_EXT}"
if [ "${TARGET_OS}" = "windows" ]; then
    BUILT_LIB="_build/lib/kain_runtime.lib"
fi

if [ ! -f "$BUILT_LIB" ]; then
    echo "Error: built library not found at ${BUILT_LIB}"
    echo "       Check the build output above for errors."
    exit 1
fi

# Install to per-triple directory
INSTALL_DIR="${KAIN_HOME}/lib/${TARGET}"
mkdir -p "$INSTALL_DIR"
cp "$BUILT_LIB" "${INSTALL_DIR}/${OUTPUT_NAME}"

LIB_SIZE="$(stat -c%s "$BUILT_LIB" 2>/dev/null || stat -f%z "$BUILT_LIB" 2>/dev/null || ls -la "$BUILT_LIB" | awk '{print $5}')"
echo "  ✓ Installed: ${INSTALL_DIR}/${OUTPUT_NAME} (${LIB_SIZE} bytes)"

# ── Verify the output is the right format ───────────────────────────
echo ""
echo "  [verify] Checking file type..."
if command -v file >/dev/null 2>&1; then
    file "${INSTALL_DIR}/${OUTPUT_NAME}"
fi

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "  ✓ Cross-compilation complete for ${TARGET}"
echo "  Library: ${INSTALL_DIR}/${OUTPUT_NAME}"
echo "═══════════════════════════════════════════════════════════════"
