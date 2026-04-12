#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
lab_root="$script_dir"
generated_dir="$lab_root/generated"
app_name="piano"
shared_lib="$lab_root/native/libpiano_audio.so"
source_file="$lab_root/native/piano_audio.c"
miniaudio_include="$repo_root/runtime/thirdparty/miniaudio"
llvm_output="$generated_dir/$app_name.ll"
binary_path="$generated_dir/$app_name"

mkdir -p "$lab_root/native" "$lab_root/native/piano_cache" "$generated_dir"

clang -shared -O2 -fPIC \
    -I"$miniaudio_include" \
    "$source_file" \
    -o "$shared_lib" \
    -ldl -lpthread -lm

(
    cd "$repo_root"
    cargo run -q --manifest-path "$repo_root/Cargo.toml" -p cli --bin kain -- build "$lab_root/src/main.kn" \
        --target llvm \
        --output "$llvm_output"
)

if [[ ! -x "$binary_path" && ! -x "$binary_path.exe" ]]; then
    echo "Unable to find a built piano executable under $generated_dir" >&2
    exit 1
fi

printf 'LLVM app ready: %s\n' "$binary_path"
