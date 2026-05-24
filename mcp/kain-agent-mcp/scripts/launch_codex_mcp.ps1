$ErrorActionPreference = "Stop"

$repoRoot = "D:\Kain-Lang"
$mcpRoot = Join-Path $repoRoot "mcp\kain-agent-mcp"
$launcherPath = $MyInvocation.MyCommand.Path

$bazelKain = if (-not [string]::IsNullOrWhiteSpace($env:KAIN_AGENT_KAIN_BIN)) {
    $env:KAIN_AGENT_KAIN_BIN
} else {
    "D:\Kain-Bazel\bin\kain.exe"
}

function Resolve-BazelBuiltKain {
    if (-not [string]::IsNullOrWhiteSpace($env:KAIN_AGENT_BUILT_KAIN_BIN) -and (Test-Path $env:KAIN_AGENT_BUILT_KAIN_BIN)) {
        return $env:KAIN_AGENT_BUILT_KAIN_BIN
    }

    $candidateRoot = "D:\Kain-Bazel\output-user-root"
    if (-not (Test-Path $candidateRoot)) {
        throw "Bazel output root was not found: $candidateRoot"
    }

    $matches = Get-ChildItem -Path $candidateRoot -Recurse -File -Filter kain.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like "*\bazel-out\*\bin\crates\cli\kain.exe" } |
        Sort-Object LastWriteTimeUtc -Descending

    if ($matches -and $matches.Count -gt 0) {
        return $matches[0].FullName
    }

    throw "Could not locate the Bazel-built Kain CLI binary."
}

function Resolve-PlanningKain {
    try {
        return (Resolve-BazelBuiltKain)
    }
    catch {
        return $bazelKain
    }
}

function Normalize-LongPath {
    param([Parameter(Mandatory = $true)][string]$PathValue)

    if ($PathValue.StartsWith("\\?\")) {
        return $PathValue.Substring(4)
    }
    return $PathValue
}

function Invoke-RunPlan {
    $planningKain = Resolve-PlanningKain
    $json = & $planningKain run $mcpRoot --target llvm --dry-run --json
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to resolve Kain MCP run plan."
    }
    return $json | ConvertFrom-Json
}

function Get-PlannedUnit {
    $plan = Invoke-RunPlan
    if (-not $plan.units -or $plan.units.Count -eq 0) {
        throw "Run plan did not expose any execution units."
    }
    return $plan.units[0]
}

function Get-WatchedFiles {
    $files = @(
        (Join-Path $mcpRoot "KAIN.toml"),
        (Join-Path $mcpRoot "config\server.json"),
        $launcherPath
    )

    Get-ChildItem -Path (Join-Path $mcpRoot "src") -Recurse -File | ForEach-Object {
        $files += $_.FullName
    }

    return $files
}

function Test-RebuildRequired {
    param([Parameter(Mandatory = $true)][string]$ExecutablePath)

    if (-not (Test-Path $ExecutablePath)) {
        return $true
    }

    $exeTime = (Get-Item $ExecutablePath).LastWriteTimeUtc
    foreach ($file in (Get-WatchedFiles)) {
        if ((Test-Path $file) -and ((Get-Item $file).LastWriteTimeUtc -gt $exeTime)) {
            return $true
        }
    }

    return $false
}

function Invoke-BazelWarmBuild {
    $oldBootMode = $env:KAIN_AGENT_MCP_BOOT_MODE
    $oldRepoRoot = $env:KAIN_AGENT_REPO_ROOT
    $oldMcpRoot = $env:KAIN_AGENT_MCP_ROOT
    $oldKainBin = $env:KAIN_AGENT_KAIN_BIN

    try {
        $env:KAIN_AGENT_MCP_BOOT_MODE = "exit"
        $env:KAIN_AGENT_REPO_ROOT = $repoRoot
        $env:KAIN_AGENT_MCP_ROOT = $mcpRoot
        $env:KAIN_AGENT_KAIN_BIN = $bazelKain
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $bazelKain
        $psi.Arguments = ('run "{0}" --target llvm' -f $mcpRoot)
        $psi.WorkingDirectory = $repoRoot
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.RedirectStandardInput = $true
        $psi.CreateNoWindow = $true

        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $psi
        $process.Start() | Out-Null
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()

        if (-not [string]::IsNullOrWhiteSpace($stdout.Result)) {
            [Console]::Error.Write($stdout.Result)
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr.Result)) {
            [Console]::Error.Write($stderr.Result)
        }
        if ($process.ExitCode -ne 0) {
            throw "Bazel Kain MCP warm build failed with exit code $($process.ExitCode)."
        }
    }
    finally {
        $env:KAIN_AGENT_MCP_BOOT_MODE = $oldBootMode
        $env:KAIN_AGENT_REPO_ROOT = $oldRepoRoot
        $env:KAIN_AGENT_MCP_ROOT = $oldMcpRoot
        $env:KAIN_AGENT_KAIN_BIN = $oldKainBin
    }
}

$unit = Get-PlannedUnit
$plannedExe = Normalize-LongPath $unit.process.executable
$plannedCwd = Normalize-LongPath $unit.process.current_working_directory

if (Test-RebuildRequired $plannedExe) {
    Invoke-BazelWarmBuild
    $unit = Get-PlannedUnit
    $plannedExe = Normalize-LongPath $unit.process.executable
    $plannedCwd = Normalize-LongPath $unit.process.current_working_directory
}

if (-not (Test-Path $plannedExe)) {
    throw "Resolved Kain MCP executable does not exist: $plannedExe"
}

$toolKain = Resolve-BazelBuiltKain
$env:KAIN_AGENT_MCP_BOOT_MODE = "stdio"
$env:KAIN_AGENT_REPO_ROOT = $repoRoot
$env:KAIN_AGENT_MCP_ROOT = $mcpRoot
$env:KAIN_AGENT_KAIN_BIN = $toolKain

Push-Location $plannedCwd
try {
    & $plannedExe
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
