#!/usr/bin/env bash
set -euo pipefail

SMOKE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$SMOKE_DIR/outputs"
ARTIFACT_DIR="$OUTPUT_DIR/generated"
SCREENSHOT_PATH="$OUTPUT_DIR/qt_plasma_runtime_lounge.png"

mkdir -p "$ARTIFACT_DIR"
rm -f "$SCREENSHOT_PATH" "$ARTIFACT_DIR/Main.qml" "$ARTIFACT_DIR/session.json"

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export QT_QUICK_BACKEND="${QT_QUICK_BACKEND:-software}"
export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"
export KAIN_UI_NATIVE_QT_RUNTIME="${KAIN_UI_NATIVE_QT_RUNTIME:-qml}"
export KAIN_UI_NATIVE_QT_ARTIFACT_DIR="$ARTIFACT_DIR"
export KAIN_UI_NATIVE_QT_SCREENSHOT_PATH="$SCREENSHOT_PATH"

cargo run --quiet --manifest-path "$SMOKE_DIR/native-app/Cargo.toml"

if [[ ! -f "$SCREENSHOT_PATH" ]]; then
    echo "expected screenshot was not generated: $SCREENSHOT_PATH" >&2
    exit 1
fi

echo "Qt smoke screenshot generated at $SCREENSHOT_PATH"
