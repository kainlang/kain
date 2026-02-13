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
    Write-Host "Auto-installing to cargo bin..." -ForegroundColor Cyan
    
    $source = "target\release\kain.exe"
    $dest = "$env:USERPROFILE\.cargo\bin\kain.exe"
    
    if (Test-Path $source) {
        try {
            Copy-Item -Path $source -Destination $dest -Force -ErrorAction Stop
            Write-Host "Installed to cargo bin" -ForegroundColor Green
        }
        catch {
            Write-Host "Could not install (file may be in use)" -ForegroundColor Yellow
        }
    }
    else {
        Write-Host "Binary not found: $source" -ForegroundColor Yellow
    }
}
