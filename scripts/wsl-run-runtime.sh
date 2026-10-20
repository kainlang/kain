#!/bin/bash
# WSL-side Kain RUNTIME-ONLY build (fast iteration: no cargo, no install).
exec > /tmp/wsl-rtbuild.log 2>&1
set -x
export PATH="$HOME/.cargo/bin:$PATH"
export LIBCLANG_PATH="/usr/lib/llvm-21/lib"
cd /mnt/t/toolchain/Kain || exit 99
python3 '!_DUMB_BUILD_KAIN_COMPILER.py' --yes --skip-compiler --skip-install --skip-verify --no-path
echo "WSL_RTBUILD_EXIT:$?"
