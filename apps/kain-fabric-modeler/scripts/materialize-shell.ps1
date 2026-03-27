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
$Surfaces = (Get-Content (Join-Path $AppRoot "config/surfaces.json") -Raw | ConvertFrom-Json).surfaces
$Modes = (Get-Content (Join-Path $AppRoot "config/workspace_modes.json") -Raw | ConvertFrom-Json).modes
$Libraries = (Get-Content (Join-Path $AppRoot "config/library_catalog.json") -Raw | ConvertFrom-Json).libraries
$Tools = (Get-Content (Join-Path $AppRoot "config/tool_catalog.json") -Raw | ConvertFrom-Json).tools
$Pipeline = (Get-Content (Join-Path $AppRoot "config/fabric_pipeline.json") -Raw | ConvertFrom-Json).steps
$Commands = (Get-Content (Join-Path $AppRoot "config/command_registry.json") -Raw | ConvertFrom-Json).commands
$Intents = (Get-Content (Join-Path $AppRoot "config/fabric_intents.json") -Raw | ConvertFrom-Json).intents
$ResourceKinds = (Get-Content (Join-Path $AppRoot "config/resource_kinds.json") -Raw | ConvertFrom-Json).resource_kinds

$ModeLines = Render-TextLines $Modes { param($mode) "$($mode.label) | $($mode.summary)" }
$CommandLines = Render-TextLines $Commands { param($command) "$($command.label) | $($command.intent) | $($command.summary)" }
$LibraryLines = Render-TextLines ($Libraries | Select-Object -First 12) { param($library) "$($library.label) | $($library.category)" }
$SurfaceLines = Render-TextLines $Surfaces { param($surface) "$($surface.title) | $($surface.kind) | $($surface.summary)" }
$ToolLines = Render-TextLines $Tools { param($tool) "$($tool.label) | $($tool.summary)" }
$PipelineLines = Render-TextLines $Pipeline { param($step) "$($step.id) | $($step.runtime) | $($step.summary)" }
$IntentLines = Render-TextLines $Intents { param($intent) "$($intent.label) | $($intent.graph) | produces $($intent.produces.Count) targets" }
$ResourceLines = Render-TextLines $ResourceKinds { param($resource) "$($resource.resource_uri) | $($resource.kind) | $($resource.summary)" }
$CapabilityLines = Render-TextLines $Manifest.required_runtime_capabilities { param($capability) $capability }
$SessionTruthLines = Render-TextLines @(
    "Canonical session document lives in session/session_schema.kn",
    "Reducers mark dirty state before heavy work executes",
    "Intent planner maps commands into reusable Fabric graphs",
    "Resource and report stores project back into the native runtime snapshot"
) { param($line) $line }

$Shell = @"
component App():
    render <slot>
        <theme name="kain_fabric_modeler">
            <token name="theme.background.top" category="color" value="#120e0b" />
            <token name="theme.background.bottom" category="color" value="#1e1915" />
            <token name="theme.surface.default" category="color" value="#211b16" />
            <token name="theme.surface.alt" category="color" value="#2d241d" />
            <token name="theme.surface.raised" category="color" value="#3a2d23" />
            <token name="theme.outline.soft" category="color" value="#5e4a39" />
            <token name="theme.outline.bright" category="color" value="#f59e0b" />
            <token name="theme.accent.primary" category="color" value="#ea580c" />
            <token name="theme.accent.soft" category="color" value="#fdba74" />
            <token name="text.default" category="color" value="#f8eee5" />
            <token name="theme.typography.scale" category="type" value={1.08} />
            <token name="theme.spacing.scale" category="space" value={1.02} />
            <token name="theme.radius.scale" category="radius" value={1.08} />
        </theme>
        <panel title="$($Manifest.window_title)" layout="dock" persistent_layout_id="$($Manifest.layout_id)" gap={18} padding={18}>
            <panel title="Session Rail" dock="left" split_ratio={0.24} min_width={280} max_width={420} resizable={true} gap={12}>
                <inspector title="Workspace Modes">
$ModeLines
                </inspector>
                <inspector title="Command Surface">
$CommandLines
                </inspector>
                <inspector title="Imported Runtime Packs">
$LibraryLines
                </inspector>
            </panel>
            <panel title="Modeling Stage" dock="center" gap={14}>
                <viewport3d title="Viewport Stage" scene="modeler_scene" />
                <panel title="Realtime Deck" layout="row" gap={12}>
                    <graph title="Material Graph" />
                    <graph title="Procedural Stack" />
                    <timeline title="Fabric Timeline" />
                </panel>
                <panel title="State Projection" layout="row" gap={12}>
                    <graph title="Intent Queue" />
                    <graph title="Topology Console" />
                    <graph title="Publish Summary" />
                </panel>
            </panel>
            <panel title="Authoring Rails" dock="right" split_ratio={0.26} min_width={320} max_width={460} resizable={true} gap={12}>
                <inspector title="Surface Registry">
$SurfaceLines
                </inspector>
                <inspector title="Tool Rail">
$ToolLines
                </inspector>
                <inspector title="Intent Library">
$IntentLines
                </inspector>
                <inspector title="Resource Registry">
$ResourceLines
                </inspector>
            </panel>
            <panel title="Runtime Telemetry" dock="bottom" split_ratio={0.25} min_height={200} max_height={340} resizable={true} layout="row" gap={12}>
                <inspector title="Fabric Pipeline">
$PipelineLines
                </inspector>
                <inspector title="Session Truth">
$SessionTruthLines
                </inspector>
                <inspector title="Capabilities">
$CapabilityLines
                </inspector>
            </panel>
        </panel>
    </slot>
"@

$GeneratedRoot = Join-Path $AppRoot "generated"
New-Item -ItemType Directory -Path $GeneratedRoot -Force | Out-Null
$OutputPath = Join-Path $GeneratedRoot "main.generated.kn"
Set-Content -Path $OutputPath -Value $Shell -NoNewline
Write-Host "Materialized $OutputPath"
