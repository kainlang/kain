#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
lab_root="$script_dir"
project_dir="$lab_root/native-app"
app_name="kain-piano"
window_title="Kain Piano Lab"
shared_lib="$lab_root/native/libpiano_audio.so"
source_file="$lab_root/native/piano_audio.c"
miniaudio_include="$repo_root/runtime/thirdparty/miniaudio"

mkdir -p "$lab_root/native" "$lab_root/native/piano_cache"

clang -shared -O2 -fPIC \
    -I"$miniaudio_include" \
    "$source_file" \
    -o "$shared_lib" \
    -ldl -lpthread -lm

(
    cd "$lab_root"
    cargo run -q --manifest-path "$repo_root/Cargo.toml" -p cli --bin kain -- build native-ui "src/main.kn" \
        --app-name "$app_name" \
        --window-title "$window_title" \
        -o "$project_dir"
)

launch_candidate="$(find "$project_dir" -maxdepth 3 -type f \( -name "$app_name" -o -name "$app_name.exe" \) -perm -111 2>/dev/null | sort | head -n1)"

if [[ -z "$launch_candidate" ]]; then
    launch_candidate="$(find "$project_dir" -maxdepth 3 -type f -perm -111 2>/dev/null | sort | head -n1)"
fi

if [[ -z "$launch_candidate" ]]; then
    echo "Unable to find a built piano app executable under $project_dir" >&2
    exit 1
fi

printf 'Native app ready: %s\n' "$launch_candidate"
