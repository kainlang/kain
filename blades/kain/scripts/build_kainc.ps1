# build_kainc.ps1 — Build the Kain Self-Host Compiler (kainc.exe)
# Usage: .\scripts\build_kainc.ps1
#
# This script uses the Rust bootstrap compiler (kain build) to compile
# the self-host Kain compiler sources into a native executable.
#
# Output: kainc.exe at the project root

param(
    [string]$Target = "llvm",
    [string]$Profile = "debug"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot | Split-Path -Parent
Set-Location $ProjectRoot

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " BUILD kainc (Kain Self-Host Compiler)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Target:  $Target" -ForegroundColor Gray
Write-Host "Profile: $Profile" -ForegroundColor Gray
Write-Host "Root:    $ProjectRoot" -ForegroundColor Gray
Write-Host ""

# Build with the Rust bootstrap compiler
Write-Host "[1/2] Compiling with kain build..." -ForegroundColor Yellow
$buildArgs = @("build", ".", "--target", $Target)
$buildResult = & kain @buildArgs 2>&1
$buildExit = $LASTEXITCODE

if ($buildExit -ne 0) {
    Write-Host "" -ForegroundColor Red
    Write-Host "BUILD FAILED (exit code: $buildExit)" -ForegroundColor Red
    Write-Host $buildResult -ForegroundColor Red
    exit $buildExit
}

Write-Host "[1/2] Build SUCCESS" -ForegroundColor Green
Write-Host ""

# Locate the built kainc.exe
Write-Host "[2/2] Locating kainc.exe..." -ForegroundColor Yellow

$exePaths = Get-ChildItem -Path $ProjectRoot -Recurse -Filter "kainc.exe" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notlike "*\.kain\*" -and $_.FullName -notlike "*\target\*" } |
    Sort-Object LastWriteTime -Descending

if ($exePaths.Count -eq 0) {
    Write-Host "ERROR: kainc.exe not found after build" -ForegroundColor Red
    Write-Host "Check .kain/out/ directory for the output binary" -ForegroundColor Yellow
    exit 1
}

$freshest = $exePaths[0]
Write-Host "Found: $($freshest.FullName)" -ForegroundColor Gray

# Copy to project root
$destPath = Join-Path $ProjectRoot "kainc.exe"
if ($freshest.FullName -ne $destPath) {
    Copy-Item -Path $freshest.FullName -Destination $destPath -Force
    Write-Host "Copied to: $destPath" -ForegroundColor Green
}
else {
    Write-Host "Already at: $destPath" -ForegroundColor Green
}
Write-Host ""

Write-Host "========================================" -ForegroundColor Green
Write-Host " BUILD COMPLETE" -ForegroundColor Green
Write-Host " kainc.exe ready at: $destPath" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
