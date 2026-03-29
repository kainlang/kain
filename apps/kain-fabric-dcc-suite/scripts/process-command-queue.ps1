param(
    [string]$AppRoot,
    [switch]$KeepQueue,
    [switch]$ExecuteFabricHotPath,
    [switch]$RegenerateShell
)

$ErrorActionPreference = "Stop"

function Ensure-StateDocuments {
    param([string]$ResolvedAppRoot)

    $sessionPath = Join-Path $ResolvedAppRoot "state/session_document.json"
    $snapshotPath = Join-Path $ResolvedAppRoot "state/runtime_snapshot.json"
    if ((-not (Test-Path $sessionPath)) -or (-not (Test-Path $snapshotPath))) {
        powershell -ExecutionPolicy Bypass -File (Join-Path $ResolvedAppRoot "scripts/materialize-session-state.ps1")
    }
}

function Get-JsonDocument {
    param([string]$Path)
    return Get-Content $Path -Raw | ConvertFrom-Json
}

function Set-JsonFile {
    param(
        [string]$Path,
        $Value,
        [int]$Depth = 24
    )

    $Value | ConvertTo-Json -Depth $Depth | Set-Content $Path
}

function New-Intent {
    param(
        [string]$IntentId,
        [string]$Reason,
        [int]$Priority,
        [int]$DebounceMs,
        [string]$SourceCommandId
    )

    return [ordered]@{
        id = $IntentId
        reason = $Reason
        priority = $Priority
        debounce_ms = $DebounceMs
        status = "queued"
        source_command_id = $SourceCommandId
        enqueued_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    }
}

function Get-ElapsedMillisecondsSinceIso {
    param([string]$IsoTimestamp)

    if ([string]::IsNullOrWhiteSpace($IsoTimestamp)) {
        return [double]::PositiveInfinity
    }

    try {
        $then = [DateTimeOffset]::Parse($IsoTimestamp)
        return (([DateTimeOffset]::UtcNow) - $then).TotalMilliseconds
    }
    catch {
        return [double]::PositiveInfinity
    }
}

function Add-IntentIfMissing {
    param(
        [System.Collections.ArrayList]$IntentQueue,
        $Intent,
        $Bridge
    )

    foreach ($existing in $IntentQueue) {
        if ($existing.id -eq $Intent.id) {
            return
        }
    }

    foreach ($recentIntent in @($Bridge.recent_intents)) {
        if ([string]$recentIntent.id -ne [string]$Intent.id) {
            continue
        }

        $elapsedMs = Get-ElapsedMillisecondsSinceIso -IsoTimestamp ([string]$recentIntent.updated_at)
        if ($elapsedMs -lt [int]$Intent.debounce_ms) {
            return
        }
    }

    $null = $IntentQueue.Add($Intent)
}

function Get-CommandKind {
    param($Command)

    if ($Command.PSObject.Properties.Name -contains "kind" -and -not [string]::IsNullOrWhiteSpace([string]$Command.kind)) {
        return [string]$Command.kind
    }
    if ($Command.PSObject.Properties.Name -contains "id" -and -not [string]::IsNullOrWhiteSpace([string]$Command.id)) {
        return [string]$Command.id
    }
    if ($Command.PSObject.Properties.Name -contains "command_id" -and -not [string]::IsNullOrWhiteSpace([string]$Command.command_id)) {
        return [string]$Command.command_id
    }

    throw "Command record missing kind/id/command_id."
}

function Get-PayloadValue {
    param(
        $Payload,
        [string]$Key,
        $Default = $null
    )

    if ($null -eq $Payload) {
        return $Default
    }
    if ($Payload.PSObject.Properties.Name -contains $Key) {
        return $Payload.$Key
    }
    return $Default
}

function Update-DerivedState {
    param(
        $RuntimeSnapshot,
        $SessionDocument,
        [System.Collections.ArrayList]$IntentQueue,
        $LatestCommand
    )

    $selectedCount = @($SessionDocument.selection.entity_ids).Count
    $gizmoSummary = "{0} | {1} | snap {2}" -f $SessionDocument.gizmo.mode, $SessionDocument.gizmo.space, ($(if ($SessionDocument.gizmo.snap_enabled) { "on" } else { "off" }))

    if (-not ($RuntimeSnapshot.dcc_suite_state.PSObject.Properties.Name -contains "session")) {
        $RuntimeSnapshot.dcc_suite_state | Add-Member -NotePropertyName session -NotePropertyValue $SessionDocument
    }
    if (-not ($RuntimeSnapshot.dcc_suite_state.PSObject.Properties.Name -contains "intent_queue")) {
        $RuntimeSnapshot.dcc_suite_state | Add-Member -NotePropertyName intent_queue -NotePropertyValue @()
    }
    if (-not ($RuntimeSnapshot.dcc_suite_state.PSObject.Properties.Name -contains "latest_command")) {
        $RuntimeSnapshot.dcc_suite_state | Add-Member -NotePropertyName latest_command -NotePropertyValue $null
    }
    if (-not ($RuntimeSnapshot.dcc_suite_state.PSObject.Properties.Name -contains "derived")) {
        $RuntimeSnapshot.dcc_suite_state | Add-Member -NotePropertyName derived -NotePropertyValue ([ordered]@{})
    }

    $RuntimeSnapshot.dcc_suite_state.session = $SessionDocument
    $RuntimeSnapshot.dcc_suite_state.intent_queue = $IntentQueue
    $RuntimeSnapshot.dcc_suite_state.latest_command = $LatestCommand
    $RuntimeSnapshot.dcc_suite_state.derived = [ordered]@{
        active_mode_label = [string]$SessionDocument.workspace.active_mode
        active_tool_label = [string]$SessionDocument.tooling.active_tool
        selection_summary = "$selectedCount entity selected"
        gizmo_summary = $gizmoSummary
        queued_intent_count = @($IntentQueue).Count
        latest_fabric_status = [string]$SessionDocument.jobs.latest_fabric_status
    }
}

function Ensure-BridgeProperty {
    param(
        $Bridge,
        [string]$Name,
        $Value
    )

    if (-not ($Bridge.PSObject.Properties.Name -contains $Name)) {
        $Bridge | Add-Member -NotePropertyName $Name -NotePropertyValue $Value
    }
}

function Get-IntentGraphLookup {
    param([string]$ConfigPath)

    $lookup = @{}
    $config = Get-JsonDocument -Path $ConfigPath
    foreach ($intent in @($config.intents)) {
        $lookup[[string]$intent.id] = [string]$intent.graph
    }
    return $lookup
}

function Invoke-FabricIntent {
    param(
        [string]$RepoRoot,
        [string]$AppRoot,
        [string]$GraphPath,
        [string]$IntentId
    )

    if ([string]::IsNullOrWhiteSpace($GraphPath)) {
        return [ordered]@{ id = $IntentId; status = "skipped"; reason = "no graph path" }
    }

    $manifestPath = Join-Path $AppRoot $GraphPath
    if (-not (Test-Path $manifestPath)) {
        return [ordered]@{ id = $IntentId; status = "missing"; manifest = $manifestPath }
    }

    Push-Location $RepoRoot
    try {
        & cargo run -p cli --bin kain -- fabric run --manifest ("apps/kain-fabric-dcc-suite/" + $GraphPath)
        if ($LASTEXITCODE -ne 0) {
            return [ordered]@{ id = $IntentId; status = "failed"; manifest = $manifestPath; exit_code = $LASTEXITCODE }
        }
        return [ordered]@{ id = $IntentId; status = "succeeded"; manifest = $manifestPath; exit_code = 0 }
    }
    finally {
        Pop-Location
    }
}

if ([string]::IsNullOrWhiteSpace($AppRoot)) {
    $AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
}

$AppRoot = (Resolve-Path $AppRoot).Path
$RepoRoot = (Resolve-Path (Join-Path $AppRoot "..\.." )).Path
$StateRoot = Join-Path $AppRoot "state"
$NativeStateRoot = Join-Path $AppRoot "native-app/state"
$CommandQueuePath = Join-Path $StateRoot "command_queue.jsonl"
$SessionPath = Join-Path $StateRoot "session_document.json"
$SnapshotPath = Join-Path $StateRoot "runtime_snapshot.json"
$NativeSessionPath = Join-Path $NativeStateRoot "session_document.json"
$NativeSnapshotPath = Join-Path $NativeStateRoot "runtime_snapshot.json"
$IntentConfigPath = Join-Path $AppRoot "config/fabric_intents.json"

Ensure-StateDocuments -ResolvedAppRoot $AppRoot

$RuntimeSnapshot = Get-JsonDocument -Path $SnapshotPath
$SessionDocument = Get-JsonDocument -Path $SessionPath
$IntentGraphLookup = Get-IntentGraphLookup -ConfigPath $IntentConfigPath
$Bridge = $RuntimeSnapshot.dcc_suite_state.bridge
Ensure-BridgeProperty -Bridge $Bridge -Name processed_command_count -Value 0
Ensure-BridgeProperty -Bridge $Bridge -Name last_processed_at -Value $null
Ensure-BridgeProperty -Bridge $Bridge -Name last_processed_batch_count -Value 0
Ensure-BridgeProperty -Bridge $Bridge -Name dispatcher_mode -Value "queue-pass"
Ensure-BridgeProperty -Bridge $Bridge -Name last_fabric_results -Value @()
Ensure-BridgeProperty -Bridge $Bridge -Name pending_intents -Value @()
Ensure-BridgeProperty -Bridge $Bridge -Name running_intents -Value @()
Ensure-BridgeProperty -Bridge $Bridge -Name recent_intents -Value @()

if (-not (Test-Path $CommandQueuePath)) {
    Set-Content -Path $CommandQueuePath -Value ""
}

$IntentQueue = New-Object System.Collections.ArrayList
$Bridge.pending_intents = @()
$Bridge.running_intents = @()

$ProcessedCount = 0
$LatestCommand = $RuntimeSnapshot.dcc_suite_state.latest_command
$RawLines = @(Get-Content $CommandQueuePath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

foreach ($rawLine in $RawLines) {
    $Command = $rawLine | ConvertFrom-Json
    $CommandKind = Get-CommandKind -Command $Command
    $Payload = Get-PayloadValue -Payload $Command -Key "payload" -Default ([pscustomobject]@{})
    $CommandId = [string](Get-PayloadValue -Payload $Command -Key "id" -Default ([guid]::NewGuid().ToString()))
    $IssuedAt = [string](Get-PayloadValue -Payload $Command -Key "issued_at" -Default ((Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")))

    switch ($CommandKind) {
        "workspace.switch_mode" {
            $SessionDocument.workspace.active_mode = [string](Get-PayloadValue -Payload $Payload -Key "mode_id" -Default $SessionDocument.workspace.active_mode)
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.preview" "workspace mode changed" 60 16 $CommandId)
        }
        "asset.ingest_package" {
            $SessionDocument.ingest.last_package_uri = [string](Get-PayloadValue -Payload $Payload -Key "source_uri" -Default $SessionDocument.ingest.last_package_uri)
            $SessionDocument.ingest.last_package_kind = [string](Get-PayloadValue -Payload $Payload -Key "package_kind" -Default $SessionDocument.ingest.last_package_kind)
            $SessionDocument.ingest.staged_package_count = [int]$SessionDocument.ingest.staged_package_count + 1
            $SessionDocument.workspace.active_mode = "scene_assembly"
            $SessionDocument.dirty.asset_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.publish_dirty = $true
            $SessionDocument.dirty.tensor_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "asset.ingest_package" "asset package staged" 90 0 $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.preview" "ingested assets need preview refresh" 60 16 $CommandId)
        }
        "tool.activate" {
            $SessionDocument.tooling.active_tool = [string](Get-PayloadValue -Payload $Payload -Key "tool_id" -Default $SessionDocument.tooling.active_tool)
        }
        "selection.set" {
            $SessionDocument.selection.entity_ids = @([string[]](Get-PayloadValue -Payload $Payload -Key "entity_ids" -Default @()))
            $SessionDocument.selection.subobject_ids = @([string[]](Get-PayloadValue -Payload $Payload -Key "subobject_ids" -Default @()))
        }
        "gizmo.set_mode" {
            $SessionDocument.gizmo.mode = [string](Get-PayloadValue -Payload $Payload -Key "mode" -Default $SessionDocument.gizmo.mode)
        }
        "gizmo.set_space" {
            $SessionDocument.gizmo.space = [string](Get-PayloadValue -Payload $Payload -Key "space" -Default $SessionDocument.gizmo.space)
        }
        "gizmo.toggle_snap" {
            $SessionDocument.gizmo.snap_enabled = [bool](Get-PayloadValue -Payload $Payload -Key "enabled" -Default (-not [bool]$SessionDocument.gizmo.snap_enabled))
        }
        "sculpt.apply_stroke" {
            $SessionDocument.workspace.active_mode = "sculpt_model"
            $SessionDocument.tooling.active_tool = [string](Get-PayloadValue -Payload $Payload -Key "brush_id" -Default "sculpt_brush")
            $SessionDocument.tooling.brush_radius = [int](Get-PayloadValue -Payload $Payload -Key "radius" -Default $SessionDocument.tooling.brush_radius)
            $SessionDocument.tooling.brush_strength_percent = [int](Get-PayloadValue -Payload $Payload -Key "strength" -Default $SessionDocument.tooling.brush_strength_percent)
            $SessionDocument.dirty.sculpt_dirty = $true
            $SessionDocument.dirty.topology_dirty = $true
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.publish_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "sculpt.apply_stroke" "operator sculpt stroke" 95 0 $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "topology.rebuild" "sculpt changed topology-sensitive data" 80 32 $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.preview" "sculpt stroke needs viewport refresh" 60 16 $CommandId)
        }
        "topology.rebuild" {
            $SessionDocument.dirty.topology_dirty = $true
            $SessionDocument.dirty.rig_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "topology.rebuild" "topology rebuild requested" 80 32 $CommandId)
        }
        "rig.sync_controls" {
            $SessionDocument.workspace.active_mode = "rig_anim"
            $SessionDocument.rig.active_rig_id = [string](Get-PayloadValue -Payload $Payload -Key "rig_id" -Default $SessionDocument.rig.active_rig_id)
            $SessionDocument.dirty.rig_dirty = $true
            $SessionDocument.dirty.animation_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "rig.sync_controls" "rig sync requested" 72 24 $CommandId)
        }
        "sim.tick" {
            $SessionDocument.workspace.active_mode = "sim_fx"
            $SessionDocument.simulation.last_tick_frame = [int](Get-PayloadValue -Payload $Payload -Key "frame" -Default $SessionDocument.simulation.last_tick_frame)
            $SessionDocument.dirty.simulation_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "sim.tick" "simulation tick requested" 64 16 $CommandId)
        }
        "material.bake_preview" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "material.bake_preview" "material preview requested" 70 16 $CommandId)
        }
        "material.author_texture_set" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.materials.active_texture_set_id = [string](Get-PayloadValue -Payload $Payload -Key "texture_set_id" -Default $SessionDocument.materials.active_texture_set_id)
            $SessionDocument.materials.paint_resolution = [int](Get-PayloadValue -Payload $Payload -Key "resolution" -Default $SessionDocument.materials.paint_resolution)
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "material.bake_preview" "texture set changed" 70 16 $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.preview" "lookdev preview needs refresh" 60 16 $CommandId)
        }
        "material.paint_layer" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.tooling.active_tool = "material_layer_paint"
            $SessionDocument.materials.active_layer_stack_id = [string](Get-PayloadValue -Payload $Payload -Key "layer_id" -Default $SessionDocument.materials.active_layer_stack_id)
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "material.bake_preview" "material layer stack changed" 70 16 $CommandId)
        }
        "material.edit_svg_mask" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.tooling.active_tool = "svg_mask_shape"
            $SessionDocument.materials.active_svg_document_id = [string](Get-PayloadValue -Payload $Payload -Key "document_id" -Default $SessionDocument.materials.active_svg_document_id)
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "material.bake_preview" "svg mask graph changed" 70 16 $CommandId)
        }
        "material.export_textures" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.materials.active_export_preset_id = [string](Get-PayloadValue -Payload $Payload -Key "preset_id" -Default $SessionDocument.materials.active_export_preset_id)
            $SessionDocument.dirty.publish_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "material.export_textures" "material export requested" 74 0 $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "publish.package" "publish bundle depends on exported textures" 40 0 $CommandId)
        }
        "render.delegate_preview" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.render.camera_id = [string](Get-PayloadValue -Payload $Payload -Key "camera_id" -Default $SessionDocument.render.camera_id)
            $SessionDocument.dirty.render_dirty = $false
            $SessionDocument.dirty.compositor_dirty = $false
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.delegate_preview" "delegated render room preview requested" 64 24 $CommandId)
        }
        "lighting.review_preview" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.render.camera_id = [string](Get-PayloadValue -Payload $Payload -Key "camera_id" -Default $SessionDocument.render.camera_id)
            if (-not ($SessionDocument.render.PSObject.Properties.Name -contains "lighting_profile_id")) {
                $SessionDocument.render | Add-Member -NotePropertyName lighting_profile_id -NotePropertyValue ""
            }
            $SessionDocument.render.lighting_profile_id = [string](Get-PayloadValue -Payload $Payload -Key "lighting_profile" -Default $SessionDocument.render.lighting_profile_id)
            $SessionDocument.dirty.render_dirty = $false
            $SessionDocument.dirty.material_dirty = $false
            $SessionDocument.dirty.compositor_dirty = $false
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "lighting.review_preview" "lighting review requested" 66 24 $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.review_capture" "lighting review needs capture and AOV telemetry" 68 32 $CommandId)
        }
        "render.review_capture" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.render.camera_id = [string](Get-PayloadValue -Payload $Payload -Key "camera_id" -Default $SessionDocument.render.camera_id)
            $SessionDocument.dirty.render_dirty = $false
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.publish_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.review_capture" "render review capture requested" 68 32 $CommandId)
        }
        "render.preview" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.render.camera_id = [string](Get-PayloadValue -Payload $Payload -Key "camera_id" -Default $SessionDocument.render.camera_id)
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.preview" "render preview requested" 60 16 $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.pathtrace_preview" "path-traced preview requested" 88 24 $CommandId)
        }
        "render.pathtrace_preview" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.render.camera_id = [string](Get-PayloadValue -Payload $Payload -Key "camera_id" -Default $SessionDocument.render.camera_id)
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "render.pathtrace_preview" "path-traced preview requested" 88 24 $CommandId)
        }
        "compositor.rebuild" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.compositor.active_stack_id = [string](Get-PayloadValue -Payload $Payload -Key "stack_id" -Default $SessionDocument.compositor.active_stack_id)
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.publish_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "compositor.rebuild" "compositor rebuild requested" 48 48 $CommandId)
        }
        "publish.package" {
            $SessionDocument.workspace.active_mode = "publish_automation"
            $SessionDocument.publish.profile_id = [string](Get-PayloadValue -Payload $Payload -Key "profile" -Default $SessionDocument.publish.profile_id)
            $SessionDocument.dirty.publish_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "publish.package" "publish package requested" 40 0 $CommandId)
        }
        "tensor.train_step" {
            $SessionDocument.dirty.tensor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "tensor.train_step" "tensor training requested" 55 0 $CommandId)
        }
        "tensor.infer_step" {
            $SessionDocument.dirty.tensor_dirty = $true
            $SessionDocument.dirty.publish_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Bridge $Bridge -Intent (New-Intent "tensor.infer_step" "tensor inference requested" 55 0 $CommandId)
        }
        default { }
    }

    $LatestCommand = [ordered]@{
        id = $CommandId
        kind = $CommandKind
        issued_at = $IssuedAt
        payload = $Payload
    }

    $ProcessedCount += 1
}

$ExecutedFabricResults = New-Object System.Collections.ArrayList
$RunningIntents = New-Object System.Collections.ArrayList
$CompletedIntents = New-Object System.Collections.ArrayList
$PendingIntents = New-Object System.Collections.ArrayList

if ($ExecuteFabricHotPath -and @($IntentQueue).Count -gt 0) {
    $hotPathIntentIds = @("material.bake_preview", "render.preview", "render.pathtrace_preview", "render.delegate_preview", "lighting.review_preview", "render.review_capture")
    $sortedIntents = @($IntentQueue | Sort-Object priority -Descending)
    foreach ($intent in $sortedIntents) {
        if ($hotPathIntentIds -contains [string]$intent.id) {
            $intent.status = "running"
            $null = $RunningIntents.Add([ordered]@{
                id = [string]$intent.id
                status = "running"
                source_command_id = [string]$intent.source_command_id
                updated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
            })

            $graphPath = $IntentGraphLookup[[string]$intent.id]
            $result = Invoke-FabricIntent -RepoRoot $RepoRoot -AppRoot $AppRoot -GraphPath $graphPath -IntentId ([string]$intent.id)
            $null = $ExecutedFabricResults.Add($result)
            $intent.status = [string]$result.status
            $null = $CompletedIntents.Add([ordered]@{
                id = [string]$intent.id
                status = [string]$result.status
                source_command_id = [string]$intent.source_command_id
                debounce_ms = [int]$intent.debounce_ms
                updated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
            })
            continue
        }

        $null = $PendingIntents.Add([ordered]@{
            id = [string]$intent.id
            reason = [string]$intent.reason
            priority = [int]$intent.priority
            debounce_ms = [int]$intent.debounce_ms
            status = "queued"
            source_command_id = [string]$intent.source_command_id
            updated_at = [string]$intent.enqueued_at
        })
    }

    if (@($ExecutedFabricResults).Count -gt 0) {
        $hasFailure = @($ExecutedFabricResults | Where-Object { $_.status -ne "succeeded" }).Count -gt 0
        $SessionDocument.jobs.latest_fabric_status = if ($hasFailure) { "failed" } else { "succeeded" }
        $SessionDocument.dirty.material_dirty = $false
        $SessionDocument.dirty.render_dirty = $false
        $SessionDocument.dirty.compositor_dirty = $false
    }
}
else {
    foreach ($intent in @($IntentQueue | Sort-Object priority -Descending)) {
        $null = $PendingIntents.Add([ordered]@{
            id = [string]$intent.id
            reason = [string]$intent.reason
            priority = [int]$intent.priority
            debounce_ms = [int]$intent.debounce_ms
            status = "queued"
            source_command_id = [string]$intent.source_command_id
            updated_at = [string]$intent.enqueued_at
        })
    }
    $SessionDocument.jobs.latest_fabric_status = if (@($PendingIntents).Count -gt 0) { "queued" } else { [string]$SessionDocument.jobs.latest_fabric_status }
}

$Bridge.pending_intents = @($PendingIntents)
$Bridge.running_intents = @()
$existingRecentIntents = @($Bridge.recent_intents)
$Bridge.recent_intents = @($CompletedIntents + $existingRecentIntents | Select-Object -First 20)

$SessionDocument.jobs.active_intents = @($PendingIntents | ForEach-Object { $_.id })

$Bridge.processed_command_count = [int]$Bridge.processed_command_count + $ProcessedCount
$Bridge.last_processed_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$Bridge.last_processed_batch_count = $ProcessedCount
$Bridge.dispatcher_mode = if ($ExecuteFabricHotPath) { "queue-pass+fabric-hot-path" } else { "queue-pass" }
$Bridge.last_fabric_results = @($ExecutedFabricResults)

$RuntimeSnapshot.dcc_suite_state.latest_fabric_run = [ordered]@{
    session_id = [string]$SessionDocument.jobs.latest_fabric_session_id
    status = [string]$SessionDocument.jobs.latest_fabric_status
    manifest_path = "apps/kain-fabric-dcc-suite/KAIN.fabric.toml"
    hot_path_results = @($ExecutedFabricResults)
}

Update-DerivedState -RuntimeSnapshot $RuntimeSnapshot -SessionDocument $SessionDocument -IntentQueue $PendingIntents -LatestCommand $LatestCommand

Set-JsonFile -Path $SessionPath -Value $SessionDocument -Depth 16
Set-JsonFile -Path $SnapshotPath -Value $RuntimeSnapshot -Depth 24

New-Item -ItemType Directory -Force -Path $NativeStateRoot | Out-Null
Set-JsonFile -Path $NativeSessionPath -Value $SessionDocument -Depth 16
Set-JsonFile -Path $NativeSnapshotPath -Value $RuntimeSnapshot -Depth 24

if ($RegenerateShell) {
    powershell -ExecutionPolicy Bypass -File (Join-Path $AppRoot "scripts/materialize-shell.ps1")
}

if (-not $KeepQueue) {
    Set-Content -Path $CommandQueuePath -Value ""
    $NativeQueuePath = Join-Path $NativeStateRoot "command_queue.jsonl"
    if (Test-Path $NativeQueuePath) {
        Set-Content -Path $NativeQueuePath -Value ""
    }
}

Write-Host "Processed $ProcessedCount command(s) from $CommandQueuePath"
if (@($ExecutedFabricResults).Count -gt 0) {
    Write-Host ("Executed Fabric hot path intents: " + ((@($ExecutedFabricResults) | ForEach-Object { $_.id + ":" + $_.status }) -join ", "))
}
