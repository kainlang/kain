#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXE_PATH="$SCRIPT_DIR/native-app/material-atrium-showcase"
BACKEND_ID="${1:-}"

if [[ ! -x "$EXE_PATH" ]]; then
  echo "Expected executable was not found at $EXE_PATH"
  echo "Build it first with:"
  echo "  cargo run -p cli --bin kain -- build native-ui smoketest/3D/material_atrium_showcase/smoke.kn --app-name material-atrium-showcase --window-title \"Kain Material Atrium Showcase\" -o smoketest/3D/material_atrium_showcase/native-app"
  exit 1
fi

if [[ -n "$BACKEND_ID" ]]; then
  export KAIN_RUNTIME_RENDERER_BACKEND="$BACKEND_ID"
fi

exec "$EXE_PATH"
