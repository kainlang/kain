#!/bin/bash
# WSL toolchain setup for Kain dumb build (Ubuntu 22.04 jammy)
# Installs: build-essential, LLVM 21, Rust stable
set -x
export DEBIAN_FRONTEND=noninteractive

echo "=== [1/5] apt update ==="
sudo apt-get update -y

echo "=== [2/5] base tools ==="
sudo apt-get install -y wget lsb-release software-properties-common gnupg curl git build-essential python3 binutils pkg-config openssl libssl-dev libasound2-dev

echo "=== [3/5] LLVM 21 repo ==="
if [ ! -f /usr/bin/clang-21 ] && [ ! -f /usr/lib/llvm-21/bin/llvm-config ]; then
  wget -qO - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add - 2>&1 | tail -n 2
  sudo add-apt-repository "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-21 main" -y
  sudo apt-get update -y
  sudo apt-get install -y clang-21 lld-21 llvm-21 llvm-21-dev libclang-21-dev clangd-21 2>&1 | tail -n 5
else
  echo "LLVM 21 already present, skipping repo add"
fi

echo "=== [4/5] clang alternatives ==="
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-21 100 2>/dev/null || true
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/lib/llvm-21/bin/llvm-config 100 2>/dev/null || true
which clang-21 llvm-config clang 2>&1
clang-21 --version 2>&1 | head -n 3
llvm-config --version 2>&1 | head -n 2
ls /usr/lib/llvm-21/lib/libclang.so* 2>&1 | head

echo "=== [5/5] rustup ==="
if [ ! -x "$HOME/.cargo/bin/cargo" ]; then
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable --profile minimal
else
  echo "cargo already present: $($HOME/.cargo/bin/cargo --version)"
fi
export PATH="$HOME/.cargo/bin:$PATH"
cargo --version
rustc --version

echo "=== SETUP DONE ==="
