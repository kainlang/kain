# Shared: runtime compile helpers for conformance tests.
# Source this at the top of any compile_tests.sh.
#
# Provides:
#   ALL_RUNTIME_SOURCES  - array of all core .c files for linking
#   RUNTIME_LDFLAGS      - platform-appropriate linker flags (no pthread on Windows)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFORMANCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RUNTIME_DIR="$(cd "$CONFORMANCE_DIR/.." && pwd)"
NATIVE_SRC="$RUNTIME_DIR/native/src"

# All core C files minus benchmarks
ALL_RUNTIME_SOURCES=()
while IFS= read -r -d '' f; do
    ALL_RUNTIME_SOURCES+=("$f")
done < <(find "$NATIVE_SRC/core" -maxdepth 1 -name "*.c" ! -name "*_benchmark*" -print0 | sort -z)

# Platform source
if [[ -f "$NATIVE_SRC/platform/platform.c" ]]; then
    ALL_RUNTIME_SOURCES+=("$NATIVE_SRC/platform/platform.c")
fi

# Platform-specific LDFLAGS
case "${OSTYPE:-}" in
    msys|cygwin|win32)
        RUNTIME_LDFLAGS="-lws2_32 -luser32 -lgdi32 -lole32 -lshell32"
        ;;
    darwin*)
        RUNTIME_LDFLAGS=""
        ;;
    *)
        RUNTIME_LDFLAGS="-lpthread -lm -ldl"
        ;;
esac
