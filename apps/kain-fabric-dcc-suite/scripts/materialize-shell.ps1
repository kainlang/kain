$ErrorActionPreference = "Stop"

function ConvertTo-KnEscapedText {
    param([string]$Value)

    if ($null -eq $Value) {
        return ""
    }

    return ($Value -replace "\\", "\\\\") -replace '"', '\"'
}

function Format-KnAttributeValue {
    param($Value)

    if ($Value -is [string]) {
        return "`"$(ConvertTo-KnEscapedText $Value)`""
    }

    if ($Value -is [bool]) {
        return "{" + $Value.ToString().ToLowerInvariant() + "}"
    }

    if ($Value -is [int] -or
        $Value -is [long] -or
        $Value -is [double] -or
        $Value -is [decimal] -or
        $Value -is [single]) {
        return "{" + [string]$Value + "}"
    }

    return "`"$(ConvertTo-KnEscapedText ([string]$Value))`""
}

function Render-TextNode {
    param(
        [string]$Role,
        [string]$Value,
        [string]$Indent = ""
    )

    return "$Indent<text role=`"$Role`">{`"$(ConvertTo-KnEscapedText $Value)`"}</text>"
}

function Render-TextLines {
    param(
        [System.Collections.IEnumerable]$Items,
        [scriptblock]$Formatter,
        [string]$Role = "body",
        [string]$Indent = ""
    )

    $lines = @()
    foreach ($item in $Items) {
        $text = [string](& $Formatter $item)
        if ([string]::IsNullOrWhiteSpace($text)) {
            continue
        }
        $lines += Render-TextNode -Role $Role -Value $text -Indent $Indent
    }
    return $lines
}

function Join-Lines {
    param([System.Collections.IEnumerable]$Lines)
    return ($Lines -join "`n")
}

function Add-Line {
    param(
        [System.Collections.Generic.List[string]]$Target,
        [string]$Line
    )

    $Target.Add($Line) | Out-Null
}

function Add-Lines {
    param(
        [System.Collections.Generic.List[string]]$Target,
        [System.Collections.IEnumerable]$Lines
    )

    foreach ($line in $Lines) {
        $Target.Add([string]$line) | Out-Null
    }
}

function New-LookupTable {
    param(
        [System.Collections.IEnumerable]$Items,
        [string]$KeyProperty
    )

    $lookup = @{}
    foreach ($item in $Items) {
        $lookup[$item.$KeyProperty] = $item
    }
    return $lookup
}

function Get-ResolvedItems {
    param(
        [System.Collections.IEnumerable]$Ids,
        [hashtable]$Lookup
    )

    $resolved = @()
    foreach ($id in $Ids) {
        if ($Lookup.ContainsKey($id)) {
            $resolved += $Lookup[$id]
        }
    }
    return ,$resolved
}

function Render-ThemeBlock {
    param(
        $Theme,
        [string]$Indent = "        "
    )

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<theme name=`"$($Theme.theme_name)`">"

    foreach ($scope in $Theme.scopes) {
        Add-Line $lines "$Indent    <scope name=`"$($scope.name)`" selector=`"$($scope.selector)`" />"
    }

    foreach ($token in $Theme.tokens) {
        Add-Line $lines "$Indent    <token name=`"$($token.name)`" category=`"$($token.category)`" value=$(Format-KnAttributeValue $token.value) />"
    }

    foreach ($variant in $Theme.variants) {
        Add-Line $lines "$Indent    <variant scope=`"$($variant.scope)`" name=`"$($variant.name)`">"
        foreach ($token in $variant.tokens) {
            Add-Line $lines "$Indent        <token name=`"$($token.name)`" category=`"$($token.category)`" value=$(Format-KnAttributeValue $token.value) />"
        }
        Add-Line $lines "$Indent    </variant>"
    }

    foreach ($textVariant in $Theme.text_variants) {
        Add-Line $lines "$Indent    <textvariant scope=`"$($textVariant.scope)`" name=`"$($textVariant.name)`">"
        foreach ($token in $textVariant.tokens) {
            Add-Line $lines "$Indent        <token name=`"$($token.name)`" category=`"$($token.category)`" value=$(Format-KnAttributeValue $token.value) />"
        }
        Add-Line $lines "$Indent    </textvariant>"
    }

    foreach ($widget in $Theme.widget_defaults) {
        Add-Line $lines "$Indent    <widget kind=`"$($widget.kind)`" scope=`"$($widget.scope)`" variant=`"$($widget.variant)`">"
        foreach ($token in $widget.tokens) {
            Add-Line $lines "$Indent        <token name=`"$($token.name)`" category=`"$($token.category)`" value=$(Format-KnAttributeValue $token.value) />"
        }
        Add-Line $lines "$Indent    </widget>"
    }

    Add-Line $lines "$Indent</theme>"
    return $lines
}

function Get-ViewportProps {
    param(
        [System.Collections.IEnumerable]$Surfaces,
        $GizmoRegistry,
        [string]$FallbackScene
    )

    $viewportSurface = $Surfaces | Where-Object { $_.id -eq "viewport_stage" } | Select-Object -First 1
    $viewportBinding = $GizmoRegistry.viewport_bindings | Where-Object { $_.surface_id -eq "viewport_stage" } | Select-Object -First 1
    $gizmoProfile = $GizmoRegistry.profiles | Where-Object { $_.id -eq $viewportBinding.gizmo_profile_id } | Select-Object -First 1

    $sceneName = if ($null -eq $viewportBinding) { $FallbackScene } else { $viewportBinding.scene }
    $surfaceTitle = if ($null -eq $viewportSurface) { "Viewport Stage" } else { $viewportSurface.title }

    $props = @(
        "title=`"$surfaceTitle`"",
        "scene=`"$sceneName`""
    )

    if ($null -ne $viewportBinding) {
        $props += "viewport_profile=`"$($viewportBinding.presentation_profile)`""
        $props += "gizmo_default_mode=`"$($viewportBinding.default_mode)`""
        $props += "gizmo_default_space=`"$($viewportBinding.default_space)`""
    }

    if ($null -ne $gizmoProfile) {
        $props += "gizmo_profile=`"$($gizmoProfile.id)`""
        $props += "gizmo_visible={" + $gizmoProfile.visible.ToString().ToLowerInvariant() + "}"
        $props += "gizmo_drag_trigger=`"$($gizmoProfile.drag_trigger)`""
        $props += "gizmo_selection_required={" + $gizmoProfile.selection_required.ToString().ToLowerInvariant() + "}"
        $props += "gizmo_hotkey_translate=`"$($gizmoProfile.hotkeys.translate)`""
        $props += "gizmo_hotkey_rotate=`"$($gizmoProfile.hotkeys.rotate)`""
        $props += "gizmo_hotkey_scale=`"$($gizmoProfile.hotkeys.scale)`""
        $props += "gizmo_hotkey_cycle_space=`"$($gizmoProfile.hotkeys.cycle_space)`""
        $props += "gizmo_hotkey_toggle_snap=`"$($gizmoProfile.hotkeys.toggle_snap)`""
        $props += "gizmo_snap_translate={$($gizmoProfile.snap.translate_world_units)}"
        $props += "gizmo_snap_rotate_degrees={$($gizmoProfile.snap.rotate_degrees)}"
        $props += "gizmo_snap_scale_percent={$($gizmoProfile.snap.scale_percent)}"
        $props += "gizmo_snap_default_enabled={" + $gizmoProfile.snap.default_enabled.ToString().ToLowerInvariant() + "}"
    }

    return ($props -join " ")
}

function Resolve-LatestFabricStatus {
    param($Snapshot)

    $candidateValues = @(
        [string]$Snapshot.latest_fabric_status,
        [string]$Snapshot.derived.latest_fabric_status,
        [string]$Snapshot.jobs.latest_fabric_status,
        [string]$Snapshot.session.derived.latest_fabric_status,
        [string]$Snapshot.session.jobs.latest_fabric_status,
        [string]$Snapshot.dcc_suite_state.derived.latest_fabric_status,
        [string]$Snapshot.dcc_suite_state.session.jobs.latest_fabric_status,
        [string]$Snapshot.recent_sessions[0].status
    )

    foreach ($candidateValue in $candidateValues) {
        if (-not [string]::IsNullOrWhiteSpace($candidateValue)) {
            return $candidateValue
        }
    }

    return "idle"
}

function New-ShellMetrics {
    param(
        $Snapshot,
        [System.Collections.IEnumerable]$Modes,
        [System.Collections.IEnumerable]$Surfaces,
        [System.Collections.IEnumerable]$Commands,
        [System.Collections.IEnumerable]$Pipeline,
        [System.Collections.IEnumerable]$Intents,
        [System.Collections.IEnumerable]$Reports,
        [System.Collections.IEnumerable]$Jobs,
        [System.Collections.IEnumerable]$RuntimePacks
    )

    $runtimePackCount = if ($null -ne $Snapshot -and $null -ne $Snapshot.runtime_packs) {
        @($Snapshot.runtime_packs).Count
    } else {
        @($RuntimePacks).Count
    }

    $extensionSeamCount = if ($null -ne $Snapshot -and $null -ne $Snapshot.extension_seams) {
        @($Snapshot.extension_seams).Count
    } else {
        0
    }

    return @{
        latest_fabric_status = Resolve-LatestFabricStatus -Snapshot $Snapshot
        workspace_mode_count = @($Modes).Count
        surface_count = @($Surfaces).Count
        runtime_pack_count = $runtimePackCount
        command_count = @($Commands).Count
        pipeline_step_count = @($Pipeline).Count
        intent_count = @($Intents).Count
        report_count = @($Reports).Count
        automation_job_count = @($Jobs).Count
        extension_seam_count = $extensionSeamCount
    }
}

function Get-ShellMetricValue {
    param(
        [string]$MetricSource,
        [hashtable]$ShellMetrics
    )

    if ([string]::IsNullOrWhiteSpace($MetricSource)) {
        return "n/a"
    }

    if (-not $ShellMetrics.ContainsKey($MetricSource)) {
        return "n/a"
    }

    $value = $ShellMetrics[$MetricSource]
    if ($MetricSource -eq "latest_fabric_status") {
        return ([string]$value).ToUpperInvariant()
    }

    return [string]$value
}

function Render-SurfaceWidget {
    param(
        $Surface,
        [string]$Indent
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $kind = [string]$Surface.kind

    if ($kind -eq "graph") {
        Add-Line $lines "$Indent<graph title=`"$($Surface.title)`" />"
        return $lines
    }

    if ($kind -eq "timeline") {
        Add-Line $lines "$Indent<timeline title=`"$($Surface.title)`" />"
        return $lines
    }

    if ($kind -eq "tree") {
        Add-Line $lines "$Indent<tree title=`"$($Surface.title)`">"
        Add-Line $lines (Render-TextNode -Role "caption" -Value $Surface.summary -Indent "$Indent    ")
        Add-Line $lines "$Indent</tree>"
        return $lines
    }

    Add-Line $lines "$Indent<inspector title=`"$($Surface.title)`">"
    Add-Line $lines (Render-TextNode -Role "caption" -Value $Surface.summary -Indent "$Indent    ")
    Add-Line $lines "$Indent</inspector>"
    return $lines
}

function Render-SurfaceCard {
    param(
        $Surface,
        [string]$Scope,
        [string]$Variant,
        [string]$Indent = ""
    )

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"$($Surface.title)`" scope=`"$Scope`" variant=`"$Variant`" layout=`"column`" gap={8}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value ([string]$Surface.kind).ToUpperInvariant() -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $Surface.summary -Indent "$Indent    ")
    Add-Lines $lines (Render-SurfaceWidget -Surface $Surface -Indent "$Indent    ")
    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-ChromeMetricCard {
    param(
        $StatusItem,
        [hashtable]$ShellMetrics,
        [string]$Scope,
        [string]$PreferredVariant = "",
        [string]$Indent = ""
    )

    $variant = if (-not [string]::IsNullOrWhiteSpace($PreferredVariant)) {
        $PreferredVariant
    } elseif ($StatusItem.metric_source -eq "latest_fabric_status") {
        "status_card"
    } else {
        "metric_pill"
    }
    $metricValue = Get-ShellMetricValue -MetricSource $StatusItem.metric_source -ShellMetrics $ShellMetrics

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"$($StatusItem.label)`" scope=`"$Scope`" variant=`"$variant`" layout=`"column`" gap={2} min_width={112}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $StatusItem.label.ToUpperInvariant() -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "metric" -Value $metricValue -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $StatusItem.caption -Indent "$Indent    ")
    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-MenuCard {
    param(
        $MenuItem,
        [string]$Scope,
        [string]$Indent = ""
    )

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"$($MenuItem.label)`" scope=`"$Scope`" variant=`"menu_pill`" layout=`"column`" gap={1} min_width={120}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value "MENU" -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $MenuItem.label -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $MenuItem.summary -Indent "$Indent    ")
    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-WorkspaceChip {
    param(
        $Page,
        $Mode,
        [bool]$IsActive,
        [string]$Scope,
        [string]$Indent = ""
    )

    $variant = if ($IsActive) { "workspace_chip_active" } else { "workspace_chip" }

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"$($Mode.label)`" scope=`"$Scope`" variant=`"$variant`" layout=`"column`" gap={2} min_width={156}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $Page.tab_label.ToUpperInvariant() -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $Mode.label -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $Mode.summary -Indent "$Indent    ")
    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-CommandSpotlightCard {
    param(
        $Command,
        [string]$Scope,
        [string]$Indent = ""
    )

    $surfaceLabel = if ([string]::IsNullOrWhiteSpace([string]$Command.surface)) { "shell" } else { $Command.surface }
    $intentLabel = if ([string]::IsNullOrWhiteSpace([string]$Command.intent)) { "no intent" } else { $Command.intent }

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"$($Command.label)`" scope=`"$Scope`" variant=`"spotlight_card`" layout=`"column`" gap={3} min_width={196}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value ("SURFACE " + $surfaceLabel.ToUpperInvariant()) -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $Command.label -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value ("intent " + $intentLabel) -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "body" -Value $Command.summary -Indent "$Indent    ")
    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-SystemRack {
    param(
        $SystemRack,
        [hashtable]$StatusItemById,
        [hashtable]$ShellMetrics,
        [string]$Scope,
        [string]$Indent = ""
    )

    $statusItems = Get-ResolvedItems -Ids $SystemRack.status_item_ids -Lookup $StatusItemById
    $statusColumns = [Math]::Max(1, [Math]::Min(2, @($statusItems).Count))

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"$($SystemRack.title)`" scope=`"$Scope`" variant=`"system_rack`" layout=`"column`" gap={8}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $SystemRack.eyebrow -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $SystemRack.title -Indent "$Indent    ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $SystemRack.summary -Indent "$Indent    ")
    Add-Line $lines "$Indent    <panel title=`"System Metrics`" scope=`"$Scope`" variant=`"system_rack`" layout=`"grid`" columns={$statusColumns} gap={8}>"
    foreach ($statusItem in $statusItems) {
        Add-Lines $lines (Render-ChromeMetricCard -StatusItem $statusItem -ShellMetrics $ShellMetrics -Scope $Scope -PreferredVariant "metric_pill" -Indent "$Indent        ")
    }
    Add-Line $lines "$Indent    </panel>"
    Add-Lines $lines (Render-TextLines -Items $SystemRack.notes -Formatter { param($note) $note } -Role "caption" -Indent "$Indent    ")
    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-ShellTopBar {
    param(
        $ShellChrome,
        [hashtable]$ShellMetrics,
        [string]$Scope,
        [string]$Indent = ""
    )

    $statusItems = @($ShellChrome.status_items)
    $menuItems = @($ShellChrome.menu_items)
    $statusColumns = [Math]::Max(1, [Math]::Min(4, $statusItems.Count))

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"Global Top Bar`" scope=`"$Scope`" variant=`"topbar`" layout=`"column`" gap={8}>"

    Add-Line $lines "$Indent    <panel title=`"Top Bar Frame`" scope=`"$Scope`" variant=`"topbar_frame`" layout=`"grid`" columns={2} gap={8}>"

    Add-Line $lines "$Indent        <panel title=`"Brand Console`" scope=`"$Scope`" variant=`"brand_console`" layout=`"column`" gap={3} min_width={340}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $ShellChrome.brand.eyebrow -Indent "$Indent            ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $ShellChrome.brand.title -Indent "$Indent            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $ShellChrome.brand.summary -Indent "$Indent            ")
    Add-Line $lines "$Indent        </panel>"

    Add-Line $lines "$Indent        <panel title=`"Status Rack`" scope=`"$Scope`" variant=`"status_rack`" layout=`"column`" gap={6}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value "RUNTIME STATUS" -Indent "$Indent            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value "Live session scale and pipeline health for the active shell." -Indent "$Indent            ")
    Add-Line $lines "$Indent            <panel title=`"Status Metrics`" scope=`"$Scope`" variant=`"status_rack`" layout=`"grid`" columns={$statusColumns} gap={8}>"
    foreach ($statusItem in $statusItems) {
        Add-Lines $lines (Render-ChromeMetricCard -StatusItem $statusItem -ShellMetrics $ShellMetrics -Scope $Scope -Indent "$Indent                ")
    }
    Add-Line $lines "$Indent            </panel>"
    Add-Line $lines "$Indent        </panel>"

    Add-Line $lines "$Indent    </panel>"

    Add-Line $lines "$Indent    <panel title=`"Menu Rack`" scope=`"$Scope`" variant=`"menu_rack`" layout=`"column`" gap={6}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value "SHELL MENUS" -Indent "$Indent        ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value "Global editor commands stay mounted in the same cockpit frame across every lane." -Indent "$Indent        ")
    Add-Line $lines "$Indent        <slot layout=`"row`" gap={6} overflow_x=`"scroll`">"
    foreach ($menuItem in $menuItems) {
        Add-Lines $lines (Render-MenuCard -MenuItem $menuItem -Scope $Scope -Indent "$Indent            ")
    }
    Add-Line $lines "$Indent        </slot>"
    Add-Line $lines "$Indent    </panel>"

    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-ShellContextStrip {
    param(
        $ShellChrome,
        [System.Collections.IEnumerable]$Pages,
        [hashtable]$ModeById,
        [hashtable]$CommandById,
        [hashtable]$StatusItemById,
        [hashtable]$ShellMetrics,
        [string]$Scope,
        [string]$Indent = ""
    )

    $spotlightCommands = Get-ResolvedItems -Ids $ShellChrome.command_spotlight.command_ids -Lookup $CommandById

    $lines = New-Object System.Collections.Generic.List[string]
    Add-Line $lines "$Indent<panel title=`"Shell Context Strip`" scope=`"$Scope`" variant=`"context_strip`" layout=`"column`" gap={10}>"

    Add-Line $lines "$Indent    <panel title=`"$($ShellChrome.workspace_switcher.title)`" scope=`"$Scope`" variant=`"workspace_strip`" layout=`"column`" gap={8}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $ShellChrome.workspace_switcher.eyebrow -Indent "$Indent        ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $ShellChrome.workspace_switcher.title -Indent "$Indent        ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $ShellChrome.workspace_switcher.summary -Indent "$Indent        ")
    Add-Line $lines "$Indent        <slot layout=`"row`" gap={8} overflow_x=`"scroll`">"

    $pageIndex = 0
    foreach ($page in $Pages) {
        if (-not $ModeById.ContainsKey($page.mode_id)) {
            continue
        }

        Add-Lines $lines (Render-WorkspaceChip `
            -Page $page `
            -Mode $ModeById[$page.mode_id] `
            -IsActive ($pageIndex -eq 0) `
            -Scope $Scope `
            -Indent "$Indent            ")

        $pageIndex = $pageIndex + 1
    }

    Add-Line $lines "$Indent        </slot>"
    Add-Line $lines "$Indent    </panel>"

    Add-Line $lines "$Indent    <panel title=`"Shell Context Blocks`" scope=`"$Scope`" variant=`"context_strip`" layout=`"grid`" columns={3} gap={10}>"

    Add-Line $lines "$Indent        <panel title=`"$($ShellChrome.command_spotlight.title)`" scope=`"$Scope`" variant=`"command_spotlight`" layout=`"column`" gap={8}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $ShellChrome.command_spotlight.eyebrow -Indent "$Indent            ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $ShellChrome.command_spotlight.title -Indent "$Indent            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $ShellChrome.command_spotlight.summary -Indent "$Indent            ")
    Add-Line $lines "$Indent            <slot layout=`"row`" gap={8} overflow_x=`"scroll`">"
    foreach ($command in $spotlightCommands) {
        Add-Lines $lines (Render-CommandSpotlightCard -Command $command -Scope $Scope -Indent "$Indent                ")
    }
    Add-Line $lines "$Indent            </slot>"
    Add-Line $lines "$Indent        </panel>"

    Add-Lines $lines (Render-SystemRack -SystemRack $ShellChrome.system_rack -StatusItemById $StatusItemById -ShellMetrics $ShellMetrics -Scope $Scope -Indent "$Indent        ")

    Add-Line $lines "$Indent        <panel title=`"$($ShellChrome.operator_notes.title)`" scope=`"$Scope`" variant=`"operator_console`" layout=`"column`" gap={6}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $ShellChrome.operator_notes.eyebrow -Indent "$Indent            ")
    Add-Line $lines (Render-TextNode -Role "title" -Value $ShellChrome.operator_notes.title -Indent "$Indent            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $ShellChrome.operator_notes.summary -Indent "$Indent            ")
    Add-Lines $lines (Render-TextLines -Items $ShellChrome.operator_notes.notes -Formatter { param($note) $note } -Role "caption" -Indent "$Indent            ")
    Add-Line $lines "$Indent        </panel>"

    Add-Line $lines "$Indent    </panel>"
    Add-Line $lines "$Indent</panel>"
    return $lines
}

function Render-WorkspacePage {
    param(
        $Page,
        $Mode,
        $Snapshot,
        [hashtable]$ToolById,
        [hashtable]$CommandById,
        [hashtable]$SurfaceById,
        [hashtable]$IntentById,
        [string]$PageTabGroupId,
        [string]$Scope,
        [int]$TabOrder,
        [bool]$IsDefaultActive,
        [string]$ViewportProps
    )

    $featuredTools = Get-ResolvedItems -Ids $Page.featured_tool_ids -Lookup $ToolById
    $quickCommands = Get-ResolvedItems -Ids $Page.quick_command_ids -Lookup $CommandById
    $centerSurfaces = Get-ResolvedItems -Ids $Page.center_surface_ids -Lookup $SurfaceById
    $rightSurfaces = Get-ResolvedItems -Ids $Page.right_surface_ids -Lookup $SurfaceById
    $bottomSurfaces = Get-ResolvedItems -Ids $Page.bottom_surface_ids -Lookup $SurfaceById
    $intents = Get-ResolvedItems -Ids $Page.intent_ids -Lookup $IntentById
    $focusSurface = if ($SurfaceById.ContainsKey($Page.focus_surface_id)) { $SurfaceById[$Page.focus_surface_id] } else { $null }
    $focusCaption = if ($null -eq $focusSurface) { "focus surface not authored" } else { "focus " + $focusSurface.title }
    $latestFabricStatus = Resolve-LatestFabricStatus -Snapshot $Snapshot
    $runtimePackCount = @($Snapshot.runtime_packs).Count
    $surfaceDeckColumns = [Math]::Min(3, [Math]::Max(1, @($centerSurfaces).Count))
    $telemetryDeckColumns = [Math]::Min(3, [Math]::Max(1, @($bottomSurfaces).Count))

    $lines = New-Object System.Collections.Generic.List[string]
    $defaultActiveLiteral = if ($IsDefaultActive) { " tab_default_active={true}" } else { "" }

    Add-Line $lines "            <panel title=`"$($Page.title)`" scope=`"$Scope`" variant=`"page`" layout=`"dock`" gap={14} persistent_layout_id=`"dcc_page_$($Page.mode_id)`" tab_group_id=`"$PageTabGroupId`" tab_label=`"$($Page.tab_label)`" tab_order={$TabOrder}$defaultActiveLiteral>"

    Add-Line $lines "                <panel title=`"Lane Console`" dock=`"left`" split_ratio={0.18} min_width={250} max_width={340} resizable={true} layout=`"column`" gap={10}>"
    Add-Line $lines "                    <panel title=`"Hero`" scope=`"$Scope`" variant=`"hero_card`" layout=`"column`" gap={6}>"
    Add-Line $lines (Render-TextNode -Role "eyebrow" -Value $Mode.label.ToUpperInvariant() -Indent "                        ")
    Add-Line $lines (Render-TextNode -Role "hero" -Value $Page.hero_title -Indent "                        ")
    Add-Line $lines (Render-TextNode -Role "body" -Value $Page.hero_summary -Indent "                        ")
    Add-Line $lines "                    </panel>"
    Add-Line $lines "                    <inspector title=`"Featured Tools`">"
    Add-Lines $lines (Render-TextLines -Items $featuredTools -Formatter { param($tool) "$($tool.label) | $($tool.summary)" } -Role "body" -Indent "                        ")
    Add-Line $lines "                    </inspector>"
    Add-Line $lines "                    <inspector title=`"Quick Commands`">"
    Add-Lines $lines (Render-TextLines -Items $quickCommands -Formatter { param($command) "$($command.label) | $($command.summary)" } -Role "body" -Indent "                        ")
    Add-Line $lines "                    </inspector>"
    if ($null -ne $focusSurface) {
        Add-Line $lines "                    <tree title=`"Focus Surface`">"
        Add-Line $lines (Render-TextNode -Role "caption" -Value $focusSurface.title -Indent "                        ")
        Add-Line $lines (Render-TextNode -Role "caption" -Value $focusSurface.summary -Indent "                        ")
        Add-Line $lines "                    </tree>"
    }
    Add-Line $lines "                </panel>"

    Add-Line $lines "                <panel title=`"Workbench Stage`" dock=`"center`" layout=`"column`" gap={12}>"
    Add-Line $lines "                    <panel title=`"Viewport Frame`" scope=`"$Scope`" variant=`"viewport_frame`" layout=`"column`" gap={10}>"
    Add-Line $lines "                        <panel title=`"Cockpit Strip`" layout=`"grid`" columns={4} gap={8}>"
    Add-Line $lines "                            <panel title=`"Mode`" scope=`"$Scope`" variant=`"quiet_card`" layout=`"column`" gap={2}>"
    Add-Line $lines (Render-TextNode -Role "metric" -Value $Mode.label -Indent "                            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value $focusCaption -Indent "                            ")
    Add-Line $lines "                            </panel>"
    Add-Line $lines "                            <panel title=`"Fabric`" scope=`"$Scope`" variant=`"status_card`" layout=`"column`" gap={2}>"
    Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]$latestFabricStatus).ToUpperInvariant() -Indent "                            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value ("intent routes " + @($intents).Count) -Indent "                            ")
    Add-Line $lines "                            </panel>"
    Add-Line $lines "                            <panel title=`"Scale`" scope=`"$Scope`" variant=`"quiet_card`" layout=`"column`" gap={2}>"
    Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]$runtimePackCount) -Indent "                            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value "runtime packs online" -Indent "                            ")
    Add-Line $lines "                            </panel>"
    Add-Line $lines "                            <panel title=`"Commands`" scope=`"$Scope`" variant=`"quiet_card`" layout=`"column`" gap={2}>"
    Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]@($quickCommands).Count) -Indent "                            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value "quick actions armed" -Indent "                            ")
    Add-Line $lines "                            </panel>"
    Add-Line $lines "                        </panel>"
    Add-Line $lines "                        <viewport3d $ViewportProps />"
    Add-Line $lines "                    </panel>"
    Add-Line $lines "                    <panel title=`"Surface Deck`" layout=`"grid`" columns={$surfaceDeckColumns} gap={10}>"
    foreach ($surface in $centerSurfaces) {
        Add-Lines $lines (Render-SurfaceCard -Surface $surface -Scope $Scope -Variant "surface_card" -Indent "                        ")
    }
    Add-Line $lines "                    </panel>"
    Add-Line $lines "                </panel>"

    Add-Line $lines "                <panel title=`"Inspector Rail`" dock=`"right`" split_ratio={0.22} min_width={300} max_width={420} resizable={true} layout=`"column`" gap={10}>"
    Add-Line $lines "                    <inspector title=`"Intent Routes`">"
    Add-Lines $lines (Render-TextLines -Items $intents -Formatter { param($intent) "$($intent.label) | $($intent.graph)" } -Role "body" -Indent "                        ")
    Add-Line $lines "                    </inspector>"
    foreach ($surface in $rightSurfaces) {
        Add-Lines $lines (Render-SurfaceCard -Surface $surface -Scope $Scope -Variant "surface_card" -Indent "                    ")
    }
    Add-Line $lines "                </panel>"

    Add-Line $lines "                <panel title=`"Telemetry Tray`" dock=`"bottom`" split_ratio={0.2} min_height={180} max_height={320} resizable={true} layout=`"column`" gap={10}>"
    Add-Line $lines "                    <panel title=`"Execution Surfaces`" layout=`"grid`" columns={$telemetryDeckColumns} gap={10}>"
    foreach ($surface in $bottomSurfaces) {
        Add-Lines $lines (Render-SurfaceCard -Surface $surface -Scope $Scope -Variant "quiet_card" -Indent "                        ")
    }
    Add-Line $lines "                    </panel>"
    Add-Line $lines "                    <inspector title=`"Extension Seams`">"
    Add-Lines $lines (Render-TextLines -Items $Snapshot.extension_seams -Formatter { param($item) $item } -Role "caption" -Indent "                        ")
    Add-Line $lines "                    </inspector>"
    Add-Line $lines "                </panel>"

    Add-Line $lines "            </panel>"
    return $lines
}

$AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
$AppRoot = (Resolve-Path $AppRoot).Path

$Manifest = Get-Content (Join-Path $AppRoot "config/app_manifest.json") -Raw | ConvertFrom-Json
$Modes = (Get-Content (Join-Path $AppRoot "config/workspace_modes.json") -Raw | ConvertFrom-Json).modes
$Surfaces = (Get-Content (Join-Path $AppRoot "config/surfaces.json") -Raw | ConvertFrom-Json).surfaces
$Tools = (Get-Content (Join-Path $AppRoot "config/tool_catalog.json") -Raw | ConvertFrom-Json).tools
$GizmoRegistry = Get-Content (Join-Path $AppRoot "config/gizmo_registry.json") -Raw | ConvertFrom-Json
$Commands = (Get-Content (Join-Path $AppRoot "config/command_registry.json") -Raw | ConvertFrom-Json).commands
$Pipeline = (Get-Content (Join-Path $AppRoot "config/fabric_pipeline.json") -Raw | ConvertFrom-Json).steps
$Intents = (Get-Content (Join-Path $AppRoot "config/fabric_intents.json") -Raw | ConvertFrom-Json).intents
$Resources = (Get-Content (Join-Path $AppRoot "config/resource_kinds.json") -Raw | ConvertFrom-Json).resource_kinds
$Reports = (Get-Content (Join-Path $AppRoot "config/report_kinds.json") -Raw | ConvertFrom-Json).report_kinds
$RuntimePacks = (Get-Content (Join-Path $AppRoot "config/runtime_packs.json") -Raw | ConvertFrom-Json).runtime_packs
$Jobs = (Get-Content (Join-Path $AppRoot "config/automation_jobs.json") -Raw | ConvertFrom-Json).jobs
$Theme = Get-Content (Join-Path $AppRoot "config/ui_theme.json") -Raw | ConvertFrom-Json
$UiShell = Get-Content (Join-Path $AppRoot "config/ui_shell.json") -Raw | ConvertFrom-Json
$ShellChrome = $UiShell.shell_chrome
$SnapshotPath = Join-Path $AppRoot "state/runtime_snapshot.json"

$Snapshot = if (Test-Path $SnapshotPath) {
    Get-Content $SnapshotPath -Raw | ConvertFrom-Json
} else {
    [pscustomobject]@{
        latest_fabric_status = "idle"
        runtime_packs = $RuntimePacks
        extension_seams = @(
            "runtime snapshot not materialized yet",
            "run materialize-session-state.ps1 after a Fabric pass to project live status"
        )
    }
}

$ModeById = New-LookupTable -Items $Modes -KeyProperty "id"
$SurfaceById = New-LookupTable -Items $Surfaces -KeyProperty "id"
$ToolById = New-LookupTable -Items $Tools -KeyProperty "id"
$CommandById = New-LookupTable -Items $Commands -KeyProperty "id"
$IntentById = New-LookupTable -Items $Intents -KeyProperty "id"
$StatusItemById = New-LookupTable -Items $ShellChrome.status_items -KeyProperty "id"
$ViewportProps = Get-ViewportProps -Surfaces $Surfaces -GizmoRegistry $GizmoRegistry -FallbackScene $UiShell.viewport_scene
$ShellMetrics = New-ShellMetrics `
    -Snapshot $Snapshot `
    -Modes $Modes `
    -Surfaces $Surfaces `
    -Commands $Commands `
    -Pipeline $Pipeline `
    -Intents $Intents `
    -Reports $Reports `
    -Jobs $Jobs `
    -RuntimePacks $RuntimePacks

$lines = New-Object System.Collections.Generic.List[string]
Add-Line $lines "component App():"
Add-Line $lines "    render <slot layout=`"column`" gap={10} padding={10} overflow_y=`"scroll`">"
Add-Lines $lines (Render-ThemeBlock -Theme $Theme -Indent "        ")
Add-Line $lines "        <panel title=`"$($Manifest.window_title)`" scope=`"dcc_shell`" variant=`"shell_root`" layout=`"column`" gap={10}>"
Add-Lines $lines (Render-ShellTopBar -ShellChrome $ShellChrome -ShellMetrics $ShellMetrics -Scope "dcc_shell" -Indent "            ")
Add-Lines $lines (Render-ShellContextStrip -ShellChrome $ShellChrome -Pages $UiShell.workspace_pages -ModeById $ModeById -CommandById $CommandById -StatusItemById $StatusItemById -ShellMetrics $ShellMetrics -Scope "dcc_shell" -Indent "            ")
Add-Line $lines "            <panel title=`"Workbench Pages`" scope=`"dcc_shell`" variant=`"page`" layout=`"column`" gap={12}>"

$pageIndex = 0
foreach ($page in $UiShell.workspace_pages) {
    if (-not $ModeById.ContainsKey($page.mode_id)) {
        continue
    }

    Add-Lines $lines (Render-WorkspacePage `
        -Page $page `
        -Mode $ModeById[$page.mode_id] `
        -Snapshot $Snapshot `
        -ToolById $ToolById `
        -CommandById $CommandById `
        -SurfaceById $SurfaceById `
        -IntentById $IntentById `
        -PageTabGroupId $UiShell.page_tab_group_id `
        -Scope "dcc_shell" `
        -TabOrder $pageIndex `
        -IsDefaultActive ($pageIndex -eq 0) `
        -ViewportProps $ViewportProps)

    $pageIndex = $pageIndex + 1
}

Add-Line $lines "            </panel>"
Add-Line $lines "            <panel title=`"Global Registries`" scope=`"dcc_shell`" variant=`"page`" layout=`"grid`" columns={4} gap={12}>"
Add-Line $lines "                <inspector title=`"Pipeline`">"
Add-Lines $lines (Render-TextLines -Items $Pipeline -Formatter { param($step) "$($step.id) | $($step.runtime)" } -Role "caption" -Indent "                    ")
Add-Line $lines "                </inspector>"
Add-Line $lines "                <inspector title=`"Resources`">"
Add-Lines $lines (Render-TextLines -Items $Resources -Formatter { param($resource) "$($resource.resource_uri) | $($resource.kind)" } -Role "caption" -Indent "                    ")
Add-Line $lines "                </inspector>"
Add-Line $lines "                <inspector title=`"Reports`">"
Add-Lines $lines (Render-TextLines -Items $Reports -Formatter { param($report) "$($report.report_uri) | $($report.summary)" } -Role "caption" -Indent "                    ")
Add-Line $lines "                </inspector>"
Add-Line $lines "                <inspector title=`"Automation`">"
Add-Lines $lines (Render-TextLines -Items $Jobs -Formatter { param($job) "$($job.label) | $($job.schedule)" } -Role "caption" -Indent "                    ")
Add-Line $lines "                </inspector>"
Add-Line $lines "            </panel>"
Add-Line $lines "        </panel>"
Add-Line $lines "    </slot>"

$OutputPath = Join-Path $AppRoot "generated/main.generated.kn"
Set-Content -Path $OutputPath -Value (Join-Lines $lines) -NoNewline
Write-Host "Materialized $OutputPath"
