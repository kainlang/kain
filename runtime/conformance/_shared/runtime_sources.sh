# Shared: discovers all runtime core source files for linking.
# Sourced by conformance test compile scripts so they never miss dependencies.
#
# Usage:
#   source "$(dirname "${BASH_SOURCE[0]}")/../../_shared/runtime_sources.sh"
#   ALL_RUNTIME_SOURCES  # expands to space-separated paths

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
NATIVE_SRC="$RUNTIME_DIR/native/src"

# All core C files for the runtime (excludes benchmarks and platform-specific)
ALL_RUNTIME_SOURCES=()
while IFS= read -r -d '' f; do
    ALL_RUNTIME_SOURCES+=("$f")
done < <(find "$NATIVE_SRC/core" -maxdepth 1 -name "*.c" ! -name "*_benchmark*" -print0 | sort -z)

# Platform layer (only the portable platform.c)
if [[ -f "$NATIVE_SRC/platform/platform.c" ]]; then
    ALL_RUNTIME_SOURCES+=("$NATIVE_SRC/platform/platform.c")
fi
