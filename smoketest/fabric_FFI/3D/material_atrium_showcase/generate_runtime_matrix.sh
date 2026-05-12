#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

cd "$REPO_ROOT"

cargo run -p kain-3d --bin generic_scene_smoke -- \
  --output-image "$SCRIPT_DIR/generic_scene_visual_reference.png" \
  --output-json "$SCRIPT_DIR/generated/generic_scene_runtime_report.json"
