#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$script_dir/native-app"
app_name="kain-piano"
user_runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

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

export XDG_RUNTIME_DIR="$user_runtime_dir"

if [[ -z "${WAYLAND_DISPLAY:-}" && -S "$user_runtime_dir/wayland-0" ]]; then
    export WAYLAND_DISPLAY="wayland-0"
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland}"
fi

if [[ -z "${DISPLAY:-}" && -S "/tmp/.X11-unix/X1" ]]; then
    export DISPLAY=":1"
fi

if [[ -z "${XAUTHORITY:-}" ]]; then
    xauth_candidate="$(ls -1t "$user_runtime_dir"/xauth_* 2>/dev/null | head -n1 || true)"
    if [[ -n "$xauth_candidate" ]]; then
        export XAUTHORITY="$xauth_candidate"
    fi
fi

export LD_LIBRARY_PATH="$script_dir/native${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

cd "$script_dir"
printf 'Launching piano lab: %s\n' "$launch_candidate"
exec "$launch_candidate" "$@"
