#!/bin/bash
# WSL-side Kain dumb build (runs INSIDE WSL, ext4-friendly logging)
exec > /tmp/wsl-build.log 2>&1
set -x
export PATH="$HOME/.cargo/bin:$PATH"
export LIBCLANG_PATH="/usr/lib/llvm-21/lib"
cd /mnt/t/toolchain/Kain || exit 99
python3 '!_DUMB_BUILD_KAIN_COMPILER.py' --yes
echo "WSL_BUILD_EXIT:$?"
