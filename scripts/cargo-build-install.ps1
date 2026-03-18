#!/usr/bin/env pwsh
# Wrapper for cargo build that auto-installs on release builds
# Usage: .\cargo-build-install.ps1 [cargo build args...]

$ErrorActionPreference = "Stop"

# Pass all arguments to cargo build
Write-Host "Building..." -ForegroundColor Cyan
cargo build $args

# Check if build succeeded
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Build complete" -ForegroundColor Green

# Auto-install if this was a release build
if ($args -contains "--release" -or $args -contains "-r") {
    $syncScript = Join-Path $PSScriptRoot "sync-kain-source-of-truth.ps1"
    if (Test-Path $syncScript) {
        Write-Host "Refreshing canonical PATH binary..." -ForegroundColor Cyan
        & $syncScript -SkipBuild
    }
    else {
        Write-Host "Sync script not found: $syncScript" -ForegroundColor Yellow
    }
}
