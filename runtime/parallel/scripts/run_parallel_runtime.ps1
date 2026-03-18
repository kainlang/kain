Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$workspaceRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$configPath = Join-Path $workspaceRoot "runtime\parallel\config\toolchains.json"
$config = Get-Content -Raw $configPath | ConvertFrom-Json

$reportDir = Join-Path $workspaceRoot $config.outputs.report_dir
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$rustReport = Join-Path $reportDir $config.outputs.rust_report
$zigReport = Join-Path $reportDir $config.outputs.zig_report
$combinedReport = Join-Path $reportDir $config.outputs.combined_report

Push-Location $workspaceRoot
try {
    $rustOutput = cargo run -p kain-runtime-parallel -- report
    $rustPath = ($rustOutput | Select-Object -Last 1).Trim()

    Push-Location (Join-Path $workspaceRoot "runtime\parallel\zig")
    try {
        $zigJson = (& $config.tools.zig.command build run -- json 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0) {
            throw "zig pipeline failed with exit code $LASTEXITCODE"
        }
        Set-Content -Path $zigReport -Value $zigJson -Encoding utf8
    }
    finally {
        Pop-Location
    }

    $rustJson = Get-Content -Raw $rustPath | ConvertFrom-Json
    $zigJsonObj = Get-Content -Raw $zigReport | ConvertFrom-Json

    $combined = [ordered]@{
        generated_at = (Get-Date).ToString("o")
        rust_report = $rustJson
        zig_report = $zigJsonObj
    }

    $combined | ConvertTo-Json -Depth 10 | Set-Content -Path $combinedReport -Encoding utf8

    Write-Host "Rust report: $rustPath"
    Write-Host "Zig report:  $zigReport"
    Write-Host "Combined:    $combinedReport"
}
finally {
    Pop-Location
}
