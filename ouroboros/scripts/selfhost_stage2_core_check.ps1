param(
    [string]$Workspace = "",
    [string]$Crate = "kain-core",
    [switch]$Build,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$OuroborosRoot = Split-Path -Parent $PSScriptRoot
if (-not $Workspace) { $Workspace = Join-Path $OuroborosRoot "out\selfhost\phase2\stage2_workspace" }

if (-not (Test-Path $Workspace)) {
    throw "Stage2 workspace not found: $Workspace"
}

$cargoMode = if ($Build) { "build" } else { "check" }
$logName = if ($Build) { "stage2_${Crate}_build.log" } else { "stage2_${Crate}_check.log" }
$logPath = Join-Path $Workspace $logName
$stdoutPath = Join-Path $Workspace ($logName + ".stdout")
$stderrPath = Join-Path $Workspace ($logName + ".stderr")

Push-Location $Workspace
try {
    $args = "$cargoMode -p $Crate"
    if (Test-Path $stdoutPath) { Remove-Item $stdoutPath -Force }
    if (Test-Path $stderrPath) { Remove-Item $stderrPath -Force }
    $process = Start-Process -FilePath "cargo" -ArgumentList $args -NoNewWindow -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    $code = $process.ExitCode
    $stdout = if (Test-Path $stdoutPath) { Get-Content $stdoutPath -Raw } else { "" }
    $stderr = if (Test-Path $stderrPath) { Get-Content $stderrPath -Raw } else { "" }
    ($stdout + $stderr) | Set-Content -Path $logPath
    if (Test-Path $stdoutPath) { Remove-Item $stdoutPath -Force }
    if (Test-Path $stderrPath) { Remove-Item $stderrPath -Force }
    if (-not $Quiet) {
        Write-Host "Wrote log: $logPath"
    }
    exit $code
}
finally {
    Pop-Location
}
