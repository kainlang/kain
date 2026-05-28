#!/usr/bin/env pwsh
param(
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    if (-not [string]::IsNullOrWhiteSpace($RepoRoot)) {
        return [System.IO.Path]::GetFullPath($RepoRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_REPO_ROOT) -and (Test-Path $env:KAIN_REPO_ROOT)) {
        return [System.IO.Path]::GetFullPath($env:KAIN_REPO_ROOT)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
}

$repoRoot = Resolve-RepoRoot
$trayScript = Join-Path $repoRoot "scripts\python\bazel_tray.py"
if (-not (Test-Path $trayScript)) {
    throw ("Bazel tray script not found at " + $trayScript)
}

$pythonw = Get-Command pythonw -ErrorAction SilentlyContinue
if ($pythonw) {
    Start-Process -FilePath $pythonw.Source -ArgumentList @($trayScript) -WorkingDirectory $repoRoot -WindowStyle Hidden
    exit 0
}

$python = Get-Command python -ErrorAction SilentlyContinue
if ($python) {
    Start-Process -FilePath $python.Source -ArgumentList @($trayScript) -WorkingDirectory $repoRoot -WindowStyle Hidden
    exit 0
}

throw "Python GUI launcher not found. Install pythonw.exe or python.exe."
