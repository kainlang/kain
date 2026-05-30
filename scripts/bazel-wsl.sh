#!/usr/bin/env bash
# ============================================================================
#  bazel-wsl.sh — Bazel wrapper for WSL that shares Windows caches
#  Usage:  ./scripts/bazel-wsl.sh build //:kain --config=dev
#          ./scripts/bazel-wsl.sh test //runtime:native_runtime_tests
# ============================================================================
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

exec bazel --bazelrc="$REPO_ROOT/.bazelrc" --bazelrc="$REPO_ROOT/.bazelrc.wsl" "$@"
