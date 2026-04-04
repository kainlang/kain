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

function New-RequiredCapabilityTools {
    param([string[]]$Capabilities)

    $tools = New-Object System.Collections.ArrayList
    foreach ($capability in $Capabilities) {
        $toolId = ($capability -replace "[^A-Za-z0-9]", "_").ToLowerInvariant()
        $null = $tools.Add([ordered]@{
            id = $toolId
            label = $capability
            capability = $capability
            approval = "workspace"
            decision = $null
            scope_decisions = @()
        })
    }
    return $tools
}

function New-CommandSnapshots {
    param($Commands)

    $snapshots = New-Object System.Collections.ArrayList
    $null = $snapshots.Add([ordered]@{
        id = "runtime.reload"
        label = "Reload Runtime"
        surface = "titlebar"
        intent = "runtime.reload"
    })

    foreach ($command in $Commands) {
        $null = $snapshots.Add([ordered]@{
            id = $command.id
            label = $command.label
            surface = $command.surface
            intent = $command.intent
        })
    }

    return $snapshots
}

function Find-ModeLabel {
    param(
        $Modes,
        [string]$ModeId
    )

    $mode = $Modes | Where-Object { $_.id -eq $ModeId } | Select-Object -First 1
    if ($null -eq $mode) {
        return $ModeId
    }
    return $mode.label
}

function New-RecentSessionPreview {
    param(
        [string]$FabricStatus,
        [string]$ModeLabel
    )

    return "DCC suite bridge ready | fabric=$FabricStatus | mode=$ModeLabel"
}

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path
$RepoRoot = (Resolve-Path (Join-Path $AppRoot "..\..")).Path
$StateRoot = Join-Path $AppRoot "state"
$NativeAppStateRoot = Join-Path $AppRoot "native-app/state"

New-Item -ItemType Directory -Path $StateRoot -Force | Out-Null
New-Item -ItemType Directory -Path $NativeAppStateRoot -Force | Out-Null

$Manifest = Get-Content (Join-Path $AppRoot "config/app_manifest.json") -Raw | ConvertFrom-Json
$Modes = (Get-Content (Join-Path $AppRoot "config/workspace_modes.json") -Raw | ConvertFrom-Json).modes
$Surfaces = (Get-Content (Join-Path $AppRoot "config/surfaces.json") -Raw | ConvertFrom-Json).surfaces
$Tools = (Get-Content (Join-Path $AppRoot "config/tool_catalog.json") -Raw | ConvertFrom-Json).tools
$Commands = (Get-Content (Join-Path $AppRoot "config/command_registry.json") -Raw | ConvertFrom-Json).commands
$CommandSummary = if ($null -ne $Commands) {
    ($Commands | ForEach-Object { "$($_.label) [$($_.id)]" }) -join " | "
} else {
    "n/a"
}
$Intents = (Get-Content (Join-Path $AppRoot "config/fabric_intents.json") -Raw | ConvertFrom-Json).intents
$IntentSummary = if ($null -ne $Intents) {
    ($Intents | ForEach-Object { "$($_.label) [$($_.id)]" }) -join " | "
} else {
    "n/a"
}
$Pipeline = (Get-Content (Join-Path $AppRoot "config/fabric_pipeline.json") -Raw | ConvertFrom-Json).steps
$RuntimePacks = (Get-Content (Join-Path $AppRoot "config/runtime_packs.json") -Raw | ConvertFrom-Json).runtime_packs
$RuntimePackSummary = if ($null -ne $RuntimePacks) {
    ($RuntimePacks | ForEach-Object { "$($_.label) [$($_.id)]" }) -join " | "
} else {
    "n/a"
}
$RuntimeLanes = (Get-Content (Join-Path $AppRoot "config/runtime_lanes.json") -Raw | ConvertFrom-Json).runtime_lanes
$ViewportModes = (Get-Content (Join-Path $AppRoot "config/viewport_modes.json") -Raw | ConvertFrom-Json).modes
$ViewportModeRegistryEntries = if ($null -ne $ViewportModes) {
    $ViewportModes | ForEach-Object { "$($_.label) [$($_.id)] => $($_.overlay_policy_id) / $($_.tool_policy_id) / $($_.view_profile_id)" }
} else {
    @("Layout [layout] => overlay_policy/layout_clear / tool_policy/layout_nav_first / view_profile/layout_blocking", "Model [model] => overlay_policy/model_topology / tool_policy/model_edit_first / view_profile/model_topology", "Sculpt [sculpt] => overlay_policy/sculpt_brush / tool_policy/sculpt_brush_first / view_profile/sculpt_surface", "Paint [paint] => overlay_policy/paint_layers / tool_policy/paint_layer_first / view_profile/paint_authoring", "Lookdev [lookdev] => overlay_policy/lookdev_balanced / tool_policy/lookdev_eval / view_profile/lookdev_balanced", "Render [render] => overlay_policy/render_review / tool_policy/render_review / view_profile/render_room")
}
$AssetPipeline = Get-Content (Join-Path $AppRoot "config/asset_pipeline_manifest.json") -Raw | ConvertFrom-Json
$AssetPipelineRecord = $AssetPipeline.asset_pipeline
$AssetPipelineSummary = [string]$AssetPipelineRecord.summary
$RuntimeLaneSummary = if ($null -ne $RuntimeLanes) {
    ($RuntimeLanes | ForEach-Object { $_.runtime }) -join " | "
} else {
    "n/a"
}
$RuntimeLaneRegistrySummary = if ($null -ne $RuntimeLanes) {
    ($RuntimeLanes | ForEach-Object { "$($_.label) [$($_.runtime)]" }) -join " | "
} else {
    "Kain Semantics Lane [kain] | Fabric Orchestration Lane [fabric] | Python Bootstrap Lane [python] | GPU Compute Lane [gpu_compute] | Native C ABI Lane [c_abi] | Rust Analysis Lane [rust_crate] | Node Bridge Lane [node_bridge]"
}
$PowerLaneRegistryEntries = if ($null -ne $RuntimeLanes) { @($RuntimeLanes) } else { @() }
$PowerLaneCount = @($PowerLaneRegistryEntries).Count
$PowerLaneSummary = if ($PowerLaneCount -gt 0) {
    ($PowerLaneRegistryEntries | ForEach-Object { "$($_.label) [$($_.runtime)]" }) -join " | "
} else {
    $RuntimeLaneRegistrySummary
}
$Resources = (Get-Content (Join-Path $AppRoot "config/resource_kinds.json") -Raw | ConvertFrom-Json).resource_kinds
$MeshContract = Get-Content (Join-Path $AppRoot "config/mesh_resource_contract.json") -Raw | ConvertFrom-Json
$Reports = (Get-Content (Join-Path $AppRoot "config/report_kinds.json") -Raw | ConvertFrom-Json).report_kinds
$Jobs = (Get-Content (Join-Path $AppRoot "config/automation_jobs.json") -Raw | ConvertFrom-Json).jobs
$GizmoRegistry = Get-Content (Join-Path $AppRoot "config/gizmo_registry.json") -Raw | ConvertFrom-Json
$UiShell = Get-Content (Join-Path $AppRoot "config/ui_shell.json") -Raw | ConvertFrom-Json

$LatestReport = Get-LatestFabricReport -ReportRoot (Join-Path $AppRoot ".kain/fabric/reports")
$LatestFabricStatus = if ($null -eq $LatestReport) { "idle" } else { $LatestReport.status }
$NowIso = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$SessionId = if ($null -eq $LatestReport) { "dcc-suite-session-bootstrap" } else { $LatestReport.session_id }

function Get-ViewportModeForWorkspaceMode {
    param(
        [string]$WorkspaceMode,
        [System.Collections.IEnumerable]$ViewportModeItems
    )

    $modeById = @{}
    foreach ($mode in $ViewportModeItems) {
        $modeById[$mode.id] = $mode
    }

    switch ($WorkspaceMode) {
        "sculpt_model" { return $modeById["model"] }
        "material_lookdev" { return $modeById["lookdev"] }
        "render_comp" { return $modeById["render"] }
        default { return $modeById["layout"] }
    }
}

$DefaultModeId = "scene_assembly"
$ActiveModeId = $DefaultModeId
$ActiveModeLabel = Find-ModeLabel -Modes $Modes -ModeId $ActiveModeId
$RecentSessionTitle = "$($Manifest.name) | $ActiveModeLabel"
$ActiveViewportMode = Get-ViewportModeForWorkspaceMode -WorkspaceMode $ActiveModeId -ViewportModeItems $ViewportModes

$CommandSnapshots = New-CommandSnapshots -Commands $Commands
$RuntimeTools = New-RequiredCapabilityTools -Capabilities $Manifest.required_runtime_capabilities

$InitialIntentQueue = @()
if ($LatestFabricStatus -eq "idle") {
    $InitialIntentQueue = @(
        [ordered]@{
            id = "project.bootstrap"
            label = "Bootstrap Suite"
            reason = "No Fabric report exists yet."
            graph = "fabric/intents/bootstrap.fabric.toml"
            debounce_ms = 0
            status = "recommended"
        }
    )
}

$SessionDocument = [ordered]@{
    project = [ordered]@{
        project_id = "fabric-dcc-suite"
        project_name = "Kain Fabric DCC Suite"
        schema_version = 1
    }
    workspace = [ordered]@{
        active_mode = $ActiveModeId
        layout_id = $Manifest.layout_id
        available_modes = @($Modes | ForEach-Object { $_.id })
    }
    runtime_lane_registry_entries = $RuntimeLanes
    power_lane_registry_entries = $PowerLaneRegistryEntries
    runtime_lane_count = @($RuntimeLanes).Count
    power_lane_count = $PowerLaneCount
    runtime_lane_summary = $RuntimeLaneSummary
    power_lane_summary = $PowerLaneSummary
    runtime_lane_registry_summary = $RuntimeLaneRegistrySummary
    power_lane_registry_summary = $PowerLaneSummary
    runtime_pack_registry_entries = $RuntimePacks
    runtime_pack_count = @($RuntimePacks).Count
    runtime_pack_summary = $RuntimePackSummary
    fabric_intent_registry_entries = $Intents
    fabric_intent_count = @($Intents).Count
    fabric_intent_summary = $IntentSummary
    command_registry = $Commands
    command_registry_entries = $Commands
    command_count = @($Commands).Count
    command_summary = $CommandSummary
    viewport = [ordered]@{
        active_mode = $ActiveViewportMode.id
        overlay_policy_id = $ActiveViewportMode.overlay_policy_id
        tool_policy_id = $ActiveViewportMode.tool_policy_id
        view_profile_id = $ActiveViewportMode.view_profile_id
        hud_density = $ActiveViewportMode.hud_density
        mode_count = @($ViewportModes).Count
        mode_summary = ($ViewportModes | ForEach-Object { $_.id }) -join " | "
        mode_registry_summary = ($ViewportModes | ForEach-Object { "$($_.id) => $($_.overlay_policy_id)" }) -join " | "
        mode_registry_entries = @($ViewportModeRegistryEntries)
    }
    workbench = [ordered]@{
        active_workbench_id = $ActiveModeId
        active_tab_group_id = $UiShell.page_tab_group_id
        active_dock_id = "dcc_workbench_pages"
        active_pane_id = "pane/viewport_stage"
        last_materialized_shell_path = "generated/main.generated.kn"
        last_runtime_snapshot_path = "state/runtime_snapshot.json"
        summary = "$ActiveModeId:tabs=$($UiShell.page_tab_group_id):dock=dcc_workbench_pages:pane=pane/viewport_stage"
    }
    context = [ordered]@{
        active_workspace_id = "workspace/$ActiveModeId"
        active_pane_id = "pane/viewport_stage"
        active_tool_id = "select"
        active_object_id = "entity/blender_startup_cube"
        active_edit_target_id = "entity/blender_startup_cube"
        active_material_id = "material/hero_surface"
        active_texture_set_id = "textureset/hero_body_udim1001"
        active_graph_node_id = "graph/lookdev_primary"
        active_frame = 96
        active_viewport_mode = $ActiveViewportMode.id
    }
    tooling = [ordered]@{
        active_tool = "select"
        brush_radius = 42
        brush_strength_percent = 68
    }
    gizmo = [ordered]@{
        active_profile_id = "dcc_transform_universal"
        mode = "translate"
        space = "world"
        snap_enabled = $false
        visible = $true
        drag_trigger = "ctrl_primary_drag"
    }
    selection = [ordered]@{
        entity_ids = @("entity/blender_startup_cube")
        subobject_ids = @()
    }
    scene = [ordered]@{
        active_document_id = "scene/dcc_suite_startup"
        active_collection_id = "collection/startup_stage"
        active_variant = "lookdev"
    }
    mesh = [ordered]@{
        active_document_id = "mesh/dcc_suite_startup_cube"
        active_edit_target_id = "entity/blender_startup_cube"
        mesh_authoring_policy_id = "mesh_authoring_policy/startup_hybrid"
        active_primitive_template_id = "primitive/cube"
        topology_edit_mode = "object"
    }
    ingest = [ordered]@{
        last_package_uri = "asset://starter/kitbash_hangar"
        last_package_kind = "gltf"
        staged_package_count = 1
    }
    asset_pipeline = [ordered]@{
        source_id = $AssetPipelineRecord.source_id
        session_route_scope = $AssetPipelineRecord.session_route_scope
        source_priority = $AssetPipelineRecord.source_priority
        transcode_profiles = @($AssetPipelineRecord.supported_transcode_profiles)
        routed_runtime_ids = @($AssetPipelineRecord.routed_runtime_ids)
        lineage_receipts = @($AssetPipelineRecord.lineage_receipts)
        registry_entries = @($AssetPipeline.asset_pipeline.source_id, $AssetPipeline.asset_pipeline.lane, $AssetPipeline.asset_pipeline.session_route_scope)
        summary = $AssetPipelineSummary
    }
    asset_ingest_summary = "gltf intake ready / 1 staged package"
    asset_ingest_status = "intake ready"
    asset_ingest_count = 1
    materials = [ordered]@{
        active_material_id = "material/hero_surface"
        active_graph_id = "graph/lookdev_primary"
        active_texture_set_id = "textureset/hero_body_udim1001"
        active_layer_stack_id = "layerstack/hero_surface_primary"
        active_svg_document_id = "svg/masks/hero_surface_primary"
        active_bake_preset_id = "bake/high_precision_curvature"
        active_export_preset_id = "export/metalrough_orm_painter"
        preview_profile = "material_preview_balanced"
        channel_profile = "basecolor_normal_roughness_metallic_ao_height_emissive"
        paint_resolution = 4096
    }
    rig = [ordered]@{
        active_rig_id = "rig/hero_body"
        active_control_set = "controls/anim_main"
        deformation_profile = "hero_deformation_preview"
    }
    animation = [ordered]@{
        active_clip_id = "clip/blocking_pass_a"
        frame = 96
        playback_mode = "paused"
    }
    simulation = [ordered]@{
        cache_profile = "sim_preview_cache"
        last_tick_frame = 96
        solver_profile = "cloth_preview"
    }
    render = [ordered]@{
        camera_id = "camera/startup_authoring"
        view_transform = "acescg"
        render_profile = "viewport_quality"
        aov_set = "beauty_plus_utility"
        accumulation_profile = "progressive_preview"
        denoise_profile = "viewport_temporal_denoise"
        review_capture_profile = "frame_review_pack"
    }
    viewport_frame_feedback = if ($LatestFabricStatus -eq "succeeded") { "frame steady / preview responsive" } else { "frame warming / preview stabilizing" }
    compositor = [ordered]@{
        active_stack_id = "comp/final_review"
        last_rebuild_reason = "bootstrap"
    }
    publish = [ordered]@{
        profile_id = "publish/review_daily"
        target_bundle = "bundle/hero_scene_daily"
        delivery_channel = "studio_review"
    }
    automation = [ordered]@{
        enabled = $true
        active_job_ids = @("thumbnail_refresh", "nightly_material_rebake", "svg_mask_cache_rebuild")
        last_audit_status = "pending"
    }
    reports = [ordered]@{
        mesh_contract_report_id = "mesh_contract_report"
        mesh_contract_report_uri = "report://mesh/contract"
        mesh_contract_report_path = "state/mesh_contract_report.json"
        topology_history_report_id = "topology_history_report"
        topology_history_report_uri = "report://topology/history"
        topology_history_report_path = "state/topology_history_report.json"
    }
    runtime_lane_registry = $RuntimeLanes
    power_lane_registry = $PowerLaneRegistryEntries
    assist = [ordered]@{
        context_report_id = "assist_context_report"
        context_report_uri = "report://assist/context"
        context_report_path = "state/assist_context_report.json"
        suggestion_report_id = "assist_suggestion_report"
        suggestion_report_uri = "report://assist/suggestion"
        suggestion_report_path = "state/assist_suggestion_report.json"
        tensor_artifact_id = "assist_tensor_artifact"
        tensor_artifact_uri = "artifact://assist/tensor-context"
        tensor_artifact_path = "state/assist_tensor_context.json"
        assistant_profile = "contextual_tensor_aware"
        context_summary = "mode=$ActiveModeLabel | layout=$($Manifest.layout_id) | tensor=warm"
        suggestion_summary = "watch selection, mode, and tensor dirty state together"
        workspace_layout_hint = $Manifest.layout_id
    }
    dirty = [ordered]@{
        asset_dirty = $false
        sculpt_dirty = $false
        topology_dirty = $false
        material_dirty = $false
        rig_dirty = $false
        animation_dirty = $false
        simulation_dirty = $false
        render_dirty = ($LatestFabricStatus -ne "succeeded")
        compositor_dirty = $false
        publish_dirty = $false
        tensor_dirty = $false
        session_needs_save = $false
    }
    jobs = [ordered]@{
        latest_fabric_session_id = $SessionId
        latest_fabric_status = $LatestFabricStatus
        active_intents = @($InitialIntentQueue | ForEach-Object { $_.id })
        active_jobs = @()
    }
}

$BridgeCommandQueuePath = Join-Path $StateRoot "command_queue.jsonl"
$BridgeSessionPath = Join-Path $StateRoot "session_document.json"
$BridgeStatus = [ordered]@{
    status = "ready"
    command_queue_path = $BridgeCommandQueuePath
    session_document_path = $BridgeSessionPath
    processed_command_count = 0
}
$RuntimeLaneHealth = if ($BridgeStatus.status -eq "live" -and $LatestFabricStatus -eq "succeeded") { "healthy" } elseif ($BridgeStatus.status -eq "live") { "bridge-live" } elseif ($LatestFabricStatus -eq "succeeded") { "fabric-green" } else { "warming" }
$RuntimeLaneHealthDetail = if ($RuntimeLaneHealth -eq "healthy") { "bridge live / fabric succeeded" } elseif ($RuntimeLaneHealth -eq "bridge-live") { "bridge live / fabric waiting" } elseif ($RuntimeLaneHealth -eq "fabric-green") { "fabric succeeded / bridge warming" } else { "bridge warming / fabric warming" }

$StepStatus = @($Pipeline | ForEach-Object {
    [ordered]@{
        id = $_.id
        runtime = $_.runtime
        status = if ($LatestFabricStatus -eq "idle") { "pending" } else { $LatestFabricStatus }
        summary = $_.summary
    }
})

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
    runtime_lane_registry = $RuntimeLanes
    power_lane_registry = $PowerLaneRegistryEntries
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
        recent_session_id = $SessionId
        recent_session_title = $RecentSessionTitle
    }
    recent_sessions = @(
        [ordered]@{
            id = $SessionId
            title = $RecentSessionTitle
            provider_id = "native_runtime"
            status = $LatestFabricStatus
            workspace_root = $RepoRoot
            updated_at = $NowIso
            message_count = 1
            last_message_role = "system"
            last_message_preview = New-RecentSessionPreview -FabricStatus $LatestFabricStatus -ModeLabel $ActiveModeLabel
        }
    )
    workspaces = @(
        [ordered]@{
            root = $RepoRoot
            session_count = 1
            recent_session_title = $RecentSessionTitle
        }
    )
    dcc_suite_state = [ordered]@{
        schema_version = 1
        manifest_registry = [ordered]@{
            app_manifest = "config/app_manifest.json"
            workspace_modes = "config/workspace_modes.json"
            surfaces = "config/surfaces.json"
            tool_catalog = "config/tool_catalog.json"
            command_registry = "config/command_registry.json"
            fabric_pipeline = "config/fabric_pipeline.json"
            fabric_intents = "config/fabric_intents.json"
            resource_kinds = "config/resource_kinds.json"
            mesh_resource_contract = "config/mesh_resource_contract.json"
            report_kinds = "config/report_kinds.json"
            runtime_packs = "config/runtime_packs.json"
            automation_jobs = "config/automation_jobs.json"
            gizmo_registry = "config/gizmo_registry.json"
            session_schema = "session/session_schema.kn"
            session_reducers = "session/reducers.kn"
            session_intent_planner = "session/intent_planner.kn"
        }
        command_registry = $Commands
        available_tools = $Tools
        workspace_modes = $Modes
        surface_registry = $Surfaces
        runtime_packs = $RuntimePacks
        gizmo_profiles = $GizmoRegistry.profiles
        viewport_gizmo_bindings = $GizmoRegistry.viewport_bindings
        resource_store = $Resources
        mesh_contract = [ordered]@{
            schema_version = $MeshContract.schema_version
            mesh_documents = $MeshContract.mesh_documents
            semantic_rules = $MeshContract.semantic_rules
        }
        report_store = $Reports
        automation_jobs = $Jobs
        intent_queue = $InitialIntentQueue
        latest_command = $null
        latest_fabric_run = [ordered]@{
            session_id = if ($null -eq $LatestReport) { $null } else { $LatestReport.session_id }
            status = $LatestFabricStatus
            manifest_path = "apps/kain-fabric-dcc-suite/KAIN.fabric.toml"
            steps = $StepStatus
        }
        bridge = $BridgeStatus
        bridge_status = $BridgeStatus.status
        workbench = [ordered]@{
            active_workbench_id = $ActiveModeId
            active_tab_group_id = $UiShell.page_tab_group_id
            active_dock_id = "dcc_workbench_pages"
            active_pane_id = "pane/viewport_stage"
            materialized_shell_path = "generated/main.generated.kn"
            runtime_snapshot_path = "state/runtime_snapshot.json"
        }
        runtime_lane_health = $RuntimeLaneHealth
        runtime_lane_health_detail = $RuntimeLaneHealthDetail
        runtime_lane_summary = $RuntimeLaneSummary
        runtime_lane_registry_summary = $RuntimeLaneRegistrySummary
        power_lane_registry_summary = $PowerLaneSummary
        runtime_pack_registry_entries = $RuntimePacks
        runtime_pack_count = @($RuntimePacks).Count
        runtime_pack_summary = $RuntimePackSummary
        fabric_intent_registry = $Intents
        fabric_intent_registry_entries = $Intents
        fabric_intent_count = @($Intents).Count
        fabric_intent_summary = $IntentSummary
        viewport_mode_count = @($ViewportModes).Count
        viewport_mode_summary = ($ViewportModes | ForEach-Object { $_.id }) -join " | "
        viewport_mode_registry_summary = ($ViewportModes | ForEach-Object { "$($_.id) => $($_.overlay_policy_id)" }) -join " | "
        viewport_mode_registry_entries = @($ViewportModeRegistryEntries)
        render_preview_chain = "pathtrace -> accumulation -> denoise"
        viewport_frame_feedback = $RuntimeSnapshot.viewport_frame_feedback
        extension_seams = @(
            "material lane still projects authoring receipts rather than a true native painter runtime",
            "tensor lane still reports readiness and plan state rather than executing a full typed tensor artifact contract",
            "simulation lane still materializes plan-oriented reports rather than a true solver runtime",
            "compositor lane still materializes rebuild plans rather than a first-class compositor graph runtime"
        )
    }
    updated_at = $NowIso
}

$RuntimeSnapshotJson = $RuntimeSnapshot | ConvertTo-Json -Depth 20
$SessionDocumentJson = $SessionDocument | ConvertTo-Json -Depth 12

Set-Content -Path (Join-Path $StateRoot "runtime_snapshot.json") -Value $RuntimeSnapshotJson
Set-Content -Path (Join-Path $NativeAppStateRoot "runtime_snapshot.json") -Value $RuntimeSnapshotJson
Set-Content -Path (Join-Path $StateRoot "session_document.json") -Value $SessionDocumentJson
Set-Content -Path (Join-Path $NativeAppStateRoot "session_document.json") -Value $SessionDocumentJson

if (-not (Test-Path $BridgeCommandQueuePath)) {
    Set-Content -Path $BridgeCommandQueuePath -Value ""
}

$NativeAppCommandQueuePath = Join-Path $NativeAppStateRoot "command_queue.jsonl"
if (-not (Test-Path $NativeAppCommandQueuePath)) {
    Set-Content -Path $NativeAppCommandQueuePath -Value ""
}

Write-Host "Materialized $(Join-Path $StateRoot 'runtime_snapshot.json')"
Write-Host "Materialized $(Join-Path $StateRoot 'session_document.json')"
