#!/usr/bin/env pwsh
param(
    [switch]$PersistUserEnv,
    [switch]$SkipBuild,
    [switch]$ManagedSync
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

function Resolve-CommandPath {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command -and $command.Source -and (Test-Path $command.Source)) {
        return $command.Source
    }

    return $null
}

function Resolve-ClangPath {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_CLANG_PATH)) {
        $candidates += $env:KAIN_CLANG_PATH
    }
    $candidates += (Join-Path $RepoRoot "toolchain\llvm\bin\clang.exe")
    $candidates += (Join-Path $RepoRoot "toolchain\llvm\bin\clang")

    $clangFromPath = Resolve-CommandPath -Name "clang"
    if ($clangFromPath) {
        $candidates += $clangFromPath
    }

    $candidates += "C:\Program Files\LLVM\bin\clang.exe"

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Resolve-Python312Path {
    if (-not [string]::IsNullOrWhiteSpace($env:PYO3_PYTHON) -and (Test-Path $env:PYO3_PYTHON)) {
        return (Resolve-Path $env:PYO3_PYTHON).Path
    }

    $pyLauncher = Resolve-CommandPath -Name "py"
    if ($pyLauncher) {
        try {
            $resolved = (& $pyLauncher -3.12 -c "import sys; print(sys.executable)" 2>$null | Select-Object -First 1).Trim()
            if (-not [string]::IsNullOrWhiteSpace($resolved) -and (Test-Path $resolved)) {
                return (Resolve-Path $resolved).Path
            }
        } catch {
        }
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\python.exe"),
        (Join-Path $env:USERPROFILE "AppData\Local\Programs\Python\Python312\python.exe")
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Get-HashValue {
    param(
        [Parameter(Mandatory = $true)]$Table,
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)]$DefaultValue
    )
    if ($Table -is [hashtable] -and $Table.ContainsKey($Key) -and $null -ne $Table[$Key]) {
        return $Table[$Key]
    }
    return $DefaultValue
}

function Convert-ToStringArray {
    param([Parameter(Mandatory = $true)]$Value)
    $result = @()
    if ($Value -is [System.Collections.IEnumerable]) {
        foreach ($item in $Value) {
            if ($null -ne $item -and -not [string]::IsNullOrWhiteSpace([string]$item)) {
                $result += [string]$item
            }
        }
    }
    return $result
}

function Load-RuntimePolicy {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    $policyPath = Join-Path $RepoRoot "blades\kain-mcp\config\runtime_policy.json"
    if (-not (Test-Path $policyPath)) {
        return @{}
    }
    try {
        return Get-Content -Raw -Path $policyPath | ConvertFrom-Json -AsHashtable
    } catch {
        return @{}
    }
}

function Resolve-SyncStateRoot {
    param([Parameter(Mandatory = $true)]$SyncPolicy)
    $envKey = [string](Get-HashValue -Table $SyncPolicy -Key "state_root_env_key" -DefaultValue "KAIN_SYNC_ROOT")
    $envOverride = [string](Get-Item -Path ("Env:" + $envKey) -ErrorAction SilentlyContinue).Value
    if (-not [string]::IsNullOrWhiteSpace($envOverride)) {
        return [System.IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables($envOverride))
    }

    $defaultRoot = if ($IsWindows) { "%USERPROFILE%\.kain" } else { "~/.kain" }
    $configKey = if ($IsWindows) { "default_state_root_windows" } else { "default_state_root_unix" }
    $configured = [string](Get-HashValue -Table $SyncPolicy -Key $configKey -DefaultValue $defaultRoot)
    $expanded = [Environment]::ExpandEnvironmentVariables($configured)
    if ($expanded.StartsWith("~")) {
        $expanded = Join-Path $HOME ($expanded.TrimStart("~\/"))
    }
    return [System.IO.Path]::GetFullPath($expanded)
}

function Resolve-StatePath {
    param(
        [Parameter(Mandatory = $true)][string]$StateRoot,
        [Parameter(Mandatory = $true)]$SyncPolicy,
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][string]$DefaultRelative
    )
    $relative = [string](Get-HashValue -Table $SyncPolicy -Key $Key -DefaultValue $DefaultRelative)
    return [System.IO.Path]::GetFullPath((Join-Path $StateRoot $relative))
}

function Resolve-RepoHeadSha {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_SYNC_REPO_SHA)) {
        return $env:KAIN_SYNC_REPO_SHA
    }
    try {
        $sha = (& git -C $RepoRoot rev-parse HEAD 2>$null | Select-Object -First 1).Trim()
        if (-not [string]::IsNullOrWhiteSpace($sha)) {
            return $sha
        }
    } catch {
    }
    return "unknown"
}

function Get-RuntimeStamp {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string[]]$RuntimeStampFiles
    )

    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_SYNC_RUNTIME_STAMP)) {
        return $env:KAIN_SYNC_RUNTIME_STAMP
    }

    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($relative in $RuntimeStampFiles) {
        $normalized = $relative -replace "\\", "/"
        $candidate = Join-Path $RepoRoot $relative
        if (Test-Path $candidate) {
            $item = Get-Item -LiteralPath $candidate
            $mtime = [DateTimeOffset]::new($item.LastWriteTimeUtc).ToUnixTimeSeconds()
            $lines.Add(("{0}|1|{1}|{2}" -f $normalized, $item.Length, $mtime))
        } else {
            $lines.Add(("{0}|0||" -f $normalized))
        }
    }

    $joined = [string]::Join("`n", $lines)
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($joined)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash($bytes)
    } finally {
        $sha256.Dispose()
    }
    $fullHash = -join ($hashBytes | ForEach-Object { $_.ToString("x2") })
    if ($fullHash.Length -gt 20) {
        return $fullHash.Substring(0, 20)
    }
    return $fullHash
}

function Move-FileAtomically {
    param(
        [Parameter(Mandatory = $true)][string]$TempPath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    if (Test-Path $DestinationPath) {
        $backupPath = "$DestinationPath.bak.$PID"
        [System.IO.File]::Replace($TempPath, $DestinationPath, $backupPath, $true)
        if (Test-Path $backupPath) {
            Remove-Item -LiteralPath $backupPath -Force -ErrorAction SilentlyContinue
        }
        return
    }

    Move-Item -LiteralPath $TempPath -Destination $DestinationPath -Force
}

function Write-JsonAtomic {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Payload
    )

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    $tempPath = Join-Path $directory ([System.IO.Path]::GetFileName($Path) + ".tmp.$PID")
    $json = $Payload | ConvertTo-Json -Depth 12
    [System.IO.File]::WriteAllText($tempPath, $json, [System.Text.UTF8Encoding]::new($false))
    Move-FileAtomically -TempPath $tempPath -DestinationPath $Path
}

function Read-BuildCounter {
    param([Parameter(Mandatory = $true)][string]$CounterPath)
    if (-not (Test-Path $CounterPath)) {
        return 0
    }
    try {
        $payload = Get-Content -Raw -Path $CounterPath | ConvertFrom-Json -AsHashtable
        if ($payload -is [hashtable] -and $payload.ContainsKey("last_build_number")) {
            return [int]$payload["last_build_number"]
        }
    } catch {
    }
    return 0
}

function Write-BuildCounter {
    param(
        [Parameter(Mandatory = $true)][string]$CounterPath,
        [Parameter(Mandatory = $true)][int]$NextBuildNumber
    )

    $payload = @{
        schema_version = 1
        last_build_number = $NextBuildNumber
    }
    Write-JsonAtomic -Path $CounterPath -Payload $payload
}

function Get-BinaryFingerprint {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path $Path)) {
        return @{
            path = $Path
            exists = $false
        }
    }
    $item = Get-Item -LiteralPath $Path
    return @{
        path = (Resolve-Path $Path).Path
        exists = $true
        size_bytes = [int64]$item.Length
        mtime_unix = [DateTimeOffset]::new($item.LastWriteTimeUtc).ToUnixTimeSeconds()
    }
}

$repoRoot = ""
if (-not [string]::IsNullOrWhiteSpace($env:KAIN_REPO_ROOT) -and (Test-Path $env:KAIN_REPO_ROOT)) {
    $repoRoot = [System.IO.Path]::GetFullPath($env:KAIN_REPO_ROOT)
} else {
    try {
        $resolvedRepoRoot = (& git rev-parse --show-toplevel 2>$null | Select-Object -First 1)
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($resolvedRepoRoot)) {
            $repoRoot = $resolvedRepoRoot.Trim()
        }
    } catch {
    }
}
if ([string]::IsNullOrWhiteSpace($repoRoot)) {
    $scriptDerivedRepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\\.."))
    if (Test-Path (Join-Path $scriptDerivedRepoRoot ".git")) {
        $repoRoot = $scriptDerivedRepoRoot
    }
}
if ([string]::IsNullOrWhiteSpace($repoRoot) -or -not (Test-Path $repoRoot)) {
    throw "Unable to resolve repository root."
}

$runtimePolicy = Load-RuntimePolicy -RepoRoot $repoRoot
$syncPolicy = Get-HashValue -Table $runtimePolicy -Key "launcher_sync" -DefaultValue @{}
$stateRoot = Resolve-SyncStateRoot -SyncPolicy $syncPolicy
$lockPath = Resolve-StatePath -StateRoot $stateRoot -SyncPolicy $syncPolicy -Key "lock_relative_path" -DefaultRelative "locks/sync.lock"
$stampPath = if (-not [string]::IsNullOrWhiteSpace($env:KAIN_SYNC_STAMP_PATH)) {
    [System.IO.Path]::GetFullPath($env:KAIN_SYNC_STAMP_PATH)
} else {
    Resolve-StatePath -StateRoot $stateRoot -SyncPolicy $syncPolicy -Key "stamp_relative_path" -DefaultRelative "state/kain_sync_stamp.json"
}
$counterPath = Resolve-StatePath -StateRoot $stateRoot -SyncPolicy $syncPolicy -Key "build_counter_relative_path" -DefaultRelative "state/build_counter.json"
$runtimeStampFiles = Convert-ToStringArray -Value (Get-HashValue -Table $syncPolicy -Key "runtime_stamp_files" -DefaultValue @(
        "runtime/kain_runtime.c",
        "runtime/native_runtime.toml",
        "blades/kain-mcp/config/runtime_policy.json"
    ))

$installRoot = if ($env:CARGO_HOME) { $env:CARGO_HOME } else { Join-Path $env:USERPROFILE ".cargo" }
$installDir = Join-Path $installRoot "bin"
$binaryNames = @("kain", "kn")
$binaryTargets = foreach ($name in $binaryNames) {
    [pscustomobject]@{
        Name = $name
        Source = Join-Path $repoRoot ("target\release\" + $name + ".exe")
        Destination = Join-Path $installDir ($name + ".exe")
    }
}
$primaryBinary = $binaryTargets | Where-Object { $_.Name -eq "kain" } | Select-Object -First 1
$resolvedClangPath = Resolve-ClangPath -RepoRoot $repoRoot
$resolvedPythonPath = Resolve-Python312Path
$sessionPathPrefixes = @($installDir)

if ($resolvedClangPath) {
    $sessionPathPrefixes += (Split-Path -Parent $resolvedClangPath)
}
if ($resolvedPythonPath) {
    $sessionPathPrefixes += (Split-Path -Parent $resolvedPythonPath)
}

$resourceMap = [ordered]@{
    "KAIN_STDLIB_PATH" = (Join-Path $repoRoot "stdlib")
    "KAIN_RUNTIME_C_PATH" = (Join-Path $repoRoot "runtime\kain_runtime.c")
    "KAIN_RUNTIME_MANIFEST_PATH" = (Join-Path $repoRoot "runtime\native_runtime.toml")
    "KAIN_SYNC_ROOT" = $stateRoot
    "KAIN_SYNC_STAMP_PATH" = $stampPath
    "KAIN_SYNC_LOCK_PATH" = $lockPath
}

if ($resolvedClangPath) {
    $resourceMap["KAIN_CLANG_PATH"] = $resolvedClangPath
}
if ($resolvedPythonPath) {
    $resourceMap["PYO3_PYTHON"] = $resolvedPythonPath
}

$buildNumberForThisSync = ""
$nextBuildNumber = 0

if (-not $ManagedSync) {
    Write-Host "============================================================================" -ForegroundColor Cyan
    Write-Host "Syncing KAIN source of truth" -ForegroundColor Cyan
    Write-Host "============================================================================" -ForegroundColor Cyan
    Write-Host "Repo Root : $repoRoot"
    Write-Host "Install   : $installDir"
    Write-Host "State Root: $stateRoot"
    Write-Host
}

Push-Location $repoRoot
try {
    if (-not $ManagedSync) {
        Write-Host "[0/5] Resolving external toolchain paths..." -ForegroundColor Cyan
        if ($resolvedClangPath) {
            Write-Host ("  [ok] clang -> {0}" -f $resolvedClangPath)
        } else {
            Write-Host "  [warn] clang.exe not found in repo toolchain, PATH, or C:\Program Files\LLVM\bin" -ForegroundColor Yellow
        }
        if ($resolvedPythonPath) {
            Write-Host ("  [ok] python 3.12 -> {0}" -f $resolvedPythonPath)
        } else {
            Write-Host "  [warn] Python 3.12 not found. PyO3-backed builds may fail on newer default Python versions." -ForegroundColor Yellow
        }
    }

    foreach ($prefix in $sessionPathPrefixes) {
        if (-not [string]::IsNullOrWhiteSpace($prefix) -and (Test-Path $prefix)) {
            $env:PATH = Set-PathPrefix -PathValue $env:PATH -Prefix $prefix
        }
    }

    foreach ($entry in $resourceMap.GetEnumerator()) {
        if (-not [string]::IsNullOrWhiteSpace([string]$entry.Value)) {
            Set-Item -Path ("Env:" + $entry.Key) -Value $entry.Value
        }
    }

    if (-not $SkipBuild) {
        $currentBuildCounter = Read-BuildCounter -CounterPath $counterPath
        $nextBuildNumber = $currentBuildCounter + 1
        $buildNumberForThisSync = [string]$nextBuildNumber
        $env:KAIN_BUILD_NUMBER = $buildNumberForThisSync
        $env:KAIN_BUILD_TRACKING_MODE = "managed"

        if (-not $ManagedSync) {
            Write-Host "[1/5] Building crates/cli in release mode (build #$buildNumberForThisSync)..." -ForegroundColor Cyan
        }
        cargo build --release -p cli
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build --release -p cli failed with exit code $LASTEXITCODE"
        }
        Write-BuildCounter -CounterPath $counterPath -NextBuildNumber $nextBuildNumber
    } else {
        if (-not $ManagedSync) {
            Write-Host "[1/5] Skipping build (using existing release binary)..." -ForegroundColor Yellow
        }
    }

    foreach ($binary in $binaryTargets) {
        if (-not (Test-Path $binary.Source)) {
            throw ("Release binary not found at " + $binary.Source)
        }
    }

    if (-not $ManagedSync) {
        Write-Host "[2/5] Installing stable PATH binaries (atomic swap)..." -ForegroundColor Cyan
    }
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
    foreach ($binary in $binaryTargets) {
        $tempDestination = ($binary.Destination + ".syncing." + $PID + ".tmp")
        Copy-Item -LiteralPath $binary.Source -Destination $tempDestination -Force
        Move-FileAtomically -TempPath $tempDestination -DestinationPath $binary.Destination
        if (-not $ManagedSync) {
            Write-Host ("  [swap] {0} -> {1}" -f $binary.Source, $binary.Destination)
        }
    }

    if (-not $ManagedSync) {
        Write-Host "[3/5] Applying KAIN resource roots to this session..." -ForegroundColor Cyan
    }
    foreach ($entry in $resourceMap.GetEnumerator()) {
        if (-not [string]::IsNullOrWhiteSpace([string]$entry.Value)) {
            Set-Item -Path ("Env:" + $entry.Key) -Value $entry.Value
            if (-not $ManagedSync) {
                Write-Host ("  [set] {0}={1}" -f $entry.Key, $entry.Value)
            }
        }
    }

    if ($PersistUserEnv) {
        if (-not $ManagedSync) {
            Write-Host "[4/5] Persisting PATH and KAIN environment variables for future shells..." -ForegroundColor Cyan
        }
        foreach ($entry in $resourceMap.GetEnumerator()) {
            if (-not [string]::IsNullOrWhiteSpace([string]$entry.Value)) {
                [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, "User")
            }
        }
        foreach ($prefix in $sessionPathPrefixes) {
            if (-not [string]::IsNullOrWhiteSpace($prefix) -and (Test-Path $prefix)) {
                Set-UserPathPrefix -Prefix $prefix
                if (-not $ManagedSync) {
                    Write-Host ("  User PATH updated to prioritize {0}" -f $prefix)
                }
            }
        }
    } elseif (-not $ManagedSync) {
        Write-Host "[4/5] Session updated. Use -PersistUserEnv to make it permanent." -ForegroundColor Cyan
    }

    $repoSha = Resolve-RepoHeadSha -RepoRoot $repoRoot
    $runtimeStamp = Get-RuntimeStamp -RepoRoot $repoRoot -RuntimeStampFiles $runtimeStampFiles
    $binaryFingerprint = Get-BinaryFingerprint -Path $primaryBinary.Destination
    $nowUnix = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $stampPayload = @{
        schema_version = 1
        repo_root = $repoRoot
        repo_sha = $repoSha
        runtime_stamp = $runtimeStamp
        runtime_stamp_files = $runtimeStampFiles
        binary = $binaryFingerprint
        build_number = $buildNumberForThisSync
        synced_at_unix = $nowUnix
        last_attempt_unix = $nowUnix
        managed_sync = [bool]$ManagedSync
    }

    Write-JsonAtomic -Path $stampPath -Payload $stampPayload

    if (-not $ManagedSync) {
        Write-Host "[5/5] Sync stamp updated: $stampPath" -ForegroundColor Cyan
        Write-Host
        Write-Host "Active PATH resolution (kain):" -ForegroundColor Green
        & where.exe kain
        Write-Host
        Write-Host "Active PATH resolution (kn):" -ForegroundColor Green
        & where.exe kn
        Write-Host
        Write-Host "Installed binary doctor output:" -ForegroundColor Green
        & $primaryBinary.Destination doctor
    }
} finally {
    Pop-Location
}
