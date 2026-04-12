#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

cd "$REPO_ROOT"

cargo run -p kain-3d --bin material_atrium_smoke -- \
  --output-image "$SCRIPT_DIR/material_atrium_visual_example.png" \
  --output-json "$SCRIPT_DIR/generated/material_atrium_runtime_matrix.json"
