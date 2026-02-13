#!/usr/bin/env pwsh
# Quick alias for cargo build --release with auto-install
# Usage: .\cb.ps1

& "$PSScriptRoot\cargo-build-install.ps1" --release
