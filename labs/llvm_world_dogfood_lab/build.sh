#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
lab_root="$script_dir"
generated_dir="$lab_root/generated"
app_name="llvm_world_dogfood_lab"
llvm_output="$generated_dir/$app_name.ll"
binary_path="$generated_dir/$app_name"

mkdir -p "$generated_dir"

(
    cd "$lab_root"
    cargo run -q --manifest-path "$repo_root/Cargo.toml" -p cli --bin kain -- build "src/main.kn" \
        --target llvm \
        --output "$llvm_output"
)

if [[ ! -x "$binary_path" && ! -x "$binary_path.exe" ]]; then
    echo "Unable to find a built llvm_world_dogfood_lab executable under $generated_dir" >&2
    exit 1
fi

printf 'LLVM dogfood app ready: %s\n' "$binary_path"
