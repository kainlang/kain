$ErrorActionPreference = "Stop"

function Get-LatestFabricReport {
    param([string]$ReportRoot)

    if (-not (Test-Path $ReportRoot)) {
        return $null
    }

    $latest = Get-ChildItem -Path $ReportRoot -Recurse -Filter "report.json" |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1

    if ($null -eq $latest) {
        return $null
    }

    return Get-Content $latest.FullName -Raw | ConvertFrom-Json
}

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path
$RepoRoot = (Resolve-Path (Join-Path $AppRoot "..\..")).Path

$Manifest = Get-Content (Join-Path $AppRoot "config/app_manifest.json") -Raw | ConvertFrom-Json
$Commands = (Get-Content (Join-Path $AppRoot "config/command_registry.json") -Raw | ConvertFrom-Json).commands
$Resources = (Get-Content (Join-Path $AppRoot "config/resource_kinds.json") -Raw | ConvertFrom-Json).resource_kinds
$Reports = (Get-Content (Join-Path $AppRoot "config/report_kinds.json") -Raw | ConvertFrom-Json).report_kinds
$Jobs = (Get-Content (Join-Path $AppRoot "config/automation_jobs.json") -Raw | ConvertFrom-Json).jobs
$Modes = (Get-Content (Join-Path $AppRoot "config/workspace_modes.json") -Raw | ConvertFrom-Json).modes
$Tools = (Get-Content (Join-Path $AppRoot "config/tool_catalog.json") -Raw | ConvertFrom-Json).tools
$LatestReport = Get-LatestFabricReport -ReportRoot (Join-Path $AppRoot ".kain/fabric/reports")

$Snapshot = [ordered]@{
    app_id = $Manifest.app_id
    name = $Manifest.name
    version = $Manifest.version
    window_title = $Manifest.window_title
    layout_id = $Manifest.layout_id
    workspace_root = $RepoRoot
    latest_fabric_status = if ($null -eq $LatestReport) { "idle" } else { $LatestReport.status }
    workspace_modes = $Modes
    tools = $Tools
    commands = $Commands
    resources = $Resources
    reports = $Reports
    automation_jobs = $Jobs
    extension_seams = @(
        "tensor lane currently reports readiness and plan state rather than executing a full typed tensor artifact contract",
        "simulation lane currently materializes plan-oriented reports rather than a true solver runtime",
        "compositor lane currently materializes rebuild plans rather than executing a first-class compositor graph runtime"
    )
}

$OutputPath = Join-Path $AppRoot "state/runtime_snapshot.json"
$Snapshot | ConvertTo-Json -Depth 10 | Set-Content -Path $OutputPath
Write-Host "Materialized $OutputPath"
