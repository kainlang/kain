#!/usr/bin/env pwsh
param(
    [switch]$PersistUserEnv,
    [switch]$SkipBuild,
    [switch]$ManagedSync
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_REPO_ROOT) -and (Test-Path $env:KAIN_REPO_ROOT)) {
        return [System.IO.Path]::GetFullPath($env:KAIN_REPO_ROOT)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
}

function Resolve-PythonCommand {
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_BAZEL_PYTHON) -and (Test-Path $env:KAIN_BAZEL_PYTHON)) {
        return @($env:KAIN_BAZEL_PYTHON)
    }

    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        return @($py.Source, "-3")
    }

    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        return @($python.Source)
    }

    $python3 = Get-Command python3 -ErrorAction SilentlyContinue
    if ($python3) {
        return @($python3.Source)
    }

    throw "Python 3 was not found. Install Python or set KAIN_BAZEL_PYTHON."
}

$repoRoot = Resolve-RepoRoot
$syncScript = Join-Path $repoRoot "scripts\python\kain_bazel_sync.py"
if (-not (Test-Path $syncScript)) {
    throw ("Kain Bazel sync script not found at " + $syncScript)
}

$pythonCommand = Resolve-PythonCommand
$pythonExe = $pythonCommand[0]
$pythonArgs = @()
if ($pythonCommand.Count -gt 1) {
    $pythonArgs = $pythonCommand[1..($pythonCommand.Count - 1)]
}

$argsForPython = @($pythonArgs)
$argsForPython += @($syncScript, "sync")
if ($SkipBuild) {
    $argsForPython += "--skip-build"
}
if ($ManagedSync) {
    $argsForPython += "--managed-sync"
}
if ($PersistUserEnv) {
    $argsForPython += "--persist-user-env"
}

& $pythonExe @argsForPython
exit $LASTEXITCODE
