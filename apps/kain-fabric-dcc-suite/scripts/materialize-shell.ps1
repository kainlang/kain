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

    $lines = New-Object System.Collections.Generic.List[string]
    $defaultActiveLiteral = if ($IsDefaultActive) { " tab_default_active={true}" } else { "" }

    Add-Line $lines "            <panel title=`"$($Page.title)`" scope=`"$Scope`" variant=`"page`" layout=`"dock`" gap={14} persistent_layout_id=`"dcc_page_$($Page.mode_id)`" tab_group_id=`"$PageTabGroupId`" tab_label=`"$($Page.tab_label)`" tab_order={$TabOrder}$defaultActiveLiteral>"

    Add-Line $lines "                <panel title=`"Navigator`" dock=`"left`" split_ratio={0.22} min_width={270} max_width={380} resizable={true} layout=`"column`" gap={12}>"
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
    Add-Line $lines "                    <panel title=`"Status Deck`" layout=`"grid`" columns={3} gap={10}>"
    Add-Line $lines "                        <panel title=`"Mode`" scope=`"$Scope`" variant=`"quiet_card`" layout=`"column`" gap={2}>"
    Add-Line $lines (Render-TextNode -Role "metric" -Value $Mode.label -Indent "                            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value ("focus " + $focusSurface.title) -Indent "                            ")
    Add-Line $lines "                        </panel>"
    Add-Line $lines "                        <panel title=`"Fabric`" scope=`"$Scope`" variant=`"status_card`" layout=`"column`" gap={2}>"
    Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]$Snapshot.latest_fabric_status).ToUpperInvariant() -Indent "                            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value ("intent routes " + @($intents).Count) -Indent "                            ")
    Add-Line $lines "                        </panel>"
    Add-Line $lines "                        <panel title=`"Scale`" scope=`"$Scope`" variant=`"quiet_card`" layout=`"column`" gap={2}>"
    Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]@($Snapshot.runtime_packs).Count) -Indent "                            ")
    Add-Line $lines (Render-TextNode -Role "caption" -Value "runtime packs online" -Indent "                            ")
    Add-Line $lines "                        </panel>"
    Add-Line $lines "                    </panel>"
    Add-Line $lines "                    <viewport3d $ViewportProps />"
    Add-Line $lines "                    <panel title=`"Surface Deck`" layout=`"grid`" columns={2} gap={12}>"
    foreach ($surface in $centerSurfaces) {
        Add-Lines $lines (Render-SurfaceCard -Surface $surface -Scope $Scope -Variant "surface_card" -Indent "                        ")
    }
    Add-Line $lines "                    </panel>"
    Add-Line $lines "                </panel>"

    Add-Line $lines "                <panel title=`"Inspector Rail`" dock=`"right`" split_ratio={0.25} min_width={320} max_width={460} resizable={true} layout=`"column`" gap={12}>"
    Add-Line $lines "                    <inspector title=`"Intent Routes`">"
    Add-Lines $lines (Render-TextLines -Items $intents -Formatter { param($intent) "$($intent.label) | $($intent.graph)" } -Role "body" -Indent "                        ")
    Add-Line $lines "                    </inspector>"
    foreach ($surface in $rightSurfaces) {
        Add-Lines $lines (Render-SurfaceCard -Surface $surface -Scope $Scope -Variant "surface_card" -Indent "                    ")
    }
    Add-Line $lines "                </panel>"

    Add-Line $lines "                <panel title=`"Telemetry Tray`" dock=`"bottom`" split_ratio={0.24} min_height={220} max_height={360} resizable={true} layout=`"column`" gap={12}>"
    Add-Line $lines "                    <panel title=`"Execution Surfaces`" layout=`"grid`" columns={2} gap={10}>"
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
$ViewportProps = Get-ViewportProps -Surfaces $Surfaces -GizmoRegistry $GizmoRegistry -FallbackScene $UiShell.viewport_scene

$lines = New-Object System.Collections.Generic.List[string]
Add-Line $lines "component App():"
Add-Line $lines "    render <slot>"
Add-Lines $lines (Render-ThemeBlock -Theme $Theme -Indent "        ")
Add-Line $lines "        <panel title=`"$($Manifest.window_title)`" scope=`"dcc_shell`" variant=`"shell_root`" layout=`"column`" gap={12} padding={12}>"
Add-Line $lines "            <panel title=`"Operator Masthead`" scope=`"dcc_shell`" variant=`"masthead`" layout=`"row`" gap={12}>"
Add-Line $lines "                <panel title=`"Identity`" scope=`"dcc_shell`" variant=`"hero_card`" layout=`"column`" gap={4}>"
Add-Line $lines (Render-TextNode -Role "eyebrow" -Value "UNIVERSAL STUDIO UI" -Indent "                    ")
Add-Line $lines (Render-TextNode -Role "hero" -Value "Workspace-aware Kain shell with page-level docks, stage decks, and authored operator rails." -Indent "                    ")
Add-Line $lines (Render-TextNode -Role "body" -Value "This shell is generated from manifest-owned UI descriptors so the suite can evolve like a real editor framework instead of a static demo layout." -Indent "                    ")
Add-Line $lines "                </panel>"
Add-Line $lines "                <panel title=`"Runtime`" scope=`"dcc_shell`" variant=`"status_card`" layout=`"column`" gap={2}>"
Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]$Snapshot.latest_fabric_status).ToUpperInvariant() -Indent "                    ")
Add-Line $lines (Render-TextNode -Role "caption" -Value ("pipeline steps " + @($Pipeline).Count) -Indent "                    ")
Add-Line $lines "                </panel>"
Add-Line $lines "                <panel title=`"Scale`" scope=`"dcc_shell`" variant=`"quiet_card`" layout=`"column`" gap={2}>"
Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]@($RuntimePacks).Count) -Indent "                    ")
Add-Line $lines (Render-TextNode -Role "caption" -Value "runtime packs" -Indent "                    ")
Add-Line $lines "                </panel>"
Add-Line $lines "                <panel title=`"Registry`" scope=`"dcc_shell`" variant=`"quiet_card`" layout=`"column`" gap={2}>"
Add-Line $lines (Render-TextNode -Role "metric" -Value ([string]@($Surfaces).Count) -Indent "                    ")
Add-Line $lines (Render-TextNode -Role "caption" -Value "surface contracts" -Indent "                    ")
Add-Line $lines "                </panel>"
Add-Line $lines "            </panel>"
Add-Line $lines "            <panel title=`"Workspace Atlas`" scope=`"dcc_shell`" variant=`"page`" layout=`"column`" gap={12}>"

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
