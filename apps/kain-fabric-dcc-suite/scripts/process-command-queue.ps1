param(
    [string]$AppRoot,
    [switch]$KeepQueue
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
    }
}

function Add-IntentIfMissing {
    param(
        [System.Collections.ArrayList]$IntentQueue,
        [hashtable]$Intent
    )

    foreach ($existing in $IntentQueue) {
        if ($existing.id -eq $Intent.id) {
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

    if ($Payload -is [hashtable] -and $Payload.ContainsKey($Key)) {
        return $Payload[$Key]
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

if ([string]::IsNullOrWhiteSpace($AppRoot)) {
    $AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
}

$AppRoot = (Resolve-Path $AppRoot).Path
$StateRoot = Join-Path $AppRoot "state"
$NativeStateRoot = Join-Path $AppRoot "native-app/state"
$CommandQueuePath = Join-Path $StateRoot "command_queue.jsonl"
$SessionPath = Join-Path $StateRoot "session_document.json"
$SnapshotPath = Join-Path $StateRoot "runtime_snapshot.json"
$NativeSessionPath = Join-Path $NativeStateRoot "session_document.json"
$NativeSnapshotPath = Join-Path $NativeStateRoot "runtime_snapshot.json"

Ensure-StateDocuments -ResolvedAppRoot $AppRoot

$RuntimeSnapshot = Get-JsonDocument -Path $SnapshotPath
$SessionDocument = Get-JsonDocument -Path $SessionPath

if (-not (Test-Path $CommandQueuePath)) {
    Set-Content -Path $CommandQueuePath -Value ""
}

$IntentQueue = New-Object System.Collections.ArrayList
foreach ($existingIntent in @($RuntimeSnapshot.dcc_suite_state.intent_queue)) {
    $null = $IntentQueue.Add([ordered]@{
        id = [string]$existingIntent.id
        reason = [string]$existingIntent.reason
        priority = [int]$existingIntent.priority
        debounce_ms = [int]$existingIntent.debounce_ms
        status = if ($null -eq $existingIntent.status) { "queued" } else { [string]$existingIntent.status }
        source_command_id = [string]$existingIntent.source_command_id
    })
}

$ProcessedCount = 0
$LatestCommand = $RuntimeSnapshot.dcc_suite_state.latest_command
$RawLines = @(Get-Content $CommandQueuePath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })

foreach ($rawLine in $RawLines) {
    $Command = $rawLine | ConvertFrom-Json
    $CommandKind = Get-CommandKind -Command $Command
    $Payload = Get-PayloadValue -Payload $Command -Key "payload" -Default @{}
    if ($null -eq $Payload) { $Payload = @{} }
    $CommandId = [string](Get-PayloadValue -Payload $Command -Key "command_id" -Default $(Get-PayloadValue -Payload $Command -Key "id" -Default ([guid]::NewGuid().ToString())))
    $IssuedAt = [string](Get-PayloadValue -Payload $Command -Key "issued_at" -Default ((Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")))

    switch ($CommandKind) {
        "workspace.switch_mode" {
            $SessionDocument.workspace.active_mode = [string](Get-PayloadValue -Payload $Payload -Key "mode_id" -Default $SessionDocument.workspace.active_mode)
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "render.preview" -Reason "workspace mode changed" -Priority 60 -DebounceMs 16 -SourceCommandId $CommandId)
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
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "asset.ingest_package" -Reason "asset package staged" -Priority 90 -DebounceMs 0 -SourceCommandId $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "render.preview" -Reason "ingested assets need preview refresh" -Priority 60 -DebounceMs 16 -SourceCommandId $CommandId)
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
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "sculpt.apply_stroke" -Reason "operator sculpt stroke" -Priority 95 -DebounceMs 0 -SourceCommandId $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "topology.rebuild" -Reason "sculpt changed topology-sensitive data" -Priority 80 -DebounceMs 32 -SourceCommandId $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "render.preview" -Reason "sculpt stroke needs viewport refresh" -Priority 60 -DebounceMs 16 -SourceCommandId $CommandId)
        }
        "topology.rebuild" {
            $SessionDocument.dirty.topology_dirty = $true
            $SessionDocument.dirty.rig_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "topology.rebuild" -Reason "topology rebuild requested" -Priority 80 -DebounceMs 32 -SourceCommandId $CommandId)
        }
        "rig.sync_controls" {
            $SessionDocument.workspace.active_mode = "rig_anim"
            $SessionDocument.rig.active_rig_id = [string](Get-PayloadValue -Payload $Payload -Key "rig_id" -Default $SessionDocument.rig.active_rig_id)
            $SessionDocument.dirty.rig_dirty = $true
            $SessionDocument.dirty.animation_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "rig.sync_controls" -Reason "rig sync requested" -Priority 72 -DebounceMs 24 -SourceCommandId $CommandId)
        }
        "sim.tick" {
            $SessionDocument.workspace.active_mode = "sim_fx"
            $SessionDocument.simulation.last_tick_frame = [int](Get-PayloadValue -Payload $Payload -Key "frame" -Default $SessionDocument.simulation.last_tick_frame)
            $SessionDocument.dirty.simulation_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "sim.tick" -Reason "simulation tick requested" -Priority 64 -DebounceMs 16 -SourceCommandId $CommandId)
        }
        "material.bake_preview" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "material.bake_preview" -Reason "material preview requested" -Priority 70 -DebounceMs 16 -SourceCommandId $CommandId)
        }
        "material.author_texture_set" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.materials.active_texture_set_id = [string](Get-PayloadValue -Payload $Payload -Key "texture_set_id" -Default $SessionDocument.materials.active_texture_set_id)
            $SessionDocument.materials.paint_resolution = [int](Get-PayloadValue -Payload $Payload -Key "resolution" -Default $SessionDocument.materials.paint_resolution)
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "material.bake_preview" -Reason "texture set changed" -Priority 70 -DebounceMs 16 -SourceCommandId $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "render.preview" -Reason "lookdev preview needs refresh" -Priority 60 -DebounceMs 16 -SourceCommandId $CommandId)
        }
        "material.paint_layer" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.tooling.active_tool = "material_layer_paint"
            $SessionDocument.materials.active_layer_stack_id = [string](Get-PayloadValue -Payload $Payload -Key "layer_id" -Default $SessionDocument.materials.active_layer_stack_id)
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "material.bake_preview" -Reason "material layer stack changed" -Priority 70 -DebounceMs 16 -SourceCommandId $CommandId)
        }
        "material.edit_svg_mask" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.tooling.active_tool = "svg_mask_shape"
            $SessionDocument.materials.active_svg_document_id = [string](Get-PayloadValue -Payload $Payload -Key "document_id" -Default $SessionDocument.materials.active_svg_document_id)
            $SessionDocument.dirty.material_dirty = $true
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "material.bake_preview" -Reason "svg mask graph changed" -Priority 70 -DebounceMs 16 -SourceCommandId $CommandId)
        }
        "material.export_textures" {
            $SessionDocument.workspace.active_mode = "material_lookdev"
            $SessionDocument.materials.active_export_preset_id = [string](Get-PayloadValue -Payload $Payload -Key "preset_id" -Default $SessionDocument.materials.active_export_preset_id)
            $SessionDocument.dirty.publish_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "material.export_textures" -Reason "material export requested" -Priority 74 -DebounceMs 0 -SourceCommandId $CommandId)
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "publish.package" -Reason "publish bundle depends on exported textures" -Priority 40 -DebounceMs 0 -SourceCommandId $CommandId)
        }
        "render.preview" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.render.camera_id = [string](Get-PayloadValue -Payload $Payload -Key "camera_id" -Default $SessionDocument.render.camera_id)
            $SessionDocument.dirty.render_dirty = $true
            $SessionDocument.dirty.compositor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "render.preview" -Reason "render preview requested" -Priority 60 -DebounceMs 16 -SourceCommandId $CommandId)
        }
        "compositor.rebuild" {
            $SessionDocument.workspace.active_mode = "render_comp"
            $SessionDocument.compositor.active_stack_id = [string](Get-PayloadValue -Payload $Payload -Key "stack_id" -Default $SessionDocument.compositor.active_stack_id)
            $SessionDocument.dirty.compositor_dirty = $true
            $SessionDocument.dirty.publish_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "compositor.rebuild" -Reason "compositor rebuild requested" -Priority 48 -DebounceMs 48 -SourceCommandId $CommandId)
        }
        "publish.package" {
            $SessionDocument.workspace.active_mode = "publish_automation"
            $SessionDocument.publish.profile_id = [string](Get-PayloadValue -Payload $Payload -Key "profile" -Default $SessionDocument.publish.profile_id)
            $SessionDocument.dirty.publish_dirty = $true
            $SessionDocument.dirty.session_needs_save = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "publish.package" -Reason "publish package requested" -Priority 40 -DebounceMs 0 -SourceCommandId $CommandId)
        }
        "tensor.train_step" {
            $SessionDocument.dirty.tensor_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "tensor.train_step" -Reason "tensor training requested" -Priority 55 -DebounceMs 0 -SourceCommandId $CommandId)
        }
        "tensor.infer_step" {
            $SessionDocument.dirty.tensor_dirty = $true
            $SessionDocument.dirty.publish_dirty = $true
            Add-IntentIfMissing -IntentQueue $IntentQueue -Intent (New-Intent -IntentId "tensor.infer_step" -Reason "tensor inference requested" -Priority 55 -DebounceMs 0 -SourceCommandId $CommandId)
        }
        default {
        }
    }

    $LatestCommand = [ordered]@{
        id = $CommandId
        kind = $CommandKind
        issued_at = $IssuedAt
        payload = $Payload
    }

    $ProcessedCount += 1
}

$SessionDocument.jobs.active_intents = @($IntentQueue | ForEach-Object { $_.id })
$SessionDocument.jobs.latest_fabric_status = if (@($IntentQueue).Count -gt 0) { "queued" } else { [string]$SessionDocument.jobs.latest_fabric_status }

if (-not ($RuntimeSnapshot.dcc_suite_state.bridge.PSObject.Properties.Name -contains "processed_command_count")) {
    $RuntimeSnapshot.dcc_suite_state.bridge | Add-Member -NotePropertyName processed_command_count -NotePropertyValue 0
}
if (-not ($RuntimeSnapshot.dcc_suite_state.bridge.PSObject.Properties.Name -contains "last_processed_at")) {
    $RuntimeSnapshot.dcc_suite_state.bridge | Add-Member -NotePropertyName last_processed_at -NotePropertyValue $null
}
if (-not ($RuntimeSnapshot.dcc_suite_state.bridge.PSObject.Properties.Name -contains "last_processed_batch_count")) {
    $RuntimeSnapshot.dcc_suite_state.bridge | Add-Member -NotePropertyName last_processed_batch_count -NotePropertyValue 0
}
$RuntimeSnapshot.dcc_suite_state.bridge.processed_command_count = [int]$RuntimeSnapshot.dcc_suite_state.bridge.processed_command_count + $ProcessedCount
$RuntimeSnapshot.dcc_suite_state.bridge.last_processed_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$RuntimeSnapshot.dcc_suite_state.bridge.last_processed_batch_count = $ProcessedCount

Update-DerivedState -RuntimeSnapshot $RuntimeSnapshot -SessionDocument $SessionDocument -IntentQueue $IntentQueue -LatestCommand $LatestCommand

Set-JsonFile -Path $SessionPath -Value $SessionDocument -Depth 16
Set-JsonFile -Path $SnapshotPath -Value $RuntimeSnapshot -Depth 24

New-Item -ItemType Directory -Force -Path $NativeStateRoot | Out-Null
Set-JsonFile -Path $NativeSessionPath -Value $SessionDocument -Depth 16
Set-JsonFile -Path $NativeSnapshotPath -Value $RuntimeSnapshot -Depth 24

if (-not $KeepQueue) {
    Set-Content -Path $CommandQueuePath -Value ""
    $NativeQueuePath = Join-Path $NativeStateRoot "command_queue.jsonl"
    if (Test-Path $NativeQueuePath) {
        Set-Content -Path $NativeQueuePath -Value ""
    }
}

Write-Host "Processed $ProcessedCount command(s) from $CommandQueuePath"
