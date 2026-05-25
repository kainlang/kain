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

$script:PyO3SupportedPythonMinors = @(12, 11, 10)
$script:PythonPollutionEnvKeys = @(
    "PYO3_PYTHON",
    "PYTHONHOME",
    "PYTHONPATH",
    "VIRTUAL_ENV",
    "CONDA_PREFIX",
    "CONDA_DEFAULT_ENV",
    "PYTHONEXECUTABLE",
    "__PYVENV_LAUNCHER__"
)

function Save-EnvironmentSnapshot {
    param([Parameter(Mandatory = $true)][string[]]$Names)

    $snapshot = [ordered]@{}
    foreach ($name in $Names) {
        $snapshot[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    }
    return $snapshot
}

function Restore-EnvironmentSnapshot {
    param([Parameter(Mandatory = $true)]$Snapshot)

    foreach ($entry in $Snapshot.GetEnumerator()) {
        if ($null -ne $entry.Value) {
            Set-Item -Path ("Env:" + $entry.Key) -Value $entry.Value
        } else {
            Remove-Item -Path ("Env:" + $entry.Key) -ErrorAction SilentlyContinue
        }
    }
}

function Invoke-WithSanitizedPythonEnvironment {
    param([Parameter(Mandatory = $true)][scriptblock]$ScriptBlock)

    $snapshot = Save-EnvironmentSnapshot -Names $script:PythonPollutionEnvKeys
    try {
        foreach ($name in $script:PythonPollutionEnvKeys) {
            Remove-Item -Path ("Env:" + $name) -ErrorAction SilentlyContinue
        }
        & $ScriptBlock
    } finally {
        Restore-EnvironmentSnapshot -Snapshot $snapshot
    }
}

function Resolve-CommandPath {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command -and $command.Source -and (Test-Path $command.Source)) {
        return $command.Source
    }

    return $null
}

function Resolve-ExistingPath {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return $null
    }

    try {
        if (Test-Path $Candidate) {
            return (Resolve-Path $Candidate).Path
        }
    } catch {
    }

    return $null
}

function Test-WindowsAppAliasPath {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return $false
    }

    return $Candidate -like "*WindowsApps*python*.exe"
}

function Resolve-PythonExecutableFromCommand {
    param(
        [Parameter(Mandatory = $true)][string]$CommandPath,
        [string[]]$Arguments = @()
    )

    try {
        $resolved = (Invoke-WithSanitizedPythonEnvironment -ScriptBlock {
                & $CommandPath @Arguments -c "import sys; print(sys.executable)" 2>$null
            } | Select-Object -First 1).Trim()
        return Resolve-ValidatedPythonPath -Candidate $resolved
    } catch {
        return $null
    }
}

function Resolve-ValidatedPythonPath {
    param([string]$Candidate)

    $existing = Resolve-ExistingPath -Candidate $Candidate
    if (-not $existing) {
        return $null
    }
    if (Test-WindowsAppAliasPath -Candidate $existing) {
        return $null
    }

    try {
        $version = (Invoke-WithSanitizedPythonEnvironment -ScriptBlock {
                & $existing -c "import sys; print(f'{sys.version_info[0]}.{sys.version_info[1]}')" 2>$null
            } | Select-Object -First 1).Trim()
        if ([string]::IsNullOrWhiteSpace($version)) {
            return $null
        }

        $parts = $version.Split('.')
        if ($parts.Count -lt 2) {
            return $null
        }

        $major = [int]$parts[0]
        $minor = [int]$parts[1]
        if ($major -ne 3 -or -not ($script:PyO3SupportedPythonMinors -contains $minor)) {
            return $null
        }

        $resolvedExecutable = (Invoke-WithSanitizedPythonEnvironment -ScriptBlock {
                & $existing -c "import sys; print(sys.executable)" 2>$null
            } | Select-Object -First 1).Trim()
        $resolvedExisting = Resolve-ExistingPath -Candidate $resolvedExecutable
        if ($resolvedExisting) {
            return $resolvedExisting
        }
    } catch {
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

function Resolve-ConfiguredPath {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$Value
    )

    $expanded = [Environment]::ExpandEnvironmentVariables($Value)
    if ($expanded.StartsWith("~")) {
        $expanded = Join-Path $HOME ($expanded.TrimStart("~\/"))
    }
    if (-not [System.IO.Path]::IsPathRooted($expanded)) {
        $expanded = Join-Path $RepoRoot $expanded
    }
    return [System.IO.Path]::GetFullPath($expanded)
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

function ConvertTo-BooleanValue {
    param(
        $Value,
        [bool]$DefaultValue = $false
    )

    if ($null -eq $Value) {
        return $DefaultValue
    }
    if ($Value -is [bool]) {
        return [bool]$Value
    }

    $normalized = ([string]$Value).Trim().ToLowerInvariant()
    if ([string]::IsNullOrWhiteSpace($normalized)) {
        return $DefaultValue
    }
    if (@("1", "true", "yes", "on", "enable", "enabled") -contains $normalized) {
        return $true
    }
    if (@("0", "false", "no", "off", "disable", "disabled") -contains $normalized) {
        return $false
    }
    return $DefaultValue
}

function Get-BooleanConfigValue {
    param(
        [Parameter(Mandatory = $true)]$Table,
        [Parameter(Mandatory = $true)][string]$Key,
        [bool]$DefaultValue = $false
    )

    if ($Table -is [hashtable] -and $Table.ContainsKey($Key)) {
        return ConvertTo-BooleanValue -Value $Table[$Key] -DefaultValue $DefaultValue
    }
    return $DefaultValue
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

function Resolve-PythonPath {
    $preferredMinors = $script:PyO3SupportedPythonMinors

    # Bazel should pick a stable PyO3 interpreter, not inherit whatever shell
    # virtualenv/conda/PYO3 state happened to be active.
    $overridePython = Resolve-ValidatedPythonPath -Candidate $env:KAIN_BAZEL_PYTHON
    if ($overridePython) {
        return $overridePython
    }

    $pyLauncher = Resolve-CommandPath -Name "py"
    if ($pyLauncher) {
        foreach ($minor in $preferredMinors) {
            $resolved = Resolve-PythonExecutableFromCommand -CommandPath $pyLauncher -Arguments @("-3.$minor")
            if ($resolved) {
                return $resolved
            }
        }

        $resolved = Resolve-PythonExecutableFromCommand -CommandPath $pyLauncher
        if ($resolved) {
            return $resolved
        }
    }

    foreach ($minor in $preferredMinors) {
        $versionSuffix = "3.$minor"
        $compactVersion = "3$minor"
        foreach ($candidate in @(
                (Join-Path $env:LOCALAPPDATA "Programs\Python\Python$compactVersion\python.exe"),
                (Join-Path $env:USERPROFILE "AppData\Local\Programs\Python\Python$compactVersion\python.exe"),
                "C:\Python$compactVersion\python.exe"
            )) {
            $resolved = Resolve-ValidatedPythonPath -Candidate $candidate
            if ($resolved) {
                return $resolved
            }
        }
    }

    $pythonFromPath = Resolve-CommandPath -Name "python"
    if ($pythonFromPath) {
        $resolved = Resolve-ValidatedPythonPath -Candidate $pythonFromPath
        if ($resolved) {
            return $resolved
        }
    }

    return $null
}

function Resolve-SyncStateRoot {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)]$SyncPolicy
    )

    $envKey = [string](Get-HashValue -Table $SyncPolicy -Key "state_root_env_key" -DefaultValue "KAIN_SYNC_ROOT")
    $envOverride = [string](Get-Item -Path ("Env:" + $envKey) -ErrorAction SilentlyContinue).Value
    if (-not [string]::IsNullOrWhiteSpace($envOverride)) {
        return Resolve-ConfiguredPath -RepoRoot $RepoRoot -Value $envOverride
    }

    $configured = [string](Get-HashValue -Table $SyncPolicy -Key "default_state_root_windows" -DefaultValue ".kain/state")
    return Resolve-ConfiguredPath -RepoRoot $RepoRoot -Value $configured
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
        [Parameter(Mandatory = $true)][string[]]$GitArgs
    )

    try {
        $repoRootForCmd = Join-Path $RepoRoot "."
        $commandText = 'git -C "' + $repoRootForCmd + '" ' + ($GitArgs -join " ") + ' 2>nul'
        $output = & cmd.exe /d /c $commandText
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
            "tools/bazel",
            "toolchain/rules_rust",
            "Cargo.toml",
            "Cargo.lock",
            "Cargo.Bazel.lock",
            "BUILD.bazel",
            "MODULE.bazel",
            "MODULE.bazel.lock",
            ".bazelrc",
            ".bazelversion",
            ".bazeliskrc"
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

function Test-PathMatchesWatchPath {
    param(
        [Parameter(Mandatory = $true)][string]$CandidatePath,
        [Parameter(Mandatory = $true)][string[]]$WatchPaths
    )

    foreach ($watchPath in $WatchPaths) {
        if ($CandidatePath.Equals($watchPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
        if ($CandidatePath.StartsWith(($watchPath + "/"), [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    return $false
}

function Resolve-SourceFilesystemWatchPaths {
    param([Parameter(Mandatory = $true)]$SyncPolicy)

    $configured = Convert-ToStringArray -Value (Get-HashValue -Table $SyncPolicy -Key "source_filesystem_watch_paths" -DefaultValue @(
            "toolchain/rules_rust"
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

function Get-FilesystemStampDescriptor {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    $candidate = Join-Path $RepoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $candidate)) {
        return ("fs|{0}|missing" -f $RelativePath)
    }

    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $item = Get-Item -LiteralPath $candidate -Force
        return ("fs|{0}|file|{1}|{2}" -f $RelativePath, $item.Length, $item.LastWriteTimeUtc.Ticks)
    }

    $entries = New-Object System.Collections.Generic.List[string]
    $entries.Add(("root|{0}|dir" -f $RelativePath))
    Get-ChildItem -LiteralPath $candidate -Recurse -Force | ForEach-Object {
        $fullPath = $_.FullName
        if ($fullPath.Length -le $RepoRoot.Length) {
            return
        }
        $relative = $fullPath.Substring($RepoRoot.Length).TrimStart('\', '/')
        $normalized = Normalize-RelativePath -PathText $relative
        if ([string]::IsNullOrWhiteSpace($normalized)) {
            return
        }
        if ($_.PSIsContainer) {
            $entries.Add(("dir|{0}" -f $normalized))
        } else {
            $entries.Add(("file|{0}|{1}|{2}" -f $normalized, $_.Length, $_.LastWriteTimeUtc.Ticks))
        }
    }
    $orderedEntries = @($entries)
    [Array]::Sort($orderedEntries, [System.StringComparer]::OrdinalIgnoreCase)
    return ("fs|{0}|{1}" -f $RelativePath, (Get-ShortSha256 -Text ([string]::Join("`n", $orderedEntries))))
}

function Get-SourceStampData {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string[]]$WatchPaths,
        [string[]]$FilesystemWatchPaths = @()
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

    $dirtyPaths = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
    $dirtyCommandArgs = @(
        @("diff", "--name-only"),
        @("diff", "--cached", "--name-only"),
        @("ls-files", "--others", "--exclude-standard")
    )
    foreach ($commandArgs in $dirtyCommandArgs) {
        $lines = Invoke-GitLines -RepoRoot $RepoRoot -GitArgs $commandArgs
        foreach ($line in $lines) {
            $normalized = Normalize-RelativePath -PathText $line
            if (-not [string]::IsNullOrWhiteSpace($normalized) -and (Test-PathMatchesWatchPath -CandidatePath $normalized -WatchPaths $normalizedWatchPaths)) {
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
    foreach ($relative in $FilesystemWatchPaths) {
        $normalized = Normalize-RelativePath -PathText ([string]$relative)
        if ([string]::IsNullOrWhiteSpace($normalized)) {
            continue
        }
        $stampLines.Add(("watch-fs|{0}" -f $normalized))
        $stampLines.Add((Get-FilesystemStampDescriptor -RepoRoot $RepoRoot -RelativePath $normalized))
    }
    $stamp = Get-ShortSha256 -Text ([string]::Join("`n", $stampLines))
    return @{
        stamp = $stamp
        dirty_count = $dirtyDescriptors.Count
        watch_paths = $normalizedWatchPaths
        filesystem_watch_paths = $FilesystemWatchPaths
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

function Resolve-StampedBinaryEntry {
    param(
        [Parameter(Mandatory = $true)]$StampPayload,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if ($StampPayload -isnot [hashtable]) {
        return $null
    }
    if (-not $StampPayload.ContainsKey("binary_by_name")) {
        return $null
    }

    $binaryByName = $StampPayload["binary_by_name"]
    if ($binaryByName -is [hashtable] -and $binaryByName.ContainsKey($Name)) {
        $entry = $binaryByName[$Name]
        if ($entry -is [hashtable]) {
            return $entry
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

function Copy-FileAtomically {
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

function Invoke-WithExclusiveFileLock {
    param(
        [Parameter(Mandatory = $true)][string]$LockPath,
        [int]$TimeoutSeconds = 300,
        [Parameter(Mandatory = $true)][scriptblock]$ScriptBlock
    )

    $directory = Split-Path -Parent $LockPath
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $lockHandle = $null
    while ($null -eq $lockHandle) {
        try {
            $lockHandle = [System.IO.File]::Open(
                $LockPath,
                [System.IO.FileMode]::OpenOrCreate,
                [System.IO.FileAccess]::ReadWrite,
                [System.IO.FileShare]::None
            )
        } catch {
            if ([DateTime]::UtcNow -ge $deadline) {
                throw ("Timed out waiting for lock " + $LockPath)
            }
            Start-Sleep -Milliseconds 500
        }
    }

    try {
        return & $ScriptBlock
    } finally {
        if ($null -ne $lockHandle) {
            $lockHandle.Dispose()
        }
    }
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
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Text
    )

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

function Get-BazelPythonEnvironmentArgs {
    param([string]$ResolvedPythonPath)

    if ([string]::IsNullOrWhiteSpace($ResolvedPythonPath)) {
        return @()
    }

    return @(
        ("--repo_env=PYO3_PYTHON=" + $ResolvedPythonPath),
        ("--action_env=PYO3_PYTHON=" + $ResolvedPythonPath)
    )
}

function Invoke-BazelCommandWithLiveOutput {
    param([Parameter(Mandatory = $true)][string[]]$BazelArgs)

    $BazelArgs = @(
        $BazelArgs |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    if ($BazelArgs.Count -eq 0) {
        throw "No Bazel arguments were provided."
    }

    $escapedArgs = @()
    foreach ($arg in $BazelArgs) {
        $escapedArgs += ('"' + ($arg -replace '"', '\"') + '"')
    }
    $commandText = "bazel " + ($escapedArgs -join " ") + " 2>&1"

    $script:LastBazelExitCode = 0
    $script:LastBazelCommandOutput = @()
    Invoke-WithSanitizedPythonEnvironment -ScriptBlock {
        $capturedOutput = @()
        & cmd.exe /d /c $commandText | Tee-Object -Variable capturedOutput | Out-Host
        $script:LastBazelExitCode = $LASTEXITCODE
        $script:LastBazelCommandOutput = @($capturedOutput)
    } | Out-Null

    $lines = @(
        $script:LastBazelCommandOutput |
        ForEach-Object { Strip-AnsiText -Text ([string]$_) }
    )
    return @{
        exit_code = $script:LastBazelExitCode
        output_lines = $lines
        output_text = [string]::Join("`n", $lines)
    }
}

function Resolve-StampedBazelBinaryPath {
    param(
        [Parameter(Mandatory = $true)]$StampPayload,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $entry = Resolve-StampedBinaryEntry -StampPayload $StampPayload -Name $Name
    if ($null -ne $entry) {
        $candidate = [string](Get-HashValue -Table $entry -Key "bazel_path" -DefaultValue "")
        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    return Resolve-StampedBinaryPath -StampPayload $StampPayload -Name $Name
}

function Resolve-StagedBinaryPath {
    param(
        [Parameter(Mandatory = $true)][string]$StateRoot,
        [Parameter(Mandatory = $true)]$SyncPolicy,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Config,
        [Parameter(Mandatory = $true)][string]$SourceStamp,
        [Parameter(Mandatory = $true)]$BazelFingerprint
    )

    $relativeDir = [string](Get-HashValue -Table $SyncPolicy -Key "staged_binary_relative_dir" -DefaultValue "bin")
    $versionToken = "{0}-{1}" -f [string](Get-HashValue -Table $BazelFingerprint -Key "mtime_unix" -DefaultValue "0"), [string](Get-HashValue -Table $BazelFingerprint -Key "size_bytes" -DefaultValue "0")
    $fileName = "{0}-{1}-{2}-{3}.exe" -f $Name, $Config, $SourceStamp, $versionToken
    return [System.IO.Path]::GetFullPath((Join-Path (Join-Path $StateRoot $relativeDir) $fileName))
}

function Test-CargoBazelRepinMismatch {
    param([Parameter(Mandatory = $true)]$BazelResult)

    $outputText = [string](Get-HashValue -Table $BazelResult -Key "output_text" -DefaultValue "")
    if ([string]::IsNullOrWhiteSpace($outputText)) {
        return $false
    }

    return $outputText.Contains("out of date for 'crates'") -and
        $outputText.Contains("CARGO_BAZEL_REPIN=true") -and
        ($outputText.Contains("Digests do not match:") -or $outputText.Contains("crate_universe"))
}

function Invoke-BazelBuildWithCargoRepinRecovery {
    param(
        [Parameter(Mandatory = $true)][string]$BinaryName,
        [Parameter(Mandatory = $true)][string]$Config,
        [string[]]$ExtraBazelArgs = @(),
        [bool]$AutoRepinEnabled = $true,
        [string]$RepinLockPath = "",
        [int]$RepinLockTimeoutSeconds = 300
    )

    $buildArgs = @("build", ("//:" + $BinaryName), ("--config=" + $Config)) + $ExtraBazelArgs
    $buildResult = Invoke-BazelCommandWithLiveOutput -BazelArgs $buildArgs
    if ([int](Get-HashValue -Table $buildResult -Key "exit_code" -DefaultValue 1) -eq 0) {
        return @{
            exit_code = 0
            auto_repin = $false
        }
    }

    if (-not $AutoRepinEnabled -or -not (Test-CargoBazelRepinMismatch -BazelResult $buildResult)) {
        return @{
            exit_code = [int](Get-HashValue -Table $buildResult -Key "exit_code" -DefaultValue 1)
            auto_repin = $false
        }
    }

    Write-Host "[kain] Cargo.Bazel.lock drift detected; repinning crate_universe and retrying once..." -ForegroundColor Yellow
    Invoke-WithExclusiveFileLock -LockPath $RepinLockPath -TimeoutSeconds $RepinLockTimeoutSeconds -ScriptBlock {
        $repinSnapshot = Save-EnvironmentSnapshot -Names @("CARGO_BAZEL_REPIN")
        try {
            Set-Item -Path Env:CARGO_BAZEL_REPIN -Value "true"
            $repinArgs = @("fetch", ("//:" + $BinaryName), ("--config=" + $Config)) + $ExtraBazelArgs
            $repinResult = Invoke-BazelCommandWithLiveOutput -BazelArgs $repinArgs
            if ([int](Get-HashValue -Table $repinResult -Key "exit_code" -DefaultValue 1) -ne 0) {
                throw ("auto-repin failed while running bazel " + ($repinArgs -join " "))
            }
        } finally {
            Restore-EnvironmentSnapshot -Snapshot $repinSnapshot
        }
    }

    Write-Host ("[kain] Cargo.Bazel.lock refreshed; retrying bazel build //:{0} --config={1}..." -f $BinaryName, $Config) -ForegroundColor Yellow
    $retryResult = Invoke-BazelCommandWithLiveOutput -BazelArgs $buildArgs
    return @{
        exit_code = [int](Get-HashValue -Table $retryResult -Key "exit_code" -DefaultValue 1)
        auto_repin = $true
    }
}

function Invoke-BazelAndCaptureLastLine {
    param([Parameter(Mandatory = $true)][string[]]$BazelArgs)

    $BazelArgs = @(
        $BazelArgs |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    if ($BazelArgs.Count -eq 0) {
        throw "No Bazel arguments were provided."
    }

    $escapedArgs = @()
    foreach ($arg in $BazelArgs) {
        $escapedArgs += ('"' + ($arg -replace '"', '\"') + '"')
    }
    $commandText = "bazel " + ($escapedArgs -join " ") + " 2>&1"
    $exitCode = 0
    $output = Invoke-WithSanitizedPythonEnvironment -ScriptBlock {
        & cmd.exe /d /c $commandText
        $script:LastBazelCaptureExitCode = $LASTEXITCODE
    }
    $exitCode = $script:LastBazelCaptureExitCode
    if ($exitCode -ne 0) {
        throw ("bazel " + ($BazelArgs -join " ") + " failed with exit code " + $exitCode)
    }
    $lines = @(
        $output |
        ForEach-Object { Strip-AnsiText -Text ([string]$_) } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    if ($lines.Count -eq 0) {
        throw ("bazel " + ($BazelArgs -join " ") + " returned no output")
    }
    return $lines[$lines.Count - 1].Trim()
}

function Resolve-BazelBinaryPath {
    param(
        [Parameter(Mandatory = $true)][string]$Config,
        [Parameter(Mandatory = $true)][string]$Name,
        [string[]]$ExtraBazelArgs = @()
    )

    $ExtraBazelArgs = @(
        $ExtraBazelArgs |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    $bazelBin = Invoke-BazelAndCaptureLastLine -BazelArgs (@("info", "bazel-bin", "--config=$Config") + $ExtraBazelArgs)
    $binaryPath = Join-Path $bazelBin ("crates/cli/" + $Name + ".exe")
    return [System.IO.Path]::GetFullPath($binaryPath)
}

$repoRoot = Resolve-RepoRoot
$runtimePolicy = Load-RuntimePolicy -RepoRoot $repoRoot
$syncPolicy = Get-HashValue -Table $runtimePolicy -Key "launcher_sync" -DefaultValue @{}
$resolvedConfig = Resolve-BazelConfigValue -SyncPolicy $syncPolicy
$stateRoot = Resolve-SyncStateRoot -RepoRoot $repoRoot -SyncPolicy $syncPolicy
$autoCargoBazelRepin = Get-BooleanConfigValue -Table $syncPolicy -Key "cargo_bazel_auto_repin_enabled" -DefaultValue $true
$cargoBazelRepinLockPath = Resolve-StatePath -StateRoot $stateRoot -SyncPolicy $syncPolicy -Key "cargo_bazel_repin_lock_relative_path" -DefaultRelative "locks/cargo-bazel-repin.lock"
$cargoBazelRepinLockTimeoutSeconds = [int](Get-HashValue -Table $syncPolicy -Key "cargo_bazel_repin_lock_timeout_seconds" -DefaultValue 300)
$stampPath = if (-not [string]::IsNullOrWhiteSpace($env:KAIN_SYNC_STAMP_PATH)) {
    [System.IO.Path]::GetFullPath($env:KAIN_SYNC_STAMP_PATH)
} else {
    Resolve-StatePath -StateRoot $stateRoot -SyncPolicy $syncPolicy -Key "stamp_relative_path" -DefaultRelative "state/kain_sync_stamp.json"
}
$runtimeStampFiles = Convert-ToStringArray -Value (Get-HashValue -Table $syncPolicy -Key "runtime_stamp_files" -DefaultValue @(
        "runtime/runtime.c",
        "runtime/native_core_runtime.toml",
        "blades/kain-mcp/config/runtime_policy.json"
    ))
$sourceWatchPaths = Resolve-SourceWatchPaths -SyncPolicy $syncPolicy
$sourceFilesystemWatchPaths = Resolve-SourceFilesystemWatchPaths -SyncPolicy $syncPolicy

$resolvedClangPath = Resolve-ClangPath -RepoRoot $repoRoot
$resolvedPythonPath = Resolve-PythonPath
$bazelPythonEnvArgs = Get-BazelPythonEnvironmentArgs -ResolvedPythonPath $resolvedPythonPath
$repoKainHome = [System.IO.Path]::GetFullPath((Join-Path $repoRoot ".kain"))
$repoKainConfigPath = Join-Path $repoKainHome "config.toml"
$invocationLocation = Get-Location
$invocationWorkingDirectory = $null
if ($invocationLocation.Provider -and $invocationLocation.Provider.Name -eq "FileSystem") {
    $invocationWorkingDirectory = $invocationLocation.ProviderPath
}

$env:KAIN_REPO_ROOT = $repoRoot
$env:KAIN_HOME = $repoKainHome
$env:KAIN_CONFIG = $repoKainConfigPath
$env:KAIN_STDLIB_PATH = (Join-Path $repoRoot "stdlib")
$env:KAIN_RUNTIME_C_PATH = (Join-Path $repoRoot "runtime\runtime.c")
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

Push-Location $repoRoot
try {
    $existingStampPayload = Read-JsonFileCompat -Path $stampPath
    $sourceStampData = Get-SourceStampData -RepoRoot $repoRoot -WatchPaths $sourceWatchPaths -FilesystemWatchPaths $sourceFilesystemWatchPaths
    $currentSourceStamp = [string](Get-HashValue -Table $sourceStampData -Key "stamp" -DefaultValue "")
    $currentSourceDirtyCount = [int](Get-HashValue -Table $sourceStampData -Key "dirty_count" -DefaultValue 0)
    $currentSourceWatchPaths = Convert-ToStringArray -Value (Get-HashValue -Table $sourceStampData -Key "watch_paths" -DefaultValue $sourceWatchPaths)
    $currentSourceFilesystemWatchPaths = Convert-ToStringArray -Value (Get-HashValue -Table $sourceStampData -Key "filesystem_watch_paths" -DefaultValue $sourceFilesystemWatchPaths)

    $previousSourceStamp = [string](Get-HashValue -Table $existingStampPayload -Key "source_stamp" -DefaultValue "")
    $previousConfig = [string](Get-HashValue -Table $existingStampPayload -Key "bazel_config" -DefaultValue "")

    $resolvedBinaryPath = $null
    $resolvedBazelBinaryPath = $null
    $stampedBinaryEntry = Resolve-StampedBinaryEntry -StampPayload $existingStampPayload -Name $BinaryName
    $stampedBinaryPath = Resolve-StampedBinaryPath -StampPayload $existingStampPayload -Name $BinaryName
    $stampedBazelBinaryPath = Resolve-StampedBazelBinaryPath -StampPayload $existingStampPayload -Name $BinaryName
    $stampedBinaryExists = $false
    if (-not [string]::IsNullOrWhiteSpace($stampedBinaryPath) -and (Test-Path $stampedBinaryPath)) {
        $stampedBinaryExists = $true
    }
    $stampedBazelBinaryExists = $false
    if (-not [string]::IsNullOrWhiteSpace($stampedBazelBinaryPath) -and (Test-Path $stampedBazelBinaryPath)) {
        $stampedBazelBinaryExists = $true
    }
    $stampedBinarySourceStamp = if ($null -ne $stampedBinaryEntry) {
        [string](Get-HashValue -Table $stampedBinaryEntry -Key "source_stamp" -DefaultValue "")
    } else {
        ""
    }
    $stampedBinaryConfig = if ($null -ne $stampedBinaryEntry) {
        [string](Get-HashValue -Table $stampedBinaryEntry -Key "bazel_config" -DefaultValue "")
    } else {
        ""
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
        } elseif ($null -eq $stampedBinaryEntry) {
            $shouldBuild = $true
            $buildReason = "missing stamped binary entry"
        } elseif ([string]::IsNullOrWhiteSpace($stampedBinarySourceStamp)) {
            $shouldBuild = $true
            $buildReason = "missing per-binary source stamp"
        } elseif ($stampedBinarySourceStamp -ne $currentSourceStamp) {
            $shouldBuild = $true
            $buildReason = "binary source stamp changed"
        } elseif ($previousSourceStamp -ne $currentSourceStamp) {
            $shouldBuild = $true
            $buildReason = "source stamp changed"
        } elseif ([string]::IsNullOrWhiteSpace($stampedBinaryConfig)) {
            $shouldBuild = $true
            $buildReason = "missing per-binary bazel config"
        } elseif ($stampedBinaryConfig -ne $resolvedConfig) {
            $shouldBuild = $true
            $buildReason = "binary bazel config changed"
        } elseif ($previousConfig -ne $resolvedConfig) {
            $shouldBuild = $true
            $buildReason = "bazel config changed"
        } elseif (-not $stampedBazelBinaryExists) {
            $shouldBuild = $true
            $buildReason = "stamped bazel binary missing"
        } else {
            $shouldBuild = $false
            $buildReason = "source unchanged"
            $resolvedBazelBinaryPath = $stampedBazelBinaryPath
        }
    }

    if ($shouldBuild) {
        $buildResult = Invoke-BazelBuildWithCargoRepinRecovery -BinaryName $BinaryName -Config $resolvedConfig -ExtraBazelArgs $bazelPythonEnvArgs -AutoRepinEnabled $autoCargoBazelRepin -RepinLockPath $cargoBazelRepinLockPath -RepinLockTimeoutSeconds $cargoBazelRepinLockTimeoutSeconds
        $script:LastBazelExitCode = [int](Get-HashValue -Table $buildResult -Key "exit_code" -DefaultValue 1)
        if ($script:LastBazelExitCode -ne 0) {
            throw ("bazel build //:" + $BinaryName + " --config=" + $resolvedConfig + " failed with exit code " + $script:LastBazelExitCode)
        }
        $resolvedBazelBinaryPath = Resolve-BazelBinaryPath -Config $resolvedConfig -Name $BinaryName -ExtraBazelArgs $bazelPythonEnvArgs
    } elseif ([string]::IsNullOrWhiteSpace($resolvedBazelBinaryPath)) {
        if ($stampedBazelBinaryExists) {
            $resolvedBazelBinaryPath = $stampedBazelBinaryPath
        } else {
            $resolvedBazelBinaryPath = Resolve-BazelBinaryPath -Config $resolvedConfig -Name $BinaryName -ExtraBazelArgs $bazelPythonEnvArgs
        }
    }

    if (-not (Test-Path $resolvedBazelBinaryPath)) {
        throw ("Bazel binary not found at " + $resolvedBazelBinaryPath)
    }

    $repoSha = Resolve-RepoHeadSha -RepoRoot $repoRoot
    $runtimeStamp = Get-RuntimeStamp -RepoRoot $repoRoot -RuntimeStampFiles $runtimeStampFiles
    $nowUnix = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $bazelBinaryFingerprint = Get-BinaryFingerprint -Path $resolvedBazelBinaryPath
    $stagedBinaryPath = Resolve-StagedBinaryPath -StateRoot $stateRoot -SyncPolicy $syncPolicy -Name $BinaryName -Config $resolvedConfig -SourceStamp $currentSourceStamp -BazelFingerprint $bazelBinaryFingerprint
    if (-not (Test-Path $stagedBinaryPath)) {
        Copy-FileAtomically -SourcePath $resolvedBazelBinaryPath -DestinationPath $stagedBinaryPath
    }
    $resolvedBinaryPath = $stagedBinaryPath
    $activeBinaryFingerprint = Get-BinaryFingerprint -Path $resolvedBinaryPath

    $binaryByName = @{}
    $existingBinaryByName = Get-HashValue -Table $existingStampPayload -Key "binary_by_name" -DefaultValue @{}
    if ($existingBinaryByName -is [hashtable]) {
        foreach ($entry in $existingBinaryByName.GetEnumerator()) {
            $binaryByName[$entry.Key] = $entry.Value
        }
    }
    $binaryByName[$BinaryName] = @{
        path = $activeBinaryFingerprint["path"]
        bazel_path = $resolvedBazelBinaryPath
        exists = $activeBinaryFingerprint["exists"]
        size_bytes = $activeBinaryFingerprint["size_bytes"]
        mtime_unix = $activeBinaryFingerprint["mtime_unix"]
        source_stamp = $currentSourceStamp
        bazel_config = $resolvedConfig
        runtime_stamp = $runtimeStamp
        repo_sha = $repoSha
        synced_at_unix = $nowUnix
    }

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
        runtime_stamp_files = @($runtimeStampFiles)
        binary_by_name = $binaryByName
        build_number = ("bazel-" + $resolvedConfig)
        synced_at_unix = $nowUnix
        last_attempt_unix = $nowUnix
        managed_sync = $false
        source_of_truth = "bazel-wrapper"
        bazel_config = $resolvedConfig
        source_stamp = $currentSourceStamp
        source_watch_paths = @($currentSourceWatchPaths)
        source_filesystem_watch_paths = @($currentSourceFilesystemWatchPaths)
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
