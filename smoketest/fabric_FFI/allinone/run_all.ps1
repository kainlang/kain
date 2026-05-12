param(
    [string[]]$Pipelines,
    [switch]$StopOnError
)

$ErrorActionPreference = "Stop"

$SmokeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $SmokeRoot "..\..")).Path
$ManifestPath = Join-Path $SmokeRoot "pipeline_manifest.json"
$ResultsJsonPath = Join-Path $SmokeRoot "results\last_run_summary.json"
$ResultsMdPath = Join-Path $SmokeRoot "results\last_run_summary.md"
$LogsRoot = Join-Path $SmokeRoot "outputs\logs"

function Resolve-Binary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Tool
    )

    $candidates = switch ($Tool) {
        "kain" {
            @(
                (Join-Path $RepoRoot "target\debug\kain.exe"),
                (Join-Path $RepoRoot "target\release\kain.exe")
            )
        }
        "clang" {
            @(
                (Join-Path $RepoRoot "toolchain\llvm\bin\clang.exe")
            )
        }
        default { @() }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    $fromPath = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    throw "Unable to resolve required tool '$Tool'."
}

function Resolve-WorkingDirectory {
    param([string]$RelativePath)

    if ([string]::IsNullOrWhiteSpace($RelativePath)) {
        return $SmokeRoot
    }

    return (Resolve-Path (Join-Path $SmokeRoot $RelativePath)).Path
}

function Resolve-ExpectedPath {
    param([string]$RelativePath)

    return (Join-Path $SmokeRoot $RelativePath)
}

function Ensure-ParentDirectory {
    param([string]$Path)

    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
}

function Remove-PathIfExists {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        return
    }

    Remove-Item -Path $Path -Recurse -Force
}

function Invoke-CleanupPaths {
    param([object[]]$CleanupPaths)

    foreach ($relativePath in $CleanupPaths) {
        if ([string]::IsNullOrWhiteSpace([string]$relativePath)) {
            continue
        }

        $absolutePath = Resolve-ExpectedPath -RelativePath $relativePath
        Remove-PathIfExists -Path $absolutePath
    }
}

function Invoke-LoggedCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,
        [Parameter(Mandatory = $true)]
        [string]$Executable,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    Ensure-ParentDirectory -Path $LogPath
    $commandLine = @($Executable) + $Arguments
    $commandText = $commandLine -join " "
    "[$Label]" | Set-Content -Path $LogPath
    "cwd: $WorkingDirectory" | Add-Content -Path $LogPath
    "command: $commandText" | Add-Content -Path $LogPath
    "" | Add-Content -Path $LogPath

    Push-Location $WorkingDirectory
    try {
        & $Executable @Arguments *>&1 | Tee-Object -FilePath $LogPath -Append
        $exitCode = if ($null -ne $LASTEXITCODE) { [int]$LASTEXITCODE } else { 0 }
    }
    finally {
        Pop-Location
    }

    return @{
        command = $commandText
        exit_code = $exitCode
        log_path = $LogPath
    }
}

function Test-ExpectedOutputs {
    param([object[]]$ExpectedOutputs)

    $status = @()
    foreach ($relativePath in $ExpectedOutputs) {
        $absolutePath = Resolve-ExpectedPath -RelativePath $relativePath
        $status += @{
            path = $absolutePath
            exists = (Test-Path $absolutePath)
        }
    }
    return $status
}

if (-not (Test-Path $LogsRoot)) {
    New-Item -ItemType Directory -Force -Path $LogsRoot | Out-Null
}

$manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json
$resolvedKain = $null
$resolvedClang = $null

$allPipelines = @($manifest.pipelines)
if ($Pipelines -and $Pipelines.Count -gt 0) {
    $selected = @()
    foreach ($requestedId in $Pipelines) {
        $match = $allPipelines | Where-Object { $_.id -eq $requestedId }
        if (-not $match) {
            throw "Unknown pipeline id '$requestedId'."
        }
        $selected += $match
    }
    $allPipelines = $selected
}

$results = @()

foreach ($pipeline in $allPipelines) {
    $label = [string]$pipeline.id
    $workingDirectory = Resolve-WorkingDirectory -RelativePath ([string]$pipeline.working_directory)
    $logPath = Join-Path $LogsRoot ("{0}.log" -f $label)
    $args = @($pipeline.args | ForEach-Object { [string]$_ })
    Invoke-CleanupPaths -CleanupPaths @($pipeline.cleanup_paths)
    $executable = switch ([string]$pipeline.type) {
        "kain" {
            if (-not $resolvedKain) {
                $resolvedKain = Resolve-Binary -Tool "kain"
            }
            $resolvedKain
        }
        "process" {
            if ([string]$pipeline.executable -eq "clang") {
                if (-not $resolvedClang) {
                    $resolvedClang = Resolve-Binary -Tool "clang"
                }
                $resolvedClang
            } else {
                Resolve-Binary -Tool ([string]$pipeline.executable)
            }
        }
        default { throw "Unsupported pipeline type '$($pipeline.type)'." }
    }

    Write-Host ""
    Write-Host "=== $label ==="
    Write-Host $pipeline.description

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $commandResult = Invoke-LoggedCommand `
        -Label $label `
        -WorkingDirectory $workingDirectory `
        -Executable $executable `
        -Arguments $args `
        -LogPath $logPath
    $stopwatch.Stop()

    $expectedStatus = Test-ExpectedOutputs -ExpectedOutputs @($pipeline.expected_outputs)
    $missingOutputs = @($expectedStatus | Where-Object { -not $_.exists })
    $succeeded = ($commandResult.exit_code -eq 0) -and ($missingOutputs.Count -eq 0)

    $entry = [ordered]@{
        id = $label
        description = [string]$pipeline.description
        command = $commandResult.command
        exit_code = $commandResult.exit_code
        duration_ms = $stopwatch.ElapsedMilliseconds
        succeeded = $succeeded
        log_path = $commandResult.log_path
        expected_outputs = $expectedStatus
    }
    $results += $entry

    if ($succeeded) {
        Write-Host "status: OK"
    } else {
        Write-Host "status: FAILED"
        if ($missingOutputs.Count -gt 0) {
            Write-Host "missing outputs:"
            foreach ($missing in $missingOutputs) {
                Write-Host ("  - {0}" -f $missing.path)
            }
        }
        if ($StopOnError) {
            break
        }
    }
}

$summary = [ordered]@{
    schema_version = 1
    smoke_root = $SmokeRoot
    repo_root = $RepoRoot
    kain_binary = $resolvedKain
    clang_binary = $resolvedClang
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    pipeline_count = $results.Count
    success_count = @($results | Where-Object { $_.succeeded }).Count
    failure_count = @($results | Where-Object { -not $_.succeeded }).Count
    results = $results
}

Ensure-ParentDirectory -Path $ResultsJsonPath
$summary | ConvertTo-Json -Depth 8 | Set-Content -Path $ResultsJsonPath

$markdown = @()
$markdown += "# All-In-One Smoke Summary"
$markdown += ""
$markdown += "- generated_at_utc: $($summary.generated_at_utc)"
$markdown += "- kain_binary: $($summary.kain_binary)"
$markdown += "- clang_binary: $($summary.clang_binary)"
$markdown += "- pipeline_count: $($summary.pipeline_count)"
$markdown += "- success_count: $($summary.success_count)"
$markdown += "- failure_count: $($summary.failure_count)"
$markdown += ""
$markdown += "## Pipelines"
$markdown += ""

foreach ($result in $results) {
    $status = if ($result.succeeded) { "OK" } else { "FAILED" }
    $markdown += "### $($result.id)"
    $markdown += ""
    $markdown += "- status: $status"
    $markdown += "- exit_code: $($result.exit_code)"
    $markdown += "- duration_ms: $($result.duration_ms)"
    $markdown += "- log_path: $($result.log_path)"
    $markdown += '- command: `' + $result.command + '`'
    if ($result.expected_outputs.Count -gt 0) {
        foreach ($output in $result.expected_outputs) {
            $exists = if ($output.exists) { "present" } else { "missing" }
            $markdown += "- output: [$exists] $($output.path)"
        }
    }
    $markdown += ""
}

$markdown -join [Environment]::NewLine | Set-Content -Path $ResultsMdPath

$failed = @($results | Where-Object { -not $_.succeeded })
if ($failed.Count -gt 0) {
    exit 1
}

exit 0
