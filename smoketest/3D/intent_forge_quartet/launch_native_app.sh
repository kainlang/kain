#!/usr/bin/env bash
set -euo pipefail

SMOKE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SMOKE_ROOT}/../../.." && pwd)"

python3 "${SMOKE_ROOT}/run_smoke.py"

EXE_PATH="$(find "${SMOKE_ROOT}/native-app" -maxdepth 1 -type f -perm -111 | head -n 1)"
if [[ -z "${EXE_PATH}" ]]; then
  echo "Unable to resolve the built native executable." >&2
  exit 1
fi

exec "${EXE_PATH}"
