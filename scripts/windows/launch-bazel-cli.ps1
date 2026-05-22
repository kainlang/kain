#!/usr/bin/env pwsh
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("kain", "kn")]
    [string]$BinaryName,

    [string]$BazelConfig = "",

    [string]$LauncherPath = "",

    [switch]$SkipBuild,

    [switch]$UpdateStampOnly,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ForwardArgs
)

$ErrorActionPreference = "Stop"

function Resolve-CommandPath {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command -and $command.Source -and (Test-Path $command.Source)) {
        return $command.Source
    }

    return $null
}

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

function Resolve-RepoRoot {
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_REPO_ROOT) -and (Test-Path $env:KAIN_REPO_ROOT)) {
        return [System.IO.Path]::GetFullPath($env:KAIN_REPO_ROOT)
    }

    $scriptDerivedRepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
    if (Test-Path (Join-Path $scriptDerivedRepoRoot ".git")) {
        return $scriptDerivedRepoRoot
    }

    throw "Unable to resolve repository root."
}

function Load-RuntimePolicy {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    $policyPath = Join-Path $RepoRoot "blades\kain-mcp\config\runtime_policy.json"
    if (-not (Test-Path $policyPath)) {
        return @{}
    }
    return ConvertFrom-JsonCompat -JsonText (Get-Content -Raw -Path $policyPath)
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

function Resolve-SyncStateRoot {
    param([Parameter(Mandatory = $true)]$SyncPolicy)

    $envKey = [string](Get-HashValue -Table $SyncPolicy -Key "state_root_env_key" -DefaultValue "KAIN_SYNC_ROOT")
    $envOverride = [string](Get-Item -Path ("Env:" + $envKey) -ErrorAction SilentlyContinue).Value
    if (-not [string]::IsNullOrWhiteSpace($envOverride)) {
        return [System.IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables($envOverride))
    }

    $configured = [string](Get-HashValue -Table $SyncPolicy -Key "default_state_root_windows" -DefaultValue "%USERPROFILE%/.kain")
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

function Read-JsonFileCompat {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path $Path)) {
        return @{}
    }
    try {
        $raw = Get-Content -Raw -Path $Path
        if ([string]::IsNullOrWhiteSpace($raw)) {
            return @{}
        }
        $parsed = ConvertFrom-JsonCompat -JsonText $raw
        if ($parsed -is [hashtable]) {
            return $parsed
        }
    } catch {
    }
    return @{}
}

function Get-ShortSha256 {
    param([Parameter(Mandatory = $true)][string]$Text)

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
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
    return Get-ShortSha256 -Text $joined
}

function Normalize-RelativePath {
    param([Parameter(Mandatory = $true)][string]$PathText)

    $normalized = $PathText.Trim()
    if ($normalized.StartsWith("./")) {
        $normalized = $normalized.Substring(2)
    }
    if ($normalized.StartsWith(".\")) {
        $normalized = $normalized.Substring(2)
    }
    $normalized = $normalized -replace "\\", "/"
    while ($normalized.StartsWith("/")) {
        $normalized = $normalized.Substring(1)
    }
    return $normalized
}

function Invoke-GitLines {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string[]]$Args
    )

    try {
        $output = & git -C $RepoRoot @Args 2>$null
        if ($LASTEXITCODE -ne 0) {
            return @()
        }
        return @(
            $output |
            ForEach-Object { ([string]$_).Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        )
    } catch {
        return @()
    }
}

function Resolve-SourceWatchPaths {
    param([Parameter(Mandatory = $true)]$SyncPolicy)

    $configured = Convert-ToStringArray -Value (Get-HashValue -Table $SyncPolicy -Key "source_watch_paths" -DefaultValue @(
            "crates",
            "runtime",
            "src",
            "Cargo.toml",
            "Cargo.lock",
            "BUILD.bazel",
            "MODULE.bazel",
            "MODULE.bazel.lock"
        ))
    $normalized = @()
    $seen = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $configured) {
        $pathText = Normalize-RelativePath -PathText ([string]$entry)
        if ([string]::IsNullOrWhiteSpace($pathText)) {
            continue
        }
        if ($seen.Add($pathText)) {
            $normalized += $pathText
        }
    }
    return $normalized
}

function Get-SourceStampData {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string[]]$WatchPaths
    )

    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_SYNC_SOURCE_STAMP)) {
        return @{
            stamp = $env:KAIN_SYNC_SOURCE_STAMP
            dirty_count = 0
            watch_paths = $WatchPaths
        }
    }

    $normalizedWatchPaths = @()
    foreach ($pathText in $WatchPaths) {
        $normalized = Normalize-RelativePath -PathText ([string]$pathText)
        if (-not [string]::IsNullOrWhiteSpace($normalized)) {
            $normalizedWatchPaths += $normalized
        }
    }
    [Array]::Sort($normalizedWatchPaths, [System.StringComparer]::OrdinalIgnoreCase)

    $headDescriptors = New-Object System.Collections.Generic.List[string]
    foreach ($relative in $normalizedWatchPaths) {
        $headObject = "missing"
        try {
            $resolved = (& git -C $RepoRoot rev-parse --verify ("HEAD:" + $relative) 2>$null | Select-Object -First 1).Trim()
            if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($resolved)) {
                $headObject = $resolved
            }
        } catch {
        }
        $headDescriptors.Add(("head|{0}|{1}" -f $relative, $headObject))
    }

    $pathArgs = @("--")
    $pathArgs += $normalizedWatchPaths
    $dirtyPaths = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    $dirtyCommandArgs = @(
        @("diff", "--name-only"),
        @("diff", "--cached", "--name-only"),
        @("ls-files", "--others", "--exclude-standard")
    )
    foreach ($commandArgs in $dirtyCommandArgs) {
        $lines = Invoke-GitLines -RepoRoot $RepoRoot -Args ($commandArgs + $pathArgs)
        foreach ($line in $lines) {
            $normalized = Normalize-RelativePath -PathText $line
            if (-not [string]::IsNullOrWhiteSpace($normalized)) {
                $null = $dirtyPaths.Add($normalized)
            }
        }
    }

    $dirtyPathList = @($dirtyPaths)
    [Array]::Sort($dirtyPathList, [System.StringComparer]::OrdinalIgnoreCase)
    $dirtyDescriptors = New-Object System.Collections.Generic.List[string]
    foreach ($relative in $dirtyPathList) {
        $candidate = Join-Path $RepoRoot $relative
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $contentHash = "nohash"
            try {
                $contentHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash.ToLowerInvariant()
            } catch {
            }
            $dirtyDescriptors.Add(("dirty|{0}|file|{1}" -f $relative, $contentHash))
            continue
        }
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            $dirtyDescriptors.Add(("dirty|{0}|dir|present" -f $relative))
            continue
        }
        $dirtyDescriptors.Add(("dirty|{0}|missing" -f $relative))
    }

    $stampLines = New-Object System.Collections.Generic.List[string]
    foreach ($relative in $normalizedWatchPaths) {
        $stampLines.Add(("watch|{0}" -f $relative))
    }
    foreach ($descriptor in $headDescriptors) {
        $stampLines.Add($descriptor)
    }
    foreach ($descriptor in $dirtyDescriptors) {
        $stampLines.Add($descriptor)
    }
    $stamp = Get-ShortSha256 -Text ([string]::Join("`n", $stampLines))
    return @{
        stamp = $stamp
        dirty_count = $dirtyDescriptors.Count
        watch_paths = $normalizedWatchPaths
    }
}

function Resolve-StampedBinaryPath {
    param(
        [Parameter(Mandatory = $true)]$StampPayload,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if ($StampPayload -isnot [hashtable]) {
        return $null
    }
    if ($StampPayload.ContainsKey("binary_by_name")) {
        $binaryByName = $StampPayload["binary_by_name"]
        if ($binaryByName -is [hashtable] -and $binaryByName.ContainsKey($Name)) {
            $entry = $binaryByName[$Name]
            if ($entry -is [hashtable] -and $entry.ContainsKey("path")) {
                $candidate = [string]$entry["path"]
                if (-not [string]::IsNullOrWhiteSpace($candidate)) {
                    return [System.IO.Path]::GetFullPath($candidate)
                }
            }
        }
    }
    if ($Name -eq "kain" -and $StampPayload.ContainsKey("binary")) {
        $legacy = $StampPayload["binary"]
        if ($legacy -is [hashtable] -and $legacy.ContainsKey("path")) {
            $candidate = [string]$legacy["path"]
            if (-not [string]::IsNullOrWhiteSpace($candidate)) {
                return [System.IO.Path]::GetFullPath($candidate)
            }
        }
    }
    return $null
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

function Strip-AnsiText {
    param([Parameter(Mandatory = $true)][string]$Text)

    return [System.Text.RegularExpressions.Regex]::Replace($Text, "\x1B\[[0-9;]*[A-Za-z]", "")
}

function Resolve-BazelConfigValue {
    param([Parameter(Mandatory = $true)]$SyncPolicy)

    if (-not [string]::IsNullOrWhiteSpace($BazelConfig)) {
        return $BazelConfig
    }
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_BAZEL_CONFIG)) {
        return $env:KAIN_BAZEL_CONFIG
    }
    return [string](Get-HashValue -Table $SyncPolicy -Key "bazel_default_config_windows" -DefaultValue "dev")
}

function Invoke-BazelAndCaptureLastLine {
    param([Parameter(Mandatory = $true)][string[]]$Args)

    $escapedArgs = @()
    foreach ($arg in $Args) {
        $escapedArgs += ('"' + ($arg -replace '"', '\"') + '"')
    }
    $commandText = "bazel " + ($escapedArgs -join " ") + " 2>&1"
    $output = & cmd.exe /d /c $commandText
    if ($LASTEXITCODE -ne 0) {
        throw ("bazel " + ($Args -join " ") + " failed with exit code " + $LASTEXITCODE)
    }
    $lines = @(
        $output |
        ForEach-Object { Strip-AnsiText -Text ([string]$_) } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    if ($lines.Count -eq 0) {
        throw ("bazel " + ($Args -join " ") + " returned no output")
    }
    return $lines[$lines.Count - 1].Trim()
}

function Resolve-BazelBinaryPath {
    param(
        [Parameter(Mandatory = $true)][string]$Config,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $bazelBin = Invoke-BazelAndCaptureLastLine -Args @("info", "bazel-bin", "--config=$Config")
    $binaryPath = Join-Path $bazelBin ("crates/cli/" + $Name + ".exe")
    return [System.IO.Path]::GetFullPath($binaryPath)
}

$repoRoot = Resolve-RepoRoot
$runtimePolicy = Load-RuntimePolicy -RepoRoot $repoRoot
$syncPolicy = Get-HashValue -Table $runtimePolicy -Key "launcher_sync" -DefaultValue @{}
$resolvedConfig = Resolve-BazelConfigValue -SyncPolicy $syncPolicy
$stateRoot = Resolve-SyncStateRoot -SyncPolicy $syncPolicy
$stampPath = if (-not [string]::IsNullOrWhiteSpace($env:KAIN_SYNC_STAMP_PATH)) {
    [System.IO.Path]::GetFullPath($env:KAIN_SYNC_STAMP_PATH)
} else {
    Resolve-StatePath -StateRoot $stateRoot -SyncPolicy $syncPolicy -Key "stamp_relative_path" -DefaultRelative "state/kain_sync_stamp.json"
}
$runtimeStampFiles = Convert-ToStringArray -Value (Get-HashValue -Table $syncPolicy -Key "runtime_stamp_files" -DefaultValue @(
        "runtime/kain_runtime.c",
        "runtime/native_core_runtime.toml",
        "blades/kain-mcp/config/runtime_policy.json"
    ))
$sourceWatchPaths = Resolve-SourceWatchPaths -SyncPolicy $syncPolicy

$resolvedClangPath = Resolve-ClangPath -RepoRoot $repoRoot
$resolvedPythonPath = Resolve-Python312Path
$invocationLocation = Get-Location
$invocationWorkingDirectory = $null
if ($invocationLocation.Provider -and $invocationLocation.Provider.Name -eq "FileSystem") {
    $invocationWorkingDirectory = $invocationLocation.ProviderPath
}

$env:KAIN_REPO_ROOT = $repoRoot
$env:KAIN_STDLIB_PATH = (Join-Path $repoRoot "stdlib")
$env:KAIN_RUNTIME_C_PATH = (Join-Path $repoRoot "runtime\kain_runtime.c")
$env:KAIN_RUNTIME_MANIFEST_PATH = (Join-Path $repoRoot "runtime\native_core_runtime.toml")
$env:KAIN_SYNC_ROOT = $stateRoot
$env:KAIN_SYNC_STAMP_PATH = $stampPath
$env:KAIN_BAZEL_CONFIG = $resolvedConfig
$env:KAIN_ACTIVE_LAUNCHER_NAME = $BinaryName
$env:KAIN_ACTIVE_LAUNCHER_MODE = "bazel-wrapper"
if (-not [string]::IsNullOrWhiteSpace($LauncherPath)) {
    $env:KAIN_ACTIVE_LAUNCHER_PATH = [System.IO.Path]::GetFullPath($LauncherPath)
} elseif (-not [string]::IsNullOrWhiteSpace($env:KAIN_ACTIVE_LAUNCHER_PATH)) {
    $env:KAIN_ACTIVE_LAUNCHER_PATH = [System.IO.Path]::GetFullPath($env:KAIN_ACTIVE_LAUNCHER_PATH)
}

if ($resolvedClangPath) {
    $env:KAIN_CLANG_PATH = $resolvedClangPath
    $env:PATH = Set-PathPrefix -PathValue $env:PATH -Prefix (Split-Path -Parent $resolvedClangPath)
}
if ($resolvedPythonPath) {
    $env:PYO3_PYTHON = $resolvedPythonPath
    $env:PATH = Set-PathPrefix -PathValue $env:PATH -Prefix (Split-Path -Parent $resolvedPythonPath)
}

Push-Location $repoRoot
try {
    $existingStampPayload = Read-JsonFileCompat -Path $stampPath
    $sourceStampData = Get-SourceStampData -RepoRoot $repoRoot -WatchPaths $sourceWatchPaths
    $currentSourceStamp = [string](Get-HashValue -Table $sourceStampData -Key "stamp" -DefaultValue "")
    $currentSourceDirtyCount = [int](Get-HashValue -Table $sourceStampData -Key "dirty_count" -DefaultValue 0)
    $currentSourceWatchPaths = Convert-ToStringArray -Value (Get-HashValue -Table $sourceStampData -Key "watch_paths" -DefaultValue $sourceWatchPaths)

    $previousSourceStamp = [string](Get-HashValue -Table $existingStampPayload -Key "source_stamp" -DefaultValue "")
    $previousConfig = [string](Get-HashValue -Table $existingStampPayload -Key "bazel_config" -DefaultValue "")

    $resolvedBinaryPath = $null
    $stampedBinaryPath = Resolve-StampedBinaryPath -StampPayload $existingStampPayload -Name $BinaryName
    $stampedBinaryExists = $false
    if (-not [string]::IsNullOrWhiteSpace($stampedBinaryPath) -and (Test-Path $stampedBinaryPath)) {
        $stampedBinaryExists = $true
    }

    $shouldBuild = -not $SkipBuild
    $buildReason = if ($SkipBuild) { "skip-build flag set" } else { "source gate not evaluated" }
    if (-not $SkipBuild) {
        if ([string]::IsNullOrWhiteSpace($currentSourceStamp)) {
            $shouldBuild = $true
            $buildReason = "source stamp unavailable"
        } elseif ([string]::IsNullOrWhiteSpace($previousSourceStamp)) {
            $shouldBuild = $true
            $buildReason = "missing previous source stamp"
        } elseif ($previousSourceStamp -ne $currentSourceStamp) {
            $shouldBuild = $true
            $buildReason = "source stamp changed"
        } elseif ($previousConfig -ne $resolvedConfig) {
            $shouldBuild = $true
            $buildReason = "bazel config changed"
        } elseif (-not $stampedBinaryExists) {
            $shouldBuild = $true
            $buildReason = "stamped binary missing"
        } else {
            $shouldBuild = $false
            $buildReason = "source unchanged"
            $resolvedBinaryPath = $stampedBinaryPath
        }
    }

    if ($shouldBuild) {
        & bazel build ("//:" + $BinaryName) ("--config=" + $resolvedConfig)
        if ($LASTEXITCODE -ne 0) {
            throw ("bazel build //:" + $BinaryName + " --config=" + $resolvedConfig + " failed with exit code " + $LASTEXITCODE)
        }
        $resolvedBinaryPath = Resolve-BazelBinaryPath -Config $resolvedConfig -Name $BinaryName
    } elseif ([string]::IsNullOrWhiteSpace($resolvedBinaryPath)) {
        if ($stampedBinaryExists) {
            $resolvedBinaryPath = $stampedBinaryPath
        } else {
            $resolvedBinaryPath = Resolve-BazelBinaryPath -Config $resolvedConfig -Name $BinaryName
        }
    }

    if (-not (Test-Path $resolvedBinaryPath)) {
        throw ("Bazel binary not found at " + $resolvedBinaryPath)
    }

    $repoSha = Resolve-RepoHeadSha -RepoRoot $repoRoot
    $runtimeStamp = Get-RuntimeStamp -RepoRoot $repoRoot -RuntimeStampFiles $runtimeStampFiles
    $nowUnix = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $activeBinaryFingerprint = Get-BinaryFingerprint -Path $resolvedBinaryPath

    $binaryByName = @{}
    $existingBinaryByName = Get-HashValue -Table $existingStampPayload -Key "binary_by_name" -DefaultValue @{}
    if ($existingBinaryByName -is [hashtable]) {
        foreach ($entry in $existingBinaryByName.GetEnumerator()) {
            $binaryByName[$entry.Key] = $entry.Value
        }
    }
    $binaryByName[$BinaryName] = $activeBinaryFingerprint

    $legacyKainBinary = $null
    if ($BinaryName -eq "kain") {
        $legacyKainBinary = $activeBinaryFingerprint
    } elseif ($binaryByName.ContainsKey("kain")) {
        $legacyKainBinary = $binaryByName["kain"]
    } elseif ($existingStampPayload.ContainsKey("binary")) {
        $legacyKainBinary = $existingStampPayload["binary"]
    }

    $stampPayload = @{
        schema_version = 1
        repo_root = $repoRoot
        repo_sha = $repoSha
        runtime_stamp = $runtimeStamp
        runtime_stamp_files = $runtimeStampFiles
        binary_by_name = $binaryByName
        build_number = ("bazel-" + $resolvedConfig)
        synced_at_unix = $nowUnix
        last_attempt_unix = $nowUnix
        managed_sync = $false
        source_of_truth = "bazel-wrapper"
        bazel_config = $resolvedConfig
        source_stamp = $currentSourceStamp
        source_watch_paths = $currentSourceWatchPaths
        source_dirty_count = $currentSourceDirtyCount
        build_performed = $shouldBuild
        build_reason = $buildReason
    }
    if ($null -ne $legacyKainBinary) {
        $stampPayload["binary"] = $legacyKainBinary
    }
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_ACTIVE_LAUNCHER_PATH)) {
        $stampPayload["launcher_path"] = $env:KAIN_ACTIVE_LAUNCHER_PATH
    }
    Write-JsonAtomic -Path $stampPath -Payload $stampPayload

    if ($UpdateStampOnly) {
        exit 0
    }

    if ($ForwardArgs.Count -gt 0 -and $ForwardArgs[0] -eq "--") {
        if ($ForwardArgs.Count -gt 1) {
            $ForwardArgs = $ForwardArgs[1..($ForwardArgs.Count - 1)]
        } else {
            $ForwardArgs = @()
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($invocationWorkingDirectory)) {
        Set-Location -LiteralPath $invocationWorkingDirectory
    }

    & $resolvedBinaryPath @ForwardArgs
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
