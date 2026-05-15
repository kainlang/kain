#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_PATH="${APP_ROOT}/native/libdcc_suite_ops.so"
SOURCE_PATH="${APP_ROOT}/native/dcc_suite_ops.c"

clang -shared -fPIC -O2 -o "${OUTPUT_PATH}" "${SOURCE_PATH}"
echo "Built ${OUTPUT_PATH}"
