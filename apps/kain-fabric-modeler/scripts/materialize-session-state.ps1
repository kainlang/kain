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

function Get-StepResult {
    param(
        $Report,
        [string]$StepId
    )

    if ($null -eq $Report -or $null -eq $Report.step_results) {
        return $null
    }

    return $Report.step_results | Where-Object { $_.id -eq $StepId } | Select-Object -First 1
}

function Get-StepOutput {
    param(
        $Step,
        [string]$OutputName
    )

    if ($null -eq $Step -or $null -eq $Step.outputs) {
        return $null
    }

    return $Step.outputs | Where-Object { $_.name -eq $OutputName } | Select-Object -First 1
}

function Get-StepDurationMs {
    param($Step)

    if ($null -eq $Step) {
        return $null
    }

    if ($null -eq $Step.started_unix_ms -or $null -eq $Step.finished_unix_ms) {
        return $null
    }

    return [int]($Step.finished_unix_ms - $Step.started_unix_ms)
}

function Get-OutputSummary {
    param($Output)

    if ($null -eq $Output) {
        return $null
    }

    if ($Output.payload.kind -eq "value") {
        return $Output.payload.value.summary
    }

    if ($Output.payload.kind -eq "shared_buffer") {
        $buffer = $Output.payload.buffer
        return "$($buffer.byte_length) bytes | $($buffer.format)"
    }

    if ($Output.payload.kind -eq "shared_image") {
        $image = $Output.payload.image
        return "$($image.width)x$($image.height) | $($image.pixel_format)"
    }

    return $Output.payload.kind
}

function Get-OutputVersion {
    param($Output)

    if ($null -eq $Output) {
        return 0
    }

    if ($Output.payload.kind -eq "value") {
        return 1
    }

    if ($Output.payload.kind -eq "shared_buffer" -and $null -ne $Output.payload.buffer.byte_length) {
        return [int]$Output.payload.buffer.byte_length
    }

    if ($Output.payload.kind -eq "shared_image" -and $null -ne $Output.payload.image.byte_length) {
        return [int]$Output.payload.image.byte_length
    }

    return 1
}

function Add-ResourceSnapshot {
    param(
        [System.Collections.ArrayList]$Resources,
        $Definition,
        $Output
    )

    $null = $Resources.Add([ordered]@{
        uri = $Definition.resource_uri
        id = $Definition.id
        kind = $Definition.kind
        producer_step = $Definition.producer_step
        storage = $Definition.storage
        status = if ($null -eq $Output) { "missing" } else { "ready" }
        version = Get-OutputVersion $Output
        summary = if ($null -eq $Output) { $Definition.summary } else { Get-OutputSummary $Output }
    })
}

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path
$RepoRoot = (Resolve-Path (Join-Path $AppRoot "..\..")).Path

$Manifest = Get-Content (Join-Path $AppRoot "config/app_manifest.json") -Raw | ConvertFrom-Json
$CommandsManifest = Get-Content (Join-Path $AppRoot "config/command_registry.json") -Raw | ConvertFrom-Json
$IntentManifest = Get-Content (Join-Path $AppRoot "config/fabric_intents.json") -Raw | ConvertFrom-Json
$ResourceKindsManifest = Get-Content (Join-Path $AppRoot "config/resource_kinds.json") -Raw | ConvertFrom-Json
$Modes = (Get-Content (Join-Path $AppRoot "config/workspace_modes.json") -Raw | ConvertFrom-Json).modes
$Tools = (Get-Content (Join-Path $AppRoot "config/tool_catalog.json") -Raw | ConvertFrom-Json).tools
$Pipeline = (Get-Content (Join-Path $AppRoot "config/fabric_pipeline.json") -Raw | ConvertFrom-Json).steps

$LatestReport = Get-LatestFabricReport -ReportRoot (Join-Path $AppRoot ".kain/fabric/reports")
$PythonStep = Get-StepResult -Report $LatestReport -StepId "python_project_seed"
$ModelSeedStep = Get-StepResult -Report $LatestReport -StepId "model_seed"
$NativeBrushStep = Get-StepResult -Report $LatestReport -StepId "native_brush"
$TopologyStep = Get-StepResult -Report $LatestReport -StepId "topology_analyzer"
$GpuPreviewStep = Get-StepResult -Report $LatestReport -StepId "gpu_preview"
$NodePublisherStep = Get-StepResult -Report $LatestReport -StepId "node_publisher"

$ProjectSettingsOutput = Get-StepOutput -Step $PythonStep -OutputName "project_settings"
$ProjectSettings = $null
if ($null -ne $ProjectSettingsOutput -and $null -ne $ProjectSettingsOutput.payload.value.json) {
    $ProjectSettings = $ProjectSettingsOutput.payload.value.json
}

$WorkspaceMode = if ($null -ne $ProjectSettings -and $null -ne $ProjectSettings.workspace_mode) { $ProjectSettings.workspace_mode } else { $Modes[0].id }
$ViewportWidth = if ($null -ne $ProjectSettings -and $null -ne $ProjectSettings.viewport_width) { [int]$ProjectSettings.viewport_width } else { 1440 }
$ViewportHeight = if ($null -ne $ProjectSettings -and $null -ne $ProjectSettings.viewport_height) { [int]$ProjectSettings.viewport_height } else { 900 }
$ProjectId = if ($null -ne $ProjectSettings -and $null -ne $ProjectSettings.project_name) { $ProjectSettings.project_name } else { "fabric-modeler" }
$LatestFabricStatus = if ($null -ne $LatestReport) { $LatestReport.status } else { "idle" }

$PreviewDirty = $false
$TopologyDirty = $false
$PublishDirty = $false
if ($LatestFabricStatus -ne "succeeded") {
    $PreviewDirty = $true
    $TopologyDirty = $true
    $PublishDirty = $true
}

$CommandSnapshots = New-Object System.Collections.ArrayList
$null = $CommandSnapshots.Add([ordered]@{
    id = "runtime.reload"
    label = "Reload Runtime"
    surface = "titlebar"
    intent = "runtime.reload"
})
foreach ($command in $CommandsManifest.commands) {
    $null = $CommandSnapshots.Add([ordered]@{
        id = $command.id
        label = $command.label
        surface = $command.surface
        intent = $command.intent
    })
}

$Resources = New-Object System.Collections.ArrayList
foreach ($definition in $ResourceKindsManifest.resource_kinds) {
    $step = Get-StepResult -Report $LatestReport -StepId $definition.producer_step
    $output = Get-StepOutput -Step $step -OutputName $definition.output_name
    Add-ResourceSnapshot -Resources $Resources -Definition $definition -Output $output
}

$Reports = New-Object System.Collections.ArrayList
$SceneReport = Get-StepOutput -Step $ModelSeedStep -OutputName "scene_report"
$TopologyReport = Get-StepOutput -Step $TopologyStep -OutputName "topology_report"
$PublishReport = Get-StepOutput -Step $NodePublisherStep -OutputName "studio_summary"

$null = $Reports.Add([ordered]@{
    uri = "report://scene/current"
    producer_step = "model_seed"
    status = if ($null -eq $SceneReport) { "missing" } else { "ready" }
    summary = if ($null -eq $SceneReport) { "Scene report unavailable." } else { Get-OutputSummary $SceneReport }
})
$null = $Reports.Add([ordered]@{
    uri = "report://topology/current"
    producer_step = "topology_analyzer"
    status = if ($null -eq $TopologyReport) { "missing" } else { "ready" }
    summary = if ($null -eq $TopologyReport) { "Topology report unavailable." } else { Get-OutputSummary $TopologyReport }
})
$null = $Reports.Add([ordered]@{
    uri = "artifact://publish/summary"
    producer_step = "node_publisher"
    status = if ($null -eq $PublishReport) { "missing" } else { "ready" }
    summary = if ($null -eq $PublishReport) { "Publish summary unavailable." } else { Get-OutputSummary $PublishReport }
})

$IntentQueue = New-Object System.Collections.ArrayList
if ($null -eq $LatestReport) {
    $intent = $IntentManifest.intents | Where-Object { $_.id -eq "project.bootstrap" } | Select-Object -First 1
    $null = $IntentQueue.Add([ordered]@{
        id = $intent.id
        label = $intent.label
        reason = "No Fabric report exists yet."
        graph = $intent.graph
        debounce_ms = $intent.debounce_ms
        status = "recommended"
    })
}

$RuntimeTools = New-Object System.Collections.ArrayList
foreach ($capability in $Manifest.required_runtime_capabilities) {
    $toolId = ($capability -replace "[^A-Za-z0-9]", "_").ToLowerInvariant()
    $null = $RuntimeTools.Add([ordered]@{
        id = $toolId
        label = $capability
        capability = $capability
        approval = "workspace"
        decision = $null
        scope_decisions = @()
    })
}

$NowIso = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$RecentSessionId = if ($null -ne $LatestReport) { $LatestReport.session_id } else { "modeler-session-bootstrap" }
$StepStatus = New-Object System.Collections.ArrayList
foreach ($step in $Pipeline) {
    $reportStep = Get-StepResult -Report $LatestReport -StepId $step.id
    $null = $StepStatus.Add([ordered]@{
        id = $step.id
        runtime = $step.runtime
        status = if ($null -eq $reportStep) { "pending" } else { $reportStep.status }
        duration_ms = Get-StepDurationMs $reportStep
        summary = $step.summary
    })
}

$RuntimeSnapshot = [ordered]@{
    app_id = $Manifest.app_id
    name = $Manifest.name
    version = $Manifest.version
    window_title = $Manifest.window_title
    root_component = $Manifest.root_component
    layout_id = $Manifest.layout_id
    required_runtime_capabilities = $Manifest.required_runtime_capabilities
    panels = @(
        [ordered]@{
            id = "runtime_surface"
            title = $Manifest.window_title
            dock = "center"
            kind = "native-ui"
        }
    )
    commands = $CommandSnapshots
    providers = @(
        [ordered]@{
            id = "native_runtime"
            label = "Native Runtime"
            transport = "in-process"
            profile_kind = "native-ui"
            supports_tools = $true
            supports_streaming = $false
            active = $true
            profile_configured = $true
            profile_keys = @()
        }
    )
    tools = $RuntimeTools
    sessions = [ordered]@{
        total_sessions = 1
        active_provider = "native_runtime"
        recent_session_id = $RecentSessionId
        recent_session_title = $Manifest.name
    }
    recent_sessions = @(
        [ordered]@{
            id = $RecentSessionId
            title = $Manifest.name
            provider_id = "native_runtime"
            status = if ($null -eq $LatestReport) { "idle" } else { $LatestFabricStatus }
            workspace_root = $RepoRoot
            updated_at = $NowIso
            message_count = 1
            last_message_role = "system"
            last_message_preview = "Modeler session projection materialized from config and latest Fabric report."
        }
    )
    workspaces = @(
        [ordered]@{
            root = $RepoRoot
            session_count = 1
            recent_session_title = $Manifest.name
        }
    )
    modeler_state = [ordered]@{
        schema_version = 1
        manifest_registry = [ordered]@{
            app_manifest = "config/app_manifest.json"
            command_registry = "config/command_registry.json"
            fabric_intents = "config/fabric_intents.json"
            resource_kinds = "config/resource_kinds.json"
            session_schema = "session/session_schema.kn"
            session_reducers = "session/reducers.kn"
            session_derived_state = "session/derived_state.kn"
            session_intent_planner = "session/intent_planner.kn"
            session_resource_registry = "session/resource_registry.kn"
            session_report_registry = "session/report_registry.kn"
        }
        session = [ordered]@{
            project = [ordered]@{
                id = $ProjectId
                name = "Kain Fabric Modeler"
                schema_version = 1
            }
            documents = [ordered]@{
                "scene/main" = [ordered]@{
                    kind = "scene"
                    root_entities = @("entity/cube_01")
                }
            }
            workspace = [ordered]@{
                active_mode = $WorkspaceMode
                layout_id = $Manifest.layout_id
                available_modes = $Modes | ForEach-Object { $_.id }
            }
            tooling = [ordered]@{
                active_tool = "select"
                tool_presets = [ordered]@{
                    clay_sculpt = [ordered]@{
                        radius = 32
                        strength = 0.65
                    }
                }
            }
            selection = [ordered]@{
                entity_ids = @("entity/cube_01")
                subobject_ids = @()
            }
            viewport = [ordered]@{
                camera_id = "camera/main"
                gizmo_mode = "translate"
                shading_mode = "lit"
                width = $ViewportWidth
                height = $ViewportHeight
            }
            ui = [ordered]@{
                active_panel = "selection_inspector"
                expanded_groups = @("tool_shelf", "topology_console", "publish_summary")
            }
            dirty = [ordered]@{
                preview = $PreviewDirty
                topology = $TopologyDirty
                publish_summary = $PublishDirty
            }
            jobs = [ordered]@{
                latest_fabric_session_id = $RecentSessionId
                latest_fabric_status = $LatestFabricStatus
                active_intents = @($IntentQueue | ForEach-Object { $_.id })
            }
            history = [ordered]@{
                undo_depth = 0
                redo_depth = 0
                last_command_id = if ($null -eq $LatestReport) { $null } else { "project.bootstrap" }
            }
        }
        derived = [ordered]@{
            selection_summary = "1 mesh selected"
            preview_state = if ($PreviewDirty) { "stale" } else { "ready" }
            topology_state = if ($TopologyDirty) { "warning" } else { "ready" }
            publish_state = if ($PublishDirty) { "stale" } else { "ready" }
            publish_ready = (-not $PreviewDirty -and -not $TopologyDirty -and -not $PublishDirty -and $LatestFabricStatus -eq "succeeded")
            requires_preview_rebake = $PreviewDirty
        }
        command_registry = $CommandsManifest.commands
        available_tools = $Tools
        resource_store = $Resources
        report_store = $Reports
        intent_queue = $IntentQueue
        latest_fabric_run = [ordered]@{
            session_id = if ($null -eq $LatestReport) { $null } else { $LatestReport.session_id }
            status = $LatestFabricStatus
            manifest_path = if ($null -eq $LatestReport) { "apps/kain-fabric-modeler/KAIN.fabric.toml" } else { $LatestReport.manifest_path }
            started_unix_ms = if ($null -eq $LatestReport) { $null } else { $LatestReport.started_unix_ms }
            finished_unix_ms = if ($null -eq $LatestReport) { $null } else { $LatestReport.finished_unix_ms }
            steps = $StepStatus
        }
    }
    updated_at = $NowIso
}

$StateRoot = Join-Path $AppRoot "state"
$NativeAppStateRoot = Join-Path $AppRoot "native-app/state"
New-Item -ItemType Directory -Path $StateRoot -Force | Out-Null
New-Item -ItemType Directory -Path $NativeAppStateRoot -Force | Out-Null

$OutputJson = $RuntimeSnapshot | ConvertTo-Json -Depth 20
Set-Content -Path (Join-Path $StateRoot "runtime_snapshot.json") -Value $OutputJson
Set-Content -Path (Join-Path $NativeAppStateRoot "runtime_snapshot.json") -Value $OutputJson

Write-Host "Materialized $(Join-Path $StateRoot 'runtime_snapshot.json')"
