$ErrorActionPreference = "Stop"

function ConvertTo-KnLiteral {
    param([string]$Value)

    if ($null -eq $Value) {
        return ""
    }

    return ($Value -replace "\\", "\\\\") -replace '"', '\"'
}

function Render-TextLines {
    param(
        [System.Collections.IEnumerable]$Items,
        [scriptblock]$Formatter,
        [string]$Indent = "                    "
    )

    $lines = @()
    foreach ($item in $Items) {
        $text = ConvertTo-KnLiteral (& $Formatter $item)
        $lines += "$Indent<text role=`"body`">{`"$text`"}</text>"
    }
    return ($lines -join "`n")
}

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path

$Manifest = Get-Content (Join-Path $AppRoot "config/app_manifest.json") -Raw | ConvertFrom-Json
$Modes = (Get-Content (Join-Path $AppRoot "config/workspace_modes.json") -Raw | ConvertFrom-Json).modes
$Surfaces = (Get-Content (Join-Path $AppRoot "config/surfaces.json") -Raw | ConvertFrom-Json).surfaces
$Tools = (Get-Content (Join-Path $AppRoot "config/tool_catalog.json") -Raw | ConvertFrom-Json).tools
$Commands = (Get-Content (Join-Path $AppRoot "config/command_registry.json") -Raw | ConvertFrom-Json).commands
$Pipeline = (Get-Content (Join-Path $AppRoot "config/fabric_pipeline.json") -Raw | ConvertFrom-Json).steps
$Intents = (Get-Content (Join-Path $AppRoot "config/fabric_intents.json") -Raw | ConvertFrom-Json).intents
$Resources = (Get-Content (Join-Path $AppRoot "config/resource_kinds.json") -Raw | ConvertFrom-Json).resource_kinds
$Reports = (Get-Content (Join-Path $AppRoot "config/report_kinds.json") -Raw | ConvertFrom-Json).report_kinds
$RuntimePacks = (Get-Content (Join-Path $AppRoot "config/runtime_packs.json") -Raw | ConvertFrom-Json).runtime_packs
$Jobs = (Get-Content (Join-Path $AppRoot "config/automation_jobs.json") -Raw | ConvertFrom-Json).jobs

$ModeLines = Render-TextLines $Modes { param($mode) "$($mode.label) | $($mode.summary)" }
$CommandLines = Render-TextLines $Commands { param($command) "$($command.label) | $($command.intent) | $($command.summary)" }
$PackLines = Render-TextLines ($RuntimePacks | Select-Object -First 8) { param($pack) "$($pack.label) | $($pack.summary)" }
$SurfaceLines = Render-TextLines $Surfaces { param($surface) "$($surface.title) | $($surface.kind) | $($surface.summary)" }
$ToolLines = Render-TextLines $Tools { param($tool) "$($tool.label) | $($tool.summary)" }
$IntentLines = Render-TextLines $Intents { param($intent) "$($intent.label) | $($intent.graph) | produces $($intent.produces.Count) targets" }
$PipelineLines = Render-TextLines $Pipeline { param($step) "$($step.id) | $($step.runtime) | $($step.summary)" }
$ResourceLines = Render-TextLines $Resources { param($resource) "$($resource.resource_uri) | $($resource.kind) | $($resource.summary)" }
$ReportLines = Render-TextLines $Reports { param($report) "$($report.report_uri) | $($report.summary)" }
$JobLines = Render-TextLines $Jobs { param($job) "$($job.label) | $($job.schedule) | $($job.summary)" }
$CapabilityLines = Render-TextLines $Manifest.required_runtime_capabilities { param($capability) $capability }

$Shell = @"
component App():
    render <slot>
        <theme name="kain_fabric_dcc_suite">
            <token name="theme.background.top" category="color" value="#0f1d23" />
            <token name="theme.background.bottom" category="color" value="#162f35" />
            <token name="theme.surface.default" category="color" value="#1e3e44" />
            <token name="theme.surface.alt" category="color" value="#28525a" />
            <token name="theme.surface.raised" category="color" value="#356972" />
            <token name="theme.outline.soft" category="color" value="#63a3a6" />
            <token name="theme.outline.bright" category="color" value="#f59e0b" />
            <token name="theme.accent.primary" category="color" value="#f97316" />
            <token name="text.default" category="color" value="#eefaf8" />
        </theme>
        <panel title="$($Manifest.window_title)" layout="dock" persistent_layout_id="$($Manifest.layout_id)" gap={18} padding={18}>
            <panel title="Suite Rail" dock="left" split_ratio={0.23} min_width={300} max_width={430} resizable={true} gap={12}>
                <inspector title="Workspace Modes">
$ModeLines
                </inspector>
                <inspector title="Runtime Packs">
$PackLines
                </inspector>
                <inspector title="Command Surface">
$CommandLines
                </inspector>
            </panel>
            <panel title="DCC Stage" dock="center" gap={14}>
                <viewport3d title="Viewport Stage" scene="dcc_suite_scene" />
                <panel title="Lane Deck" layout="row" gap={12}>
                    <graph title="Material Graph" />
                    <graph title="Rig Graph" />
                    <timeline title="Animation Timeline" />
                </panel>
                <panel title="Production Deck" layout="row" gap={12}>
                    <graph title="Simulation Board" />
                    <graph title="Render Lounge" />
                    <graph title="Compositor Stack" />
                </panel>
            </panel>
            <panel title="Operator Rails" dock="right" split_ratio={0.27} min_width={320} max_width={470} resizable={true} gap={12}>
                <inspector title="Surfaces">
$SurfaceLines
                </inspector>
                <inspector title="Tool Rail">
$ToolLines
                </inspector>
                <inspector title="Intent Library">
$IntentLines
                </inspector>
                <inspector title="Resources">
$ResourceLines
                </inspector>
                <inspector title="Reports">
$ReportLines
                </inspector>
            </panel>
            <panel title="Runtime Telemetry" dock="bottom" split_ratio={0.27} min_height={220} max_height={360} resizable={true} layout="row" gap={12}>
                <inspector title="Fabric Pipeline">
$PipelineLines
                </inspector>
                <inspector title="Automation Jobs">
$JobLines
                </inspector>
                <inspector title="Capabilities">
$CapabilityLines
                </inspector>
            </panel>
        </panel>
    </slot>
"@

$OutputPath = Join-Path $AppRoot "generated/main.generated.kn"
Set-Content -Path $OutputPath -Value $Shell -NoNewline
Write-Host "Materialized $OutputPath"
