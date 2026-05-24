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

function Resolve-RustcPath {
    if (-not [string]::IsNullOrWhiteSpace($env:RUSTC) -and (Test-Path $env:RUSTC)) {
        return (Resolve-Path $env:RUSTC).Path
    }

    $rustcFromPath = Resolve-CommandPath -Name "rustc"
    if ($rustcFromPath) {
        return (Resolve-Path $rustcFromPath).Path
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

function ConvertTo-NormalizedObject {
    param([Parameter(Mandatory = $true)]$Value)

    if ($null -eq $Value) {
        return $null
    }
    if ($Value -is [hashtable]) {
        $result = @{}
        foreach ($key in $Value.Keys) {
            $result[$key] = ConvertTo-NormalizedObject -Value $Value[$key]
        }
        return $result
    }
    if ($Value -is [System.Management.Automation.PSCustomObject]) {
        $result = @{}
        foreach ($property in $Value.PSObject.Properties) {
            $result[$property.Name] = ConvertTo-NormalizedObject -Value $property.Value
        }
        return $result
    }
    if (($Value -is [System.Collections.IEnumerable]) -and -not ($Value -is [string])) {
        $items = @()
        foreach ($item in $Value) {
            $items += ,(ConvertTo-NormalizedObject -Value $item)
        }
        return $items
    }
    return $Value
}

function ConvertFrom-JsonCompat {
    param([Parameter(Mandatory = $true)][string]$JsonText)

    $convertCommand = Get-Command ConvertFrom-Json -ErrorAction Stop
    if ($convertCommand.Parameters.ContainsKey("AsHashtable")) {
        return ConvertFrom-Json -InputObject $JsonText -AsHashtable
    }
    $parsed = ConvertFrom-Json -InputObject $JsonText
    return ConvertTo-NormalizedObject -Value $parsed
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

function Resolve-ConfiguredPathList {
    param([Parameter(Mandatory = $true)]$Value)

    $results = @()
    foreach ($item in (Convert-ToStringArray -Value $Value)) {
        $expanded = [Environment]::ExpandEnvironmentVariables($item)
        if ($expanded.StartsWith("~")) {
            $expanded = Join-Path $HOME ($expanded.TrimStart("~\/"))
        }
        $results += [System.IO.Path]::GetFullPath($expanded)
    }
    return $results
}

function Load-RuntimePolicy {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    $policyPath = Join-Path $RepoRoot "blades\kain-mcp\config\runtime_policy.json"
    if (-not (Test-Path $policyPath)) {
        return @{}
    }
    try {
        return ConvertFrom-JsonCompat -JsonText (Get-Content -Raw -Path $policyPath)
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

function Resolve-RepoCommitCount {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    try {
        $count = (& git -C $RepoRoot rev-list --count HEAD 2>$null | Select-Object -First 1).Trim()
        if (-not [string]::IsNullOrWhiteSpace($count)) {
            return $count
        }
    } catch {
    }
    return "0"
}

function Resolve-RepoDirtyState {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    try {
        $status = (& git -C $RepoRoot status --porcelain 2>$null)
        if ($LASTEXITCODE -eq 0) {
            if ([string]::IsNullOrWhiteSpace(($status | Out-String))) {
                return "clean"
            }
            return "dirty"
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

function Write-TextAtomic {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    $tempPath = Join-Path $directory ([System.IO.Path]::GetFileName($Path) + ".tmp.$PID")
    [System.IO.File]::WriteAllText($tempPath, $Content, [System.Text.UTF8Encoding]::new($false))
    Move-FileAtomically -TempPath $tempPath -DestinationPath $Path
}

function Copy-FileAtomic {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    $directory = Split-Path -Parent $DestinationPath
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    $tempPath = Join-Path $directory ([System.IO.Path]::GetFileName($DestinationPath) + ".tmp.$PID")
    Copy-Item -LiteralPath $SourcePath -Destination $tempPath -Force
    Move-FileAtomically -TempPath $tempPath -DestinationPath $DestinationPath
}

function Backup-ExistingBinaryOnce {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Suffix
    )

    if (-not (Test-Path $Path)) {
        return
    }

    $backupPath = "$Path.$Suffix"
    if (Test-Path $backupPath) {
        return
    }

    Copy-FileAtomic -SourcePath $Path -DestinationPath $backupPath
}

function Read-BuildCounter {
    param([Parameter(Mandatory = $true)][string]$CounterPath)
    if (-not (Test-Path $CounterPath)) {
        return 0
    }
    try {
        $payload = ConvertFrom-JsonCompat -JsonText (Get-Content -Raw -Path $CounterPath)
        if ($payload -is [hashtable] -and $payload.ContainsKey("last_build_number")) {
            return [int]$payload["last_build_number"]
        }
    } catch {
    }
    return 0
}

function Read-SyncStampBuildNumber {
    param([Parameter(Mandatory = $true)][string]$StampPath)
    if (-not (Test-Path $StampPath)) {
        return 0
    }
    try {
        $payload = ConvertFrom-JsonCompat -JsonText (Get-Content -Raw -Path $StampPath)
        if ($payload -is [hashtable] -and $payload.ContainsKey("build_number")) {
            $raw = [string]$payload["build_number"]
            if (-not [string]::IsNullOrWhiteSpace($raw)) {
                return [int]$raw
            }
        }
    } catch {
    }
    return 0
}

function Read-ManagedBuildNumber {
    param(
        [Parameter(Mandatory = $true)][string]$CounterPath,
        [Parameter(Mandatory = $true)][string]$StampPath
    )
    $counterValue = Read-BuildCounter -CounterPath $CounterPath
    $stampValue = Read-SyncStampBuildNumber -StampPath $StampPath
    return [Math]::Max($counterValue, $stampValue)
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

function Build-BazelLauncherBinary {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$RustcPath,
        [Parameter(Mandatory = $true)][string]$DefaultRepoRoot,
        [Parameter(Mandatory = $true)][string]$DefaultBazelConfig,
        [Parameter(Mandatory = $true)][string]$DefaultLauncherDir,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )

    $sourcePath = Join-Path $RepoRoot "scripts\windows\kain_bazel_cli_launcher.rs"
    if (-not (Test-Path $sourcePath)) {
        throw ("Bazel launcher source not found at " + $sourcePath)
    }

    $outputDir = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDir)) {
        New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
    }

    $tempOutputPath = Join-Path $outputDir ([System.IO.Path]::GetFileName($OutputPath) + ".tmp.$PID")
    $previousRepoRoot = $env:KAIN_DEFAULT_REPO_ROOT
    $previousBazelConfig = $env:KAIN_DEFAULT_BAZEL_CONFIG
    $previousLauncherDir = $env:KAIN_DEFAULT_LAUNCHER_DIR

    try {
        $env:KAIN_DEFAULT_REPO_ROOT = $DefaultRepoRoot
        $env:KAIN_DEFAULT_BAZEL_CONFIG = $DefaultBazelConfig
        $env:KAIN_DEFAULT_LAUNCHER_DIR = $DefaultLauncherDir
        & $RustcPath $sourcePath "--crate-name" "kain_bazel_cli_launcher" "-C" "opt-level=2" "-C" "debuginfo=0" "-o" $tempOutputPath
        if ($LASTEXITCODE -ne 0) {
            throw ("rustc failed to build Bazel launcher shim with exit code " + $LASTEXITCODE)
        }
        Move-FileAtomically -TempPath $tempOutputPath -DestinationPath $OutputPath
    } finally {
        if ($null -ne $previousRepoRoot) {
            $env:KAIN_DEFAULT_REPO_ROOT = $previousRepoRoot
        } else {
            Remove-Item Env:KAIN_DEFAULT_REPO_ROOT -ErrorAction SilentlyContinue
        }
        if ($null -ne $previousBazelConfig) {
            $env:KAIN_DEFAULT_BAZEL_CONFIG = $previousBazelConfig
        } else {
            Remove-Item Env:KAIN_DEFAULT_BAZEL_CONFIG -ErrorAction SilentlyContinue
        }
        if ($null -ne $previousLauncherDir) {
            $env:KAIN_DEFAULT_LAUNCHER_DIR = $previousLauncherDir
        } else {
            Remove-Item Env:KAIN_DEFAULT_LAUNCHER_DIR -ErrorAction SilentlyContinue
        }
        if (Test-Path $tempOutputPath) {
            Remove-Item -LiteralPath $tempOutputPath -Force -ErrorAction SilentlyContinue
        }
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
        "runtime/runtime.c",
        "runtime/native_core_runtime.toml",
        "blades/kain-mcp/config/runtime_policy.json"
    ))

$launcherScriptPath = Join-Path $repoRoot "scripts\windows\launch-bazel-cli.ps1"
if (-not (Test-Path $launcherScriptPath)) {
    throw ("Bazel launcher script not found at " + $launcherScriptPath)
}
$sharedLauncherDir = if (-not [string]::IsNullOrWhiteSpace($env:KAIN_BAZEL_LAUNCHER_DIR)) {
    [System.IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables($env:KAIN_BAZEL_LAUNCHER_DIR))
} else {
    [System.IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables([string](Get-HashValue -Table $syncPolicy -Key "shared_launcher_dir_windows" -DefaultValue "D:/Kain-Bazel/bin")))
}
$shadowLauncherDirs = Resolve-ConfiguredPathList -Value (Get-HashValue -Table $syncPolicy -Key "shadow_launcher_dirs_windows" -DefaultValue @("%USERPROFILE%/.kain/bin", "%USERPROFILE%/.cargo/bin"))
$bazelConfig = if (-not [string]::IsNullOrWhiteSpace($env:KAIN_BAZEL_CONFIG)) {
    $env:KAIN_BAZEL_CONFIG
} else {
    [string](Get-HashValue -Table $syncPolicy -Key "bazel_default_config_windows" -DefaultValue "dev")
}
$binaryNames = @("kain", "kn")
$binaryTargets = foreach ($name in $binaryNames) {
    [pscustomobject]@{
        Name = $name
        SharedExecutable = Join-Path $sharedLauncherDir ($name + ".exe")
        SharedWrapper = Join-Path $sharedLauncherDir ($name + ".cmd")
        ShadowExecutables = @($shadowLauncherDirs | ForEach-Object { Join-Path $_ ($name + ".exe") })
    }
}
$primaryBinary = $binaryTargets | Where-Object { $_.Name -eq "kain" } | Select-Object -First 1
$resolvedClangPath = Resolve-ClangPath -RepoRoot $repoRoot
$resolvedPythonPath = Resolve-Python312Path
$resolvedRustcPath = Resolve-RustcPath
New-Item -ItemType Directory -Force -Path $sharedLauncherDir | Out-Null
$sessionPathPrefixes = @($sharedLauncherDir)
$launcherShimTemplatePath = Join-Path $stateRoot "artifacts\kain_bazel_cli_launcher.exe"

if ($resolvedClangPath) {
    $sessionPathPrefixes += (Split-Path -Parent $resolvedClangPath)
}
if ($resolvedPythonPath) {
    $sessionPathPrefixes += (Split-Path -Parent $resolvedPythonPath)
}

$resourceMap = [ordered]@{
    "KAIN_REPO_ROOT" = $repoRoot
    "KAIN_STDLIB_PATH" = (Join-Path $repoRoot "stdlib")
    "KAIN_RUNTIME_C_PATH" = (Join-Path $repoRoot "runtime\runtime.c")
    "KAIN_RUNTIME_MANIFEST_PATH" = (Join-Path $repoRoot "runtime\native_core_runtime.toml")
    "KAIN_SYNC_ROOT" = $stateRoot
    "KAIN_SYNC_STAMP_PATH" = $stampPath
    "KAIN_SYNC_LOCK_PATH" = $lockPath
    "KAIN_BAZEL_CONFIG" = $bazelConfig
    "KAIN_BAZEL_LAUNCHER_DIR" = $sharedLauncherDir
}

if ($resolvedClangPath) {
    $resourceMap["KAIN_CLANG_PATH"] = $resolvedClangPath
}
if ($resolvedPythonPath) {
    $resourceMap["PYO3_PYTHON"] = $resolvedPythonPath
}

if (-not $ManagedSync) {
    Write-Host "============================================================================" -ForegroundColor Cyan
    Write-Host "Syncing KAIN source of truth" -ForegroundColor Cyan
    Write-Host "============================================================================" -ForegroundColor Cyan
    Write-Host "Repo Root : $repoRoot"
    Write-Host "Launcher  : $sharedLauncherDir"
    Write-Host "Config    : $bazelConfig"
    Write-Host "State Root: $stateRoot"
    Write-Host
}

Push-Location $repoRoot
try {
    if (-not $ManagedSync) {
        Write-Host "[0/6] Resolving external toolchain paths..." -ForegroundColor Cyan
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
        if ($resolvedRustcPath) {
            Write-Host ("  [ok] rustc -> {0}" -f $resolvedRustcPath)
        } else {
            throw "rustc.exe not found in PATH or RUSTC. The Bazel launcher shim cannot be built."
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
        if (-not $ManagedSync) {
            Write-Host "[1/6] Building Bazel CLI targets through the shared launcher lane..." -ForegroundColor Cyan
        }
        foreach ($binary in $binaryTargets) {
            & powershell -NoProfile -ExecutionPolicy Bypass -File $launcherScriptPath -BinaryName $binary.Name -BazelConfig $bazelConfig -LauncherPath $binary.SharedExecutable -UpdateStampOnly
            if ($LASTEXITCODE -ne 0) {
                throw ("Bazel launcher build failed for " + $binary.Name + " with exit code " + $LASTEXITCODE)
            }
            if (-not $ManagedSync) {
                Write-Host ("  [built] //:{0} via {1}" -f $binary.Name, $binary.SharedExecutable)
            }
        }
    } else {
        if (-not $ManagedSync) {
            Write-Host "[1/6] Skipping build (refreshing wrapper stamp only)..." -ForegroundColor Yellow
        }
        & powershell -NoProfile -ExecutionPolicy Bypass -File $launcherScriptPath -BinaryName "kain" -BazelConfig $bazelConfig -LauncherPath $primaryBinary.SharedExecutable -UpdateStampOnly -SkipBuild
        if ($LASTEXITCODE -ne 0) {
            throw ("Bazel launcher stamp refresh failed with exit code " + $LASTEXITCODE)
        }
    }

    if (-not $ManagedSync) {
        Write-Host "[2/6] Building native Bazel launcher shim..." -ForegroundColor Cyan
    }
    Build-BazelLauncherBinary -RepoRoot $repoRoot -RustcPath $resolvedRustcPath -DefaultRepoRoot $repoRoot -DefaultBazelConfig $bazelConfig -DefaultLauncherDir $sharedLauncherDir -OutputPath $launcherShimTemplatePath
    if (-not $ManagedSync) {
        Write-Host ("  [shim] {0}" -f $launcherShimTemplatePath)
        Write-Host "[3/6] Installing shared PATH launchers..." -ForegroundColor Cyan
    }
    foreach ($binary in $binaryTargets) {
        Copy-FileAtomic -SourcePath $launcherShimTemplatePath -DestinationPath $binary.SharedExecutable
        if (-not $ManagedSync) {
            Write-Host ("  [exe] {0}" -f $binary.SharedExecutable)
        }
        foreach ($shadowExecutable in $binary.ShadowExecutables) {
            Backup-ExistingBinaryOnce -Path $shadowExecutable -Suffix "pre-bazel-wrapper"
            Copy-FileAtomic -SourcePath $launcherShimTemplatePath -DestinationPath $shadowExecutable
            if (-not $ManagedSync) {
                Write-Host ("  [shadow] {0}" -f $shadowExecutable)
            }
        }
        $wrapperContent = @"
@echo off
"%~dp0$($binary.Name).exe" %*
exit /b %ERRORLEVEL%
"@
        Write-TextAtomic -Path $binary.SharedWrapper -Content $wrapperContent
        if (-not $ManagedSync) {
            Write-Host ("  [cmd] {0}" -f $binary.SharedWrapper)
        }
    }

    if (-not $ManagedSync) {
        Write-Host "[4/6] Applying KAIN resource roots to this session..." -ForegroundColor Cyan
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
            Write-Host "[5/6] Persisting PATH and KAIN environment variables for future shells..." -ForegroundColor Cyan
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
        Write-Host "[5/6] Session updated. Use -PersistUserEnv to make it permanent." -ForegroundColor Cyan
    }

    if (-not (Test-Path $stampPath)) {
        throw ("Expected sync stamp at " + $stampPath + " after Bazel launcher refresh")
    }

    if (-not $ManagedSync) {
        Write-Host "[6/6] Sync stamp updated: $stampPath" -ForegroundColor Cyan
        Write-Host
        Write-Host "Active PATH resolution (kain):" -ForegroundColor Green
        & where.exe kain
        Write-Host
        Write-Host "Active PATH resolution (kn):" -ForegroundColor Green
        & where.exe kn
        Write-Host
        Write-Host "Installed launcher doctor output:" -ForegroundColor Green
        & $primaryBinary.SharedExecutable doctor
    }
} finally {
    Pop-Location
}
