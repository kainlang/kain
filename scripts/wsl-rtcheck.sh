#!/bin/bash
# Pre-flight: compile every Linux runtime source with dumb-build flags.
exec > /tmp/rtcheck.log 2>&1
set -u
cd /mnt/t/toolchain/Kain/runtime/native || exit 99
OUT=/tmp/rtcheck
rm -rf "$OUT"
mkdir -p "$OUT/objs"
FAILS="$OUT/fails.txt"
touch "$FAILS"
compile_one() {
  src="$1"
  n="$(basename "$src" .c)"
  if ! clang-21 -std=c11 -O2 -D_GNU_SOURCE -fPIC -pthread \
      -Iinclude -Isrc/core -Isrc/platform -g \
      -c "$src" -o "$OUT/objs/$n.o" 2>"$OUT/$n.err"; then
    echo "$src" >> "$FAILS"
    tail -n 5 "$OUT/$n.err"
  fi
}
export -f compile_one 2>/dev/null || true
for f in src/core/*.c src/platform/platform.c src/platform/linux/linux_shared.c; do
  case "$f" in
    *_benchmark.c|python_runtime*|cuda_runtime*) continue;;
  esac
  compile_one "$f" &
  # throttle to 16 parallel
  while [ "$(jobs -r | wc -l)" -ge 16 ]; do sleep 0.2; done
done
wait
echo "=== FAILS ==="
cat "$FAILS"
echo "=== objects: $(ls "$OUT"/objs/*.o 2>/dev/null | wc -l) ==="
