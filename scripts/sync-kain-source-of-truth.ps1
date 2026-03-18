#!/usr/bin/env pwsh
param(
    [switch]$PersistUserEnv,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Set-PathPrefix {
    param(
        [Parameter(Mandatory = $true)][string]$PathValue,
        [Parameter(Mandatory = $true)][string]$Prefix
    )

    $parts = @()
    foreach ($part in ($PathValue -split ';')) {
        if (-not [string]::IsNullOrWhiteSpace($part) -and $part -ne $Prefix) {
            $parts += $part
        }
    }

    if ($parts.Count -eq 0) {
        return $Prefix
    }

    return ($Prefix + ';' + ($parts -join ';'))
}

function Set-UserPathPrefix {
    param([Parameter(Mandatory = $true)][string]$Prefix)

    $currentUserPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ([string]::IsNullOrWhiteSpace($currentUserPath)) {
        $currentUserPath = ""
    }

    $nextUserPath = Set-PathPrefix -PathValue $currentUserPath -Prefix $Prefix
    [Environment]::SetEnvironmentVariable("Path", $nextUserPath, "User")
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$installRoot = if ($env:CARGO_HOME) {
    $env:CARGO_HOME
} else {
    Join-Path $env:USERPROFILE ".cargo"
}
$installDir = Join-Path $installRoot "bin"
$sourceExe = Join-Path $repoRoot "target\release\kain.exe"
$destExe = Join-Path $installDir "kain.exe"

$resourceMap = [ordered]@{
    "KAIN_STDLIB_PATH" = (Join-Path $repoRoot "stdlib")
    "KAIN_RUNTIME_C_PATH" = (Join-Path $repoRoot "runtime\kain_runtime.c")
    "KAIN_RUNTIME_MANIFEST_PATH" = (Join-Path $repoRoot "runtime\native_runtime.toml")
    "KAIN_CLANG_PATH" = (Join-Path $repoRoot "toolchain\llvm\bin\clang.exe")
}

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Syncing KAIN source of truth" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Repo Root : $repoRoot"
Write-Host "Install   : $destExe"
Write-Host

Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        Write-Host "[1/4] Building crates/cli in release mode..." -ForegroundColor Cyan
        cargo build --release -p cli
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build --release -p cli failed with exit code $LASTEXITCODE"
        }
    } else {
        Write-Host "[1/4] Skipping build (using existing release binary)..." -ForegroundColor Yellow
    }

    if (-not (Test-Path $sourceExe)) {
        throw "Release binary not found at $sourceExe"
    }

    Write-Host "[2/4] Installing stable PATH binary..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
    Copy-Item -Path $sourceExe -Destination $destExe -Force

    Write-Host "[3/4] Applying KAIN resource roots to this session..." -ForegroundColor Cyan
    foreach ($entry in $resourceMap.GetEnumerator()) {
        if (Test-Path $entry.Value) {
            Set-Item -Path ("Env:" + $entry.Key) -Value $entry.Value
            Write-Host ("  [set] {0}={1}" -f $entry.Key, $entry.Value)
        } else {
            Write-Host ("  [warn] Skipping {0}; path not found: {1}" -f $entry.Key, $entry.Value) -ForegroundColor Yellow
        }
    }

    $env:PATH = Set-PathPrefix -PathValue $env:PATH -Prefix $installDir

    if ($PersistUserEnv) {
        Write-Host "[4/4] Persisting PATH and KAIN environment variables for future shells..." -ForegroundColor Cyan
        foreach ($entry in $resourceMap.GetEnumerator()) {
            if (Test-Path $entry.Value) {
                [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "User")
            }
        }
        Set-UserPathPrefix -Prefix $installDir
        Write-Host "  User PATH updated to prioritize $installDir"
    } else {
        Write-Host "[4/4] Session updated. Use -PersistUserEnv to make it permanent." -ForegroundColor Cyan
    }

    Write-Host
    Write-Host "Active PATH resolution:" -ForegroundColor Green
    & where.exe kain
    Write-Host
    Write-Host "Installed binary doctor output:" -ForegroundColor Green
    & $destExe doctor
}
finally {
    Pop-Location
}
