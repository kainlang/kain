Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($repoRoot)) {
    Write-Error "Unable to resolve repository root."
}

$hooksPath = Join-Path $repoRoot ".githooks"
if (-not (Test-Path $hooksPath)) {
    Write-Error "Expected hooks folder does not exist: $hooksPath"
}

git config core.hooksPath ".githooks"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to set core.hooksPath."
}

Write-Host "Git hooks path configured to .githooks"
Write-Host "Pre-commit hook now runs scripts/windows/check-stale-artifacts.ps1"
