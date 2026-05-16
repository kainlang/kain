#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
generated_dir="$script_dir/generated"
app_name="piano"
binary_path="$generated_dir/$app_name"
user_runtime_dir="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

if [[ ! -x "$binary_path" && ! -x "$binary_path.exe" ]]; then
    "$script_dir/build.sh"
fi

if [[ ! -x "$binary_path" && ! -x "$binary_path.exe" ]]; then
    echo "Unable to find a piano executable under $generated_dir" >&2
    exit 1
fi

launch_candidate="$binary_path"
if [[ -x "$binary_path.exe" ]]; then
    launch_candidate="$binary_path.exe"
fi

binary_base="$launch_candidate"
if [[ "$launch_candidate" == *.exe ]]; then
    binary_base="${launch_candidate%.exe}"
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

export RUNTIME_CONTRACT="${RUNTIME_CONTRACT:-$binary_base.runtime_contract.json}"
export KAIN_REALTIME_APP_BUNDLE="${KAIN_REALTIME_APP_BUNDLE:-$binary_base.realtime_app.json}"
export CONTRACT_STRICT="${CONTRACT_STRICT:-1}"
export LD_LIBRARY_PATH="$script_dir/native${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

cd "$script_dir"
printf 'Launching piano lab: %s\n' "$launch_candidate"
exec "$launch_candidate" "$@"
