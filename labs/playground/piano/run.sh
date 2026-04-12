#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$script_dir/native-app"
app_name="kain-piano"

if [[ ! -d "$project_dir" ]]; then
    "$script_dir/build.sh"
fi

launch_candidate="$(find "$project_dir" -maxdepth 3 -type f \( -name "$app_name" -o -name "$app_name.exe" \) -perm -111 2>/dev/null | sort | tail -n1)"

if [[ -z "$launch_candidate" ]]; then
    "$script_dir/build.sh"
    launch_candidate="$(find "$project_dir" -maxdepth 3 -type f \( -name "$app_name" -o -name "$app_name.exe" \) -perm -111 2>/dev/null | sort | tail -n1)"
fi

if [[ -z "$launch_candidate" ]]; then
    echo "Unable to find a piano app executable under $project_dir" >&2
    exit 1
fi

export LD_LIBRARY_PATH="$script_dir/native${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

cd "$script_dir"
printf 'Launching piano lab: %s\n' "$launch_candidate"
exec "$launch_candidate" "$@"
