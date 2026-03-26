[CmdletBinding()]
param(
    [string]$TemplateRoot = "M:\Templates\3D"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-JsonFile {
    param([string]$Path)
    Get-Content -Raw $Path | ConvertFrom-Json
}

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Data
    )

    $parent = Split-Path -Parent $Path
    if (!(Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }

    $json = $Data | ConvertTo-Json -Depth 100
    Set-Content -Path $Path -Value $json
}

function New-IndexMap {
    param(
        [object[]]$Entries,
        [scriptblock]$KeySelector,
        [scriptblock]$ValueSelector
    )

    $index = [ordered]@{}
    foreach ($entry in $Entries) {
        $rawKeys = & $KeySelector $entry
        $keys = @()
        if ($null -eq $rawKeys) {
            $keys = @()
        } elseif ($rawKeys -is [System.Array]) {
            $keys = @($rawKeys)
        } else {
            $keys = @($rawKeys)
        }

        $value = & $ValueSelector $entry
        if ([string]::IsNullOrWhiteSpace([string]$value)) {
            continue
        }

        foreach ($rawKey in $keys) {
            $key = [string]$rawKey
            if ([string]::IsNullOrWhiteSpace($key)) {
                continue
            }
            if (!$index.Contains($key)) {
                $index[$key] = @()
            }

            $existing = @($index[$key])
            $stringValue = [string]$value
            if ($existing -notcontains $stringValue) {
                $index[$key] = @($existing + $stringValue)
            }
        }
    }

    $orderedIndex = [ordered]@{}
    foreach ($key in ($index.Keys | Sort-Object)) {
        $orderedIndex[$key] = @($index[$key] | Sort-Object)
    }
    return $orderedIndex
}

function Flatten-UniqueValues {
    param(
        [object[]]$Entries,
        [scriptblock]$CollectionSelector
    )

    $values = New-Object System.Collections.Generic.HashSet[string]
    foreach ($entry in $Entries) {
        $collection = & $CollectionSelector $entry
        if ($null -eq $collection) {
            continue
        }

        foreach ($item in $collection) {
            if (![string]::IsNullOrWhiteSpace([string]$item)) {
                [void]$values.Add([string]$item)
            }
        }
    }

    return @($values | Sort-Object)
}

function Get-OptionalValue {
    param(
        [object]$Object,
        [string]$PropertyName
    )

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Get-RegexCapture {
    param(
        [string]$Text,
        [string]$Pattern
    )

    $match = [regex]::Match($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if (!$match.Success) {
        return $null
    }

    return $match.Groups[1].Value
}

function Normalize-Array {
    param([object]$Value)

    if ($null -eq $Value) {
        return @()
    }

    if ($Value -is [System.Array]) {
        if ($Value.Count -eq 1 -and $Value[0] -is [System.Array]) {
            return @($Value[0])
        }
        return @($Value)
    }

    return @($Value)
}

try {
$templateRoot = (Resolve-Path $TemplateRoot).Path
$manifestsRoot = Join-Path $templateRoot "manifests"
$generatedRoot = Join-Path $templateRoot "generated"
$runtimeReflectionRoot = Join-Path $generatedRoot "runtime-reflection"

$runtimeApps = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "runtime_apps.json"))
$workspacePresets = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "workspace_presets.json"))
$sources = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "sources.json"))
$buildGraphs = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "build_graphs.json"))
$distributionChannels = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "distribution_channels.json"))
$tensorPipelines = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "tensor_pipelines.json"))
$gpuKernels = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "gpu_kernels.json"))
$runtimeCompatibilityRuntimeSource = Get-Content -Raw -Path (Join-Path $templateRoot "src-kain\stdlib\three_d_runtime\runtime_compatibility_runtime.kn")
$resourceReflectionRuntimeSource = Get-Content -Raw -Path (Join-Path $templateRoot "src-kain\stdlib\three_d_runtime\resource_reflection_runtime.kn")
$resourceRuntimeSource = Get-Content -Raw -Path (Join-Path $templateRoot "src-kain\stdlib\three_d_runtime\resource_runtime.kn")
$runtimeContractsCatalog = Read-JsonFile -Path (Join-Path $runtimeReflectionRoot "contracts\catalog.json")
$gpuReflectionCatalog = Read-JsonFile -Path (Join-Path $runtimeReflectionRoot "gpu\catalog.json")

$launchManifestFiles = @(Get-ChildItem -Path (Join-Path $generatedRoot "workspace-presets\launch") -File -Filter "*.json" | Sort-Object Name)
$receiptFiles = @(Get-ChildItem -Path (Join-Path $generatedRoot "workspace-presets\receipts") -File -Filter "*.json" | Sort-Object Name)
$launchManifests = @($launchManifestFiles | ForEach-Object { Read-JsonFile -Path $_.FullName })
$receiptsWithPaths = @(
    $receiptFiles | ForEach-Object {
        [ordered]@{
            data = Read-JsonFile -Path $_.FullName
            path = "generated/workspace-presets/receipts/$($_.Name)"
        }
    }
)

$runtimeAppsById = @{}
foreach ($runtimeApp in $runtimeApps) {
    $runtimeAppsById[$runtimeApp.id] = $runtimeApp
}

$workspacePresetsById = @{}
foreach ($preset in $workspacePresets) {
    $workspacePresetsById[$preset.id] = $preset
}

$sourceIdsByPath = @{}
foreach ($source in $sources) {
    $sourcePath = Get-OptionalValue -Object $source -PropertyName "source_path"
    $sourceId = Get-OptionalValue -Object $source -PropertyName "id"
    if (![string]::IsNullOrWhiteSpace([string]$sourcePath) -and -not [string]::IsNullOrWhiteSpace([string]$sourceId)) {
        $sourceIdsByPath[[string]$sourcePath] = [string]$sourceId
    }
}

function Resolve-SourceId {
    param(
        [object]$Object,
        [hashtable]$SourceIdsByPath
    )

    $sourceId = Get-OptionalValue -Object $Object -PropertyName "source_id"
    if (-not [string]::IsNullOrWhiteSpace([string]$sourceId)) {
        return [string]$sourceId
    }

    $sourcePath = Get-OptionalValue -Object $Object -PropertyName "source_path"
    if (![string]::IsNullOrWhiteSpace([string]$sourcePath) -and $SourceIdsByPath.ContainsKey([string]$sourcePath)) {
        return [string]$SourceIdsByPath[[string]$sourcePath]
    }

    return $null
}

$launchManifestsByPresetId = @{}
foreach ($launch in $launchManifests) {
    $launchManifestsByPresetId[$launch.preset_id] = $launch
}

$receiptsByPresetId = @{}
foreach ($receiptEntry in $receiptsWithPaths) {
    $receipt = $receiptEntry.data
    $receiptsByPresetId[$receipt.preset_id] = $receiptEntry
}

$launchProfileEntries = New-Object System.Collections.Generic.List[object]
foreach ($presetId in ($launchManifestsByPresetId.Keys | Sort-Object)) {
    $launch = $launchManifestsByPresetId[$presetId]
    $preset = $workspacePresetsById[$presetId]
    $runtimeApp = $runtimeAppsById[$launch.runtime_app_id]
    $receiptEntry = $receiptsByPresetId[$presetId]
    $receipt = if ($null -ne $receiptEntry) { $receiptEntry.data } else { $null }

    $launchProfileEntries.Add([ordered]@{
        manifest_id = $launch.manifest_id
        preset_id = $launch.preset_id
        preset_kind = Get-OptionalValue -Object $preset -PropertyName "preset_kind"
        focus_lane = $launch.focus_lane
        runtime_app_id = $launch.runtime_app_id
        runtime_app_source_id = Resolve-SourceId -Object $runtimeApp -SourceIdsByPath $sourceIdsByPath
        runtime_app_namespace = Get-OptionalValue -Object $runtimeApp -PropertyName "namespace"
        runtime_kind = Get-OptionalValue -Object $runtimeApp -PropertyName "runtime_kind"
        runtime_bundle_profile_id = $launch.runtime_bundle_profile_id
        host_kind = if ($null -ne $runtimeApp) { Get-OptionalValue -Object $runtimeApp -PropertyName "host_kind" } else { Get-OptionalValue -Object $launch -PropertyName "host_kind" }
        launch_manifest_path = $launch.artifacts[0].path
        launch_schema_id = $launch.schema_id
        launch_schema_path = (($launch.artifacts | Where-Object { $_.kind -eq "launch_schema_bundle" } | Select-Object -First 1).path)
        receipt_id = if ($null -ne $receipt) { $receipt.receipt_id } else { $null }
        receipt_path = if ($null -ne $receiptEntry) { $receiptEntry.path } else { $null }
        receipt_promotion_state = if ($null -ne $receipt) { $receipt.promotion_state } else { $null }
        delivery_registry_id = if ($null -ne $receipt) { $receipt.distribution_channel_id } else { $launch.delivery_registry_id }
    })
}

$buildGraphEntries = New-Object System.Collections.Generic.List[object]
foreach ($graph in $buildGraphs) {
    $matchingChannels = @(
        $distributionChannels |
            Where-Object {
                $graphRoots = @($graph.outputs)
                $channelRoots = @($_.artifact_roots)
                @($graphRoots | Where-Object { $channelRoots -contains $_ }).Count -gt 0
            } |
            Sort-Object id
    )

    $buildGraphEntries.Add([ordered]@{
        id = $graph.id
        label = $graph.label
        graph_kind = $graph.graph_kind
        queue = $graph.queue
        inputs = @($graph.inputs)
        outputs = @($graph.outputs)
        linked_distribution_channels = @($matchingChannels | ForEach-Object { $_.id })
    })
}

$distributionEntries = New-Object System.Collections.Generic.List[object]
foreach ($channel in $distributionChannels) {
    $matchingGraphs = @(
        $buildGraphs |
            Where-Object {
                $graphRoots = @($_.outputs)
                $channelRoots = @($channel.artifact_roots)
                @($graphRoots | Where-Object { $channelRoots -contains $_ }).Count -gt 0
            } |
            Sort-Object id
    )

    $distributionEntries.Add([ordered]@{
        id = $channel.id
        label = $channel.label
        channel_kind = $channel.channel_kind
        approval_policy = $channel.approval_policy
        artifact_roots = @($channel.artifact_roots)
        linked_build_graphs = @($matchingGraphs | ForEach-Object { $_.id })
    })
}

$launchCatalog = [ordered]@{}
$launchCatalog["catalog_id"] = "launch_profile_catalog"
$launchCatalog["catalog_scope"] = "runtime_bundle_launch_profile_and_workspace_preset_binding_metadata"
$launchCatalog["tensor_pipeline_id"] = "runtime_reflection_tensor_pipeline"
$launchCatalog["profile_id"] = "runtime_bundle_launch_profile"
$launchCatalog["manifest_source"] = "manifests/workspace_presets.json"
$launchCatalog["runtime_app_manifest"] = "manifests/runtime_apps.json"
$launchCatalog["entry_count"] = $launchProfileEntries.Count
$launchCatalog["entries"] = $launchProfileEntries.ToArray()
$launchCatalog["indexes"] = [ordered]@{}
$launchCatalog["artifact_roots"] = @(
    "generated/workspace-presets/launch",
    "generated/workspace-presets/receipts",
    "generated/runtime-reflection/launch-profiles"
)
$launchCatalog["reflection_catalogs"] = @(
    "workspace_preset_catalog",
    "workspace_preset_receipt_catalog",
    "distribution_receipt_catalog"
)
Write-Host "Building launch indexes"
$launchCatalog["indexes"]["by_focus_lane"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.focus_lane } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_runtime_app"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.runtime_app_id } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_runtime_app_source_id"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.runtime_app_source_id } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_host_kind"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.host_kind } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_delivery_registry"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.delivery_registry_id } -ValueSelector { param($entry) $entry.preset_id }

$buildGraphCatalog = [ordered]@{}
$buildGraphCatalog["catalog_id"] = "build_graph_catalog"
$buildGraphCatalog["catalog_scope"] = "build_graph_queue_output_and_promotion_metadata"
$buildGraphCatalog["tensor_pipeline_id"] = "runtime_reflection_tensor_pipeline"
$buildGraphCatalog["manifest_source"] = "manifests/build_graphs.json"
$buildGraphCatalog["graph_count"] = $buildGraphEntries.Count
$buildGraphCatalog["entries"] = $buildGraphEntries.ToArray()
$buildGraphCatalog["indexes"] = [ordered]@{}
$buildGraphCatalog["output_roots"] = Flatten-UniqueValues -Entries $buildGraphEntries -CollectionSelector { param($entry) @($entry.outputs) }
$buildGraphCatalog["artifact_roots"] = @("generated/runtime-reflection/build-graphs")
$buildGraphCatalog["reflection_catalogs"] = @(
    "distribution_receipt_catalog",
    "runtime_contract_catalog"
)
Write-Host "Building build-graph indexes"
$buildGraphCatalog["indexes"]["by_queue"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) $entry.queue } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_graph_kind"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) $entry.graph_kind } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_input_manifest"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) @($entry.inputs) } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_output_root"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) @($entry.outputs) } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_distribution_channel"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) @($entry.linked_distribution_channels) } -ValueSelector { param($entry) $entry.id }

$distributionCatalog = [ordered]@{}
$distributionCatalog["catalog_id"] = "distribution_receipt_catalog"
$distributionCatalog["catalog_scope"] = "distribution_channel_delivery_and_receipt_metadata"
$distributionCatalog["tensor_pipeline_id"] = "runtime_reflection_tensor_pipeline"
$distributionCatalog["manifest_source"] = "manifests/distribution_channels.json"
$distributionCatalog["channel_count"] = $distributionEntries.Count
$distributionCatalog["entries"] = $distributionEntries.ToArray()
$distributionCatalog["indexes"] = [ordered]@{}
$distributionCatalog["artifact_roots"] = @("generated/runtime-reflection/distribution")
$distributionCatalog["reflection_catalogs"] = @(
    "build_graph_catalog",
    "launch_profile_catalog"
)
Write-Host "Building distribution indexes"
$distributionCatalog["indexes"]["by_channel_kind"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) $entry.channel_kind } -ValueSelector { param($entry) $entry.id }
$distributionCatalog["indexes"]["by_approval_policy"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) $entry.approval_policy } -ValueSelector { param($entry) $entry.id }
$distributionCatalog["indexes"]["by_artifact_root"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) @($entry.artifact_roots) } -ValueSelector { param($entry) $entry.id }
$distributionCatalog["indexes"]["by_build_graph"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) @($entry.linked_build_graphs) } -ValueSelector { param($entry) $entry.id }

$runtimeCompatibilityMatrixId = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.compatibility_matrix\.matrix_id\s*=\s*"([^"]+)"'
$runtimeCompatibilityMatrixScope = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.compatibility_matrix\.matrix_scope\s*=\s*"([^"]+)"'
$runtimeCompatibilityValidationPolicy = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.compatibility_matrix\.validation_policy\s*=\s*"([^"]+)"'
$runtimeCompatibilityWindowId = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.compatibility_window\.window_id\s*=\s*"([^"]+)"'
$runtimeCompatibilityWindowScope = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.compatibility_window\.window_scope\s*=\s*"([^"]+)"'
$runtimeCompatibilityPromotionPolicy = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.compatibility_window\.promotion_policy\s*=\s*"([^"]+)"'
$runtimeCompatibilityReadinessId = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.launch_readiness\.readiness_id\s*=\s*"([^"]+)"'
$runtimeCompatibilityReadinessScope = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.launch_readiness\.readiness_scope\s*=\s*"([^"]+)"'
$runtimeCompatibilityGatePolicy = Get-RegexCapture -Text $runtimeCompatibilityRuntimeSource -Pattern 'profile\.launch_readiness\.gate_policy\s*=\s*"([^"]+)"'

$runtimeCompatibilityRowsByKey = @{}
foreach ($runtimeApp in @($runtimeApps | Sort-Object id)) {
    $runtimeKind = Get-OptionalValue -Object $runtimeApp -PropertyName "runtime_kind"
    $hostKind = Get-OptionalValue -Object $runtimeApp -PropertyName "host_kind"
    $rowKey = "$hostKind::$runtimeKind"

    if (!$runtimeCompatibilityRowsByKey.ContainsKey($rowKey)) {
        $runtimeCompatibilityRowsByKey[$rowKey] = New-Object System.Collections.Generic.List[object]
    }

    $runtimeCompatibilityRowsByKey[$rowKey].Add([ordered]@{
        runtime_app_id = $runtimeApp.id
        runtime_app_source_id = Resolve-SourceId -Object $runtimeApp -SourceIdsByPath $sourceIdsByPath
        runtime_app_namespace = Get-OptionalValue -Object $runtimeApp -PropertyName "namespace"
        runtime_app_label = Get-OptionalValue -Object $runtimeApp -PropertyName "label"
        host_kind = $hostKind
        runtime_kind = $runtimeKind
        output_targets = @(
            @($runtimeApp.outputs) |
                ForEach-Object { $_.target } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        source_path = $runtimeApp.source_path
    })
}

$runtimeCompatibilityEntries = New-Object System.Collections.Generic.List[object]
foreach ($rowKey in ($runtimeCompatibilityRowsByKey.Keys | Sort-Object)) {
    $cells = @($runtimeCompatibilityRowsByKey[$rowKey] | Sort-Object runtime_app_id)
    $cell = $cells[0]
    $outputTargets = Flatten-UniqueValues -Entries $cells -CollectionSelector { param($entry) @($entry.output_targets) }
    $hasWasmTarget = @($outputTargets) -contains "wasm"
    $featurePackTier = if ($cell.host_kind -eq "native_ui" -and $hasWasmTarget) {
        "immersive_native_bridge"
    } elseif ($cell.host_kind -eq "native_ui") {
        "native_authoring_suite"
    } elseif ($cell.host_kind -eq "hybrid") {
        "cross_surface_runtime"
    } elseif ($cell.host_kind -eq "ks_runtime") {
        "scripted_orchestration"
    } else {
        "general_runtime_pack"
    }
    $budgetWindowTier = if ($cell.host_kind -eq "native_ui" -and $hasWasmTarget) {
        "gpu_and_streaming_budget_window"
    } elseif ($cell.host_kind -eq "native_ui") {
        "desktop_interactive_budget_window"
    } elseif ($cell.host_kind -eq "hybrid") {
        "balanced_runtime_budget_window"
    } elseif ($cell.host_kind -eq "ks_runtime") {
        "batch_orchestration_budget_window"
    } else {
        "default_budget_window"
    }
    $policyBundleId = "${featurePackTier}__${budgetWindowTier}"

    $runtimeCompatibilityEntries.Add([ordered]@{
        matrix_cell_id = "$($cell.host_kind)_$($cell.runtime_kind)_compatibility_cell"
        compatibility_matrix_id = $runtimeCompatibilityMatrixId
        compatibility_matrix_scope = $runtimeCompatibilityMatrixScope
        compatibility_window_id = $runtimeCompatibilityWindowId
        compatibility_window_scope = $runtimeCompatibilityWindowScope
        launch_readiness_id = $runtimeCompatibilityReadinessId
        launch_readiness_scope = $runtimeCompatibilityReadinessScope
        validation_policy = $runtimeCompatibilityValidationPolicy
        promotion_policy = $runtimeCompatibilityPromotionPolicy
        gate_policy = $runtimeCompatibilityGatePolicy
        backend_kind = $cell.host_kind
        target_kind = $cell.runtime_kind
        feature_pack_tier = $featurePackTier
        budget_window_tier = $budgetWindowTier
        policy_bundle_id = $policyBundleId
        runtime_app_count = $cells.Count
        runtime_app_ids = @($cells | ForEach-Object { $_.runtime_app_id })
        runtime_app_source_ids = @($cells | ForEach-Object { $_.runtime_app_source_id })
        runtime_app_namespaces = @($cells | ForEach-Object { $_.runtime_app_namespace })
        runtime_app_labels = @($cells | ForEach-Object { $_.runtime_app_label })
        runtime_app_source_paths = @($cells | ForEach-Object { $_.source_path })
        output_targets = $outputTargets
    })
}

$runtimeCompatibilityCatalog = [ordered]@{}
$runtimeCompatibilityCatalog["catalog_id"] = "runtime_compatibility_catalog"
$runtimeCompatibilityCatalog["catalog_scope"] = "runtime_target_backend_window_matrix_and_launch_readiness_metadata"
$runtimeCompatibilityCatalog["tensor_pipeline_id"] = "runtime_compatibility_tensor_pipeline"
$runtimeCompatibilityCatalog["manifest_source"] = "src-kain/stdlib/three_d_runtime/runtime_compatibility_runtime.kn"
$runtimeCompatibilityCatalog["entry_count"] = $runtimeCompatibilityEntries.Count
$runtimeCompatibilityCatalog["entries"] = $runtimeCompatibilityEntries.ToArray()
$runtimeCompatibilityCatalog["matrix_axes"] = [ordered]@{
    backend_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
    target_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
    feature_pack_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique)
    budget_window_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique)
    output_targets = Flatten-UniqueValues -Entries $runtimeCompatibilityEntries -CollectionSelector { param($entry) @($entry.output_targets) }
}
$runtimeCompatibilityCatalog["matrix_descriptor"] = [ordered]@{
    matrix_id = $runtimeCompatibilityMatrixId
    matrix_scope = $runtimeCompatibilityMatrixScope
    validation_policy = $runtimeCompatibilityValidationPolicy
}
$runtimeCompatibilityCatalog["window_descriptor"] = [ordered]@{
    window_id = $runtimeCompatibilityWindowId
    window_scope = $runtimeCompatibilityWindowScope
    promotion_policy = $runtimeCompatibilityPromotionPolicy
}
$runtimeCompatibilityCatalog["launch_readiness_descriptor"] = [ordered]@{
    readiness_id = $runtimeCompatibilityReadinessId
    readiness_scope = $runtimeCompatibilityReadinessScope
    gate_policy = $runtimeCompatibilityGatePolicy
}
$runtimeCompatibilityDescriptorRoot = "generated/runtime-compatibility/descriptors"
$runtimeCompatibilityDescriptorDocuments = New-Object System.Collections.Generic.List[object]
$runtimeCompatibilityDescriptorDocuments.Add([ordered]@{
    descriptor_document_id = "${runtimeCompatibilityMatrixId}_descriptor_document"
    descriptor_path = "$runtimeCompatibilityDescriptorRoot/$runtimeCompatibilityMatrixId.json"
    descriptor_kind = "compatibility_matrix"
    descriptor_id = $runtimeCompatibilityMatrixId
    descriptor_scope = $runtimeCompatibilityMatrixScope
    policy = [ordered]@{
        name = "validation_policy"
        value = $runtimeCompatibilityValidationPolicy
    }
    runtime_links = [ordered]@{
        tensor_pipeline_id = "runtime_compatibility_tensor_pipeline"
        build_graph_id = "runtime_compatibility_delivery_graph"
        distribution_channel_id = "runtime_compatibility_delivery_registry"
        gpu_kernel_id = "runtime_compatibility_resolve"
    }
    matrix_axes = [ordered]@{
        backend_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
        target_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
        feature_pack_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique)
        budget_window_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique)
        output_targets = Flatten-UniqueValues -Entries $runtimeCompatibilityEntries -CollectionSelector { param($entry) @($entry.output_targets) }
    }
    artifact_roots = @(
        "generated/runtime-compatibility",
        "generated/distribution/runtime-compatibility"
    )
})
$runtimeCompatibilityDescriptorDocuments.Add([ordered]@{
    descriptor_document_id = "${runtimeCompatibilityWindowId}_descriptor_document"
    descriptor_path = "$runtimeCompatibilityDescriptorRoot/$runtimeCompatibilityWindowId.json"
    descriptor_kind = "compatibility_window"
    descriptor_id = $runtimeCompatibilityWindowId
    descriptor_scope = $runtimeCompatibilityWindowScope
    policy = [ordered]@{
        name = "promotion_policy"
        value = $runtimeCompatibilityPromotionPolicy
    }
    runtime_links = [ordered]@{
        tensor_pipeline_id = "runtime_compatibility_tensor_pipeline"
        build_graph_id = "runtime_compatibility_delivery_graph"
        distribution_channel_id = "runtime_compatibility_delivery_registry"
        gpu_kernel_id = "runtime_compatibility_resolve"
    }
    matrix_axes = [ordered]@{
        backend_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
        target_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
        feature_pack_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique)
        budget_window_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique)
        output_targets = Flatten-UniqueValues -Entries $runtimeCompatibilityEntries -CollectionSelector { param($entry) @($entry.output_targets) }
    }
    artifact_roots = @(
        "generated/runtime-compatibility",
        "generated/distribution/runtime-compatibility"
    )
})
$runtimeCompatibilityDescriptorDocuments.Add([ordered]@{
    descriptor_document_id = "${runtimeCompatibilityReadinessId}_descriptor_document"
    descriptor_path = "$runtimeCompatibilityDescriptorRoot/$runtimeCompatibilityReadinessId.json"
    descriptor_kind = "launch_readiness"
    descriptor_id = $runtimeCompatibilityReadinessId
    descriptor_scope = $runtimeCompatibilityReadinessScope
    policy = [ordered]@{
        name = "gate_policy"
        value = $runtimeCompatibilityGatePolicy
    }
    runtime_links = [ordered]@{
        tensor_pipeline_id = "runtime_compatibility_tensor_pipeline"
        build_graph_id = "runtime_compatibility_delivery_graph"
        distribution_channel_id = "runtime_compatibility_delivery_registry"
        gpu_kernel_id = "runtime_compatibility_resolve"
    }
    matrix_axes = [ordered]@{
        backend_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
        target_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
        feature_pack_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique)
        budget_window_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique)
        output_targets = Flatten-UniqueValues -Entries $runtimeCompatibilityEntries -CollectionSelector { param($entry) @($entry.output_targets) }
    }
    artifact_roots = @(
        "generated/runtime-compatibility",
        "generated/distribution/runtime-compatibility"
    )
})
$runtimeCompatibilityDescriptorDocuments.Add([ordered]@{
    descriptor_document_id = "runtime_feature_pack_windows_descriptor_document"
    descriptor_path = "$runtimeCompatibilityDescriptorRoot/runtime_feature_pack_windows.json"
    descriptor_kind = "feature_pack_windows"
    descriptor_id = "runtime_feature_pack_windows"
    descriptor_scope = "manifest_derived_feature_pack_and_budget_window_tiering"
    policy = [ordered]@{
        name = "tier_policy"
        value = "manifest_derived_feature_pack_and_budget_window_tiering"
    }
    runtime_links = [ordered]@{
        tensor_pipeline_id = "runtime_compatibility_tensor_pipeline"
        build_graph_id = "runtime_compatibility_delivery_graph"
        distribution_channel_id = "runtime_compatibility_delivery_registry"
        gpu_kernel_id = "runtime_compatibility_resolve"
    }
    matrix_axes = [ordered]@{
        backend_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
        target_kinds = @($runtimeCompatibilityEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
        feature_pack_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique)
        budget_window_tiers = @($runtimeCompatibilityEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique)
        output_targets = Flatten-UniqueValues -Entries $runtimeCompatibilityEntries -CollectionSelector { param($entry) @($entry.output_targets) }
    }
    feature_pack_windows = @(
        @($runtimeCompatibilityEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique) | ForEach-Object {
            $featurePackTier = [string]$_
            $tierEntries = @($runtimeCompatibilityEntries | Where-Object { $_.feature_pack_tier -eq $featurePackTier })
            [ordered]@{
                feature_pack_tier = $featurePackTier
                budget_window_tiers = @($tierEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique)
                matrix_cell_ids = @($tierEntries | ForEach-Object { $_.matrix_cell_id } | Sort-Object -Unique)
                runtime_app_ids = @($tierEntries | ForEach-Object { @($_.runtime_app_ids) } | Sort-Object -Unique)
                backend_kinds = @($tierEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
                target_kinds = @($tierEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
                output_targets = Flatten-UniqueValues -Entries $tierEntries -CollectionSelector { param($entry) @($entry.output_targets) }
            }
        }
    )
    artifact_roots = @(
        "generated/runtime-compatibility",
        "generated/distribution/runtime-compatibility"
    )
})
$runtimeCompatibilityCatalog["tier_views"] = [ordered]@{
    feature_pack_windows = @(
        @($runtimeCompatibilityEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique) | ForEach-Object {
            $featurePackTier = [string]$_
            $tierEntries = @($runtimeCompatibilityEntries | Where-Object { $_.feature_pack_tier -eq $featurePackTier })
            [ordered]@{
                feature_pack_tier = $featurePackTier
                budget_window_tiers = @($tierEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique)
                policy_bundle_ids = @($tierEntries | ForEach-Object { $_.policy_bundle_id } | Sort-Object -Unique)
                matrix_cell_ids = @($tierEntries | ForEach-Object { $_.matrix_cell_id } | Sort-Object -Unique)
                runtime_app_ids = @($tierEntries | ForEach-Object { @($_.runtime_app_ids) } | Sort-Object -Unique)
                backend_kinds = @($tierEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
                target_kinds = @($tierEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
                output_targets = Flatten-UniqueValues -Entries $tierEntries -CollectionSelector { param($entry) @($entry.output_targets) }
            }
        }
    )
    budget_window_profiles = @(
        @($runtimeCompatibilityEntries | ForEach-Object { $_.budget_window_tier } | Sort-Object -Unique) | ForEach-Object {
            $budgetWindowTier = [string]$_
            $tierEntries = @($runtimeCompatibilityEntries | Where-Object { $_.budget_window_tier -eq $budgetWindowTier })
            [ordered]@{
                budget_window_tier = $budgetWindowTier
                feature_pack_tiers = @($tierEntries | ForEach-Object { $_.feature_pack_tier } | Sort-Object -Unique)
                policy_bundle_ids = @($tierEntries | ForEach-Object { $_.policy_bundle_id } | Sort-Object -Unique)
                matrix_cell_ids = @($tierEntries | ForEach-Object { $_.matrix_cell_id } | Sort-Object -Unique)
                runtime_app_ids = @($tierEntries | ForEach-Object { @($_.runtime_app_ids) } | Sort-Object -Unique)
                backend_kinds = @($tierEntries | ForEach-Object { $_.backend_kind } | Sort-Object -Unique)
                target_kinds = @($tierEntries | ForEach-Object { $_.target_kind } | Sort-Object -Unique)
                output_targets = Flatten-UniqueValues -Entries $tierEntries -CollectionSelector { param($entry) @($entry.output_targets) }
            }
        }
    )
}
$runtimeCompatibilityCatalog["descriptor_count"] = $runtimeCompatibilityDescriptorDocuments.Count
$runtimeCompatibilityCatalog["descriptor_root"] = $runtimeCompatibilityDescriptorRoot
$runtimeCompatibilityCatalog["descriptor_paths"] = @($runtimeCompatibilityDescriptorDocuments | ForEach-Object { $_.descriptor_path })
$runtimeCompatibilityCatalog["artifact_roots"] = @(
    "generated/runtime-compatibility",
    "generated/distribution/runtime-compatibility"
)
$runtimeCompatibilityCatalog["reflection_catalogs"] = @(
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
$runtimeCompatibilityCatalog["linked_build_graph_id"] = "runtime_compatibility_delivery_graph"
$runtimeCompatibilityCatalog["linked_distribution_channel_id"] = "runtime_compatibility_delivery_registry"
$runtimeCompatibilityCatalog["linked_tensor_pipeline_id"] = "runtime_compatibility_tensor_pipeline"
$runtimeCompatibilityCatalog["linked_kernel_id"] = "runtime_compatibility_resolve"
$runtimeCompatibilityCatalog["indexes"] = [ordered]@{}
Write-Host "Building runtime compatibility indexes"
$runtimeCompatibilityCatalog["indexes"]["by_matrix_cell_id"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) $entry.matrix_cell_id } -ValueSelector { param($entry) $entry.target_kind }
$runtimeCompatibilityCatalog["indexes"]["by_backend_kind"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) $entry.backend_kind } -ValueSelector { param($entry) $entry.target_kind }
$runtimeCompatibilityCatalog["indexes"]["by_target_kind"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) $entry.target_kind } -ValueSelector { param($entry) $entry.backend_kind }
$runtimeCompatibilityCatalog["indexes"]["by_runtime_app"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) @($entry.runtime_app_ids) } -ValueSelector { param($entry) $entry.backend_kind }
$runtimeCompatibilityCatalog["indexes"]["by_runtime_app_source_id"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) @($entry.runtime_app_source_ids) } -ValueSelector { param($entry) $entry.backend_kind }
$runtimeCompatibilityCatalog["indexes"]["by_output_target"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) @($entry.output_targets) } -ValueSelector { param($entry) $entry.backend_kind }
$runtimeCompatibilityCatalog["indexes"]["by_source_path"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) @($entry.runtime_app_source_paths) } -ValueSelector { param($entry) $entry.backend_kind }
$runtimeCompatibilityCatalog["indexes"]["by_source_id"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) @($entry.runtime_app_source_ids) } -ValueSelector { param($entry) $entry.backend_kind }
$runtimeCompatibilityCatalog["indexes"]["by_feature_pack_tier"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) $entry.feature_pack_tier } -ValueSelector { param($entry) $entry.matrix_cell_id }
$runtimeCompatibilityCatalog["indexes"]["by_budget_window_tier"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) $entry.budget_window_tier } -ValueSelector { param($entry) $entry.matrix_cell_id }
$runtimeCompatibilityCatalog["indexes"]["by_policy_bundle_id"] = New-IndexMap -Entries $runtimeCompatibilityEntries -KeySelector { param($entry) $entry.policy_bundle_id } -ValueSelector { param($entry) $entry.matrix_cell_id }

$jobsGraph = $buildGraphs | Where-Object { $_.id -eq "jobs_dispatch_graph" } | Select-Object -First 1
$jobsChannel = $distributionChannels | Where-Object { $_.id -eq "jobs_delivery_registry" } | Select-Object -First 1
$jobsPipeline = $tensorPipelines | Where-Object { $_.id -eq "jobs_dispatch_tensor_pipeline" } | Select-Object -First 1
$jobsPipelinePasses = if ($null -ne $jobsPipeline) { @($jobsPipeline.passes) } else { @() }
$jobsDispatchGraphId = if ($null -ne $jobsGraph) { [string]($jobsGraph | Select-Object -ExpandProperty id -First 1) } else { "jobs_dispatch_graph" }
$jobsQueueId = if ($null -ne $jobsGraph) { [string]($jobsGraph | Select-Object -ExpandProperty queue -First 1) } else { "jobs_dispatch_queue" }
$jobsDistributionChannelId = if ($null -ne $jobsChannel) { [string]($jobsChannel | Select-Object -ExpandProperty id -First 1) } else { "jobs_delivery_registry" }
$jobsDeliveryApprovalPolicy = if ($null -ne $jobsChannel) { [string]($jobsChannel | Select-Object -ExpandProperty approval_policy -First 1) } else { $null }
$jobsPipelineId = if ($null -ne $jobsPipeline) { [string]($jobsPipeline | Select-Object -ExpandProperty id -First 1) } else { "jobs_dispatch_tensor_pipeline" }
$jobsPipelineDomain = if ($null -ne $jobsPipeline) { [string]($jobsPipeline | Select-Object -ExpandProperty domain -First 1) } else { $null }
$jobsPipelinePriority = if ($null -ne $jobsPipeline) { [string]($jobsPipeline | Select-Object -ExpandProperty priority -First 1) } else { $null }
$jobsPipelineResidency = if ($null -ne $jobsPipeline) { [string]($jobsPipeline | Select-Object -ExpandProperty residency -First 1) } else { $null }
$jobsPipelinePassIds = @(
    $jobsPipelinePasses |
        ForEach-Object { [string]$_ } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object -Unique
)

$gpuKernelsById = @{}
foreach ($kernel in $gpuKernels) {
    $gpuKernelsById[$kernel.id] = $kernel
}

$jobsPipelineKernels = New-Object System.Collections.Generic.List[object]
foreach ($passId in $jobsPipelinePasses) {
    $kernel = if ($gpuKernelsById.ContainsKey($passId)) { $gpuKernelsById[$passId] } else { $null }
    $jobsPipelineKernels.Add([ordered]@{
        id = $passId
        source_path = if ($null -ne $kernel) { $kernel.source_path } else { $null }
        stage = if ($null -ne $kernel) { $kernel.stage } else { $null }
        tensor_role = if ($null -ne $kernel) { $kernel.tensor_role } else { $null }
        consumes = if ($null -ne $kernel) { @($kernel.consumes) } else { @() }
        produces = if ($null -ne $kernel) { @($kernel.produces) } else { @() }
    })
}

$jobsReceiptFiles = @(Get-ChildItem -Path (Join-Path $generatedRoot "jobs\receipts") -File -Filter "*.json" | Sort-Object Name)
$jobsRetryFiles = @(Get-ChildItem -Path (Join-Path $generatedRoot "jobs\retries") -File -Filter "*.json" | Sort-Object Name)
$jobsReceiptEntries = @(
    $jobsReceiptFiles | ForEach-Object {
        [ordered]@{
            data = Read-JsonFile -Path $_.FullName
            path = "generated/jobs/receipts/$($_.Name)"
        }
    }
)
$jobsRetryEntries = @(
    $jobsRetryFiles | ForEach-Object {
        [ordered]@{
            data = Read-JsonFile -Path $_.FullName
            path = "generated/jobs/retries/$($_.Name)"
        }
    }
)

$jobsSchema = Read-JsonFile -Path (Join-Path $generatedRoot "schemas/jobs/receipts/schema.json")
$jobsSchemaIndex = Read-JsonFile -Path (Join-Path $generatedRoot "schemas/jobs/receipts/indexes/catalog.json")
$jobsTemplateFiles = @(Get-ChildItem -Path (Join-Path $generatedRoot "schemas/jobs/receipts/templates") -File -Filter "*.json" | Sort-Object Name)
$jobsTemplateEntries = @(
    $jobsTemplateFiles | ForEach-Object {
        [ordered]@{
            data = Read-JsonFile -Path $_.FullName
            path = "generated/schemas/jobs/receipts/templates/$($_.Name)"
        }
    }
)

$receiptExamples = @(
    $jobsReceiptEntries | ForEach-Object {
        [ordered]@{
            receipt_id = $_.data.receipt_id
            receipt_path = $_.path
            queue_id = $_.data.queue_id
            dispatch_graph_id = $_.data.dispatch_graph_id
            distribution_channel_id = $_.data.distribution_channel_id
            retry_ledger_id = $_.data.retry_ledger_id
            job_state = $_.data.job_state
            promotion_state = $_.data.promotion_state
        }
    }
)

$retryEntryExamples = @(
    $jobsRetryEntries | ForEach-Object {
        $retryData = $_.data
        $states = @()
        $resumePolicies = @()
        $jobReceiptIds = @()

        foreach ($ledgerEntry in @($retryData.entries)) {
            if ($null -ne $ledgerEntry.state) { $states += $ledgerEntry.state }
            if ($null -ne $ledgerEntry.resume_policy) { $resumePolicies += $ledgerEntry.resume_policy }
            if ($null -ne $ledgerEntry.job_receipt_id) { $jobReceiptIds += $ledgerEntry.job_receipt_id }
        }

        [ordered]@{
            ledger_id = $retryData.ledger_id
            ledger_path = $_.path
            ledger_kind = $retryData.ledger_kind
            reflection_catalog_id = $retryData.reflection_catalog_id
            states = @($states | Sort-Object -Unique)
            resume_policies = @($resumePolicies | Sort-Object -Unique)
            job_receipt_ids = @($jobReceiptIds | Sort-Object -Unique)
        }
    }
)

$jobsReceiptSchemaCatalog = [ordered]@{}
$jobsReceiptSchemaCatalog["catalog_id"] = "jobs_receipt_schema_catalog"
$jobsReceiptSchemaCatalog["catalog_scope"] = "jobs_delivery_receipt_schema_and_retry_resume_contract_metadata"
$jobsReceiptSchemaCatalog["tensor_pipeline_id"] = $jobsPipelineId
$jobsReceiptSchemaCatalog["schema_id"] = $jobsSchema.schema_id
$jobsReceiptSchemaCatalog["document_kind"] = $jobsSchema.document_kind
$jobsReceiptSchemaCatalog["schema_root"] = "generated/schemas/jobs/receipts"
$jobsReceiptSchemaCatalog["template_root"] = "generated/schemas/jobs/receipts/templates"
$jobsReceiptSchemaCatalog["index_root"] = "generated/schemas/jobs/receipts/indexes"
$jobsReceiptSchemaCatalog["artifact_index"] = "generated/schemas/jobs/receipts/indexes/catalog.json"
$jobsReceiptSchemaCatalog["entry_count"] = 1
$jobsReceiptSchemaEntry = [ordered]@{}
$jobsReceiptSchemaEntry["schema_id"] = $jobsSchema.schema_id
$jobsReceiptSchemaEntry["schema_version"] = $jobsSchema.schema_version
$jobsReceiptSchemaEntry["validation_policy"] = $jobsSchema.validation_policy
$jobsReceiptSchemaEntry["queue_id"] = $jobsQueueId
$jobsReceiptSchemaEntry["dispatch_graph_id"] = $jobsDispatchGraphId
$jobsReceiptSchemaEntry["distribution_channel_id"] = $jobsDistributionChannelId
$jobsReceiptSchemaEntry["retry_ledger_id"] = "jobs_retry_ledger"
$jobsReceiptSchemaEntry["tensor_pipeline_id"] = $jobsPipelineId
$jobsReceiptSchemaEntry["tensor_pipeline_domain"] = $jobsPipelineDomain
$jobsReceiptSchemaEntry["tensor_pipeline_priority"] = $jobsPipelinePriority
$jobsReceiptSchemaEntry["tensor_pipeline_residency"] = $jobsPipelineResidency
$jobsReceiptSchemaEntry["linked_kernel_ids"] = @($jobsPipelinePassIds)
$jobsReceiptSchemaEntry["linked_kernels"] = @($jobsPipelineKernels.ToArray())
$jobsReceiptSchemaEntry["template_ids"] = @($jobsSchemaIndex.template_ids)
$jobsReceiptSchemaEntry["manifest_inputs"] = if ($null -ne $jobsGraph) { @($jobsGraph.inputs) } else { @() }
$jobsReceiptSchemaEntry["artifact_roots"] = if ($null -ne $jobsGraph) { @($jobsGraph.outputs) } elseif ($null -ne $jobsChannel) { @($jobsChannel.artifact_roots) } else { @() }
$jobsReceiptSchemaEntry["examples"] = @($receiptExamples)
$jobsReceiptSchemaCatalog["entries"] = @($jobsReceiptSchemaEntry)
$jobsReceiptSchemaCatalog["indexes"] = [ordered]@{}
$jobsReceiptSchemaCatalog["artifact_roots"] = @(
    "generated/jobs/receipts",
    "generated/jobs/retries",
    "generated/schemas/jobs/receipts",
    "generated/runtime-reflection/jobs-receipt-schemas"
)
$jobsReceiptSchemaCatalog["reflection_catalogs"] = @(
    "jobs_receipt_template_catalog",
    "jobs_retry_ledger_catalog",
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
Write-Host "Building jobs receipt-schema indexes"
$jobsReceiptSchemaCatalog["indexes"]["by_queue"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.queue_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_dispatch_graph"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.dispatch_graph_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_distribution_channel"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.distribution_channel_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_retry_ledger"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.retry_ledger_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_job_state"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.job_state } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_promotion_state"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.promotion_state } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_template_id"] = New-IndexMap -Entries @($jobsSchemaIndex) -KeySelector { param($entry) @($entry.template_ids) } -ValueSelector { param($entry) $entry.schema_id }
$jobsReceiptSchemaCatalog["indexes"]["by_tensor_pipeline"] = New-IndexMap -Entries @(@{ id = $jobsPipelineId }) -KeySelector { param($entry) $entry.id } -ValueSelector { param($entry) "jobs_receipt_schema_catalog" }
$jobsReceiptSchemaCatalog["indexes"]["by_kernel"] = New-IndexMap -Entries $jobsPipelineKernels -KeySelector { param($entry) $entry.id } -ValueSelector { param($entry) "jobs_receipt_schema_catalog" }
$jobsReceiptSchemaCatalog["indexes"]["by_resume_policy"] = New-IndexMap -Entries $jobsRetryEntries -KeySelector { param($entry) @($entry.data.entries | ForEach-Object { $_.resume_policy }) } -ValueSelector { param($entry) $entry.data.ledger_id }

$jobsReceiptTemplateCatalog = [ordered]@{}
$jobsReceiptTemplateCatalog["catalog_id"] = "jobs_receipt_template_catalog"
$jobsReceiptTemplateCatalog["catalog_scope"] = "jobs_delivery_receipt_template_and_index_metadata"
$jobsReceiptTemplateCatalog["tensor_pipeline_id"] = $jobsPipelineId
$jobsReceiptTemplateCatalog["schema_id"] = $jobsSchema.schema_id
$jobsReceiptTemplateCatalog["entry_count"] = $jobsTemplateEntries.Count
$jobsReceiptTemplateCatalog["entries"] = @(
    $jobsTemplateEntries | ForEach-Object {
        [ordered]@{
            template_id = $_.data.template_id
            template_path = $_.path
            template_version = Get-OptionalValue -Object $_.data -PropertyName "template_version"
            schema_id = $_.data.schema_id
            dispatch_graph_id = $_.data.dispatch_graph_id
            queue_id = $_.data.queue_id
            retry_ledger_id = $_.data.retry_ledger_id
            distribution_channel_id = $_.data.distribution_channel_id
            index_path = "generated/schemas/jobs/receipts/indexes/catalog.json"
            linked_kernel_ids = @($jobsPipelinePassIds)
            linked_build_graph_ids = @($jobsDispatchGraphId)
            linked_distribution_channel_ids = @($jobsDistributionChannelId)
            examples = @($receiptExamples)
        }
    }
)
$jobsReceiptTemplateCatalog["indexes"] = [ordered]@{}
$jobsReceiptTemplateCatalog["template_ids"] = @($jobsSchemaIndex.template_ids)
$jobsReceiptTemplateCatalog["index_path"] = "generated/schemas/jobs/receipts/indexes/catalog.json"
$jobsReceiptTemplateCatalog["artifact_roots"] = @(
    "generated/schemas/jobs/receipts/templates",
    "generated/schemas/jobs/receipts/indexes",
    "generated/runtime-reflection/jobs-receipt-templates"
)
$jobsReceiptTemplateCatalog["reflection_catalogs"] = @(
    "jobs_receipt_schema_catalog",
    "jobs_retry_ledger_catalog",
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
Write-Host "Building jobs receipt-template indexes"
$jobsReceiptTemplateCatalog["indexes"]["by_template_id"] = New-IndexMap -Entries $jobsReceiptTemplateCatalog.entries -KeySelector { param($entry) $entry.template_id } -ValueSelector { param($entry) $entry.template_id }
$jobsReceiptTemplateCatalog["indexes"]["by_schema_id"] = New-IndexMap -Entries $jobsReceiptTemplateCatalog.entries -KeySelector { param($entry) $entry.schema_id } -ValueSelector { param($entry) $entry.template_id }
$jobsReceiptTemplateCatalog["indexes"]["by_dispatch_graph"] = New-IndexMap -Entries $jobsReceiptTemplateCatalog.entries -KeySelector { param($entry) $entry.dispatch_graph_id } -ValueSelector { param($entry) $entry.template_id }
$jobsReceiptTemplateCatalog["indexes"]["by_queue"] = New-IndexMap -Entries $jobsReceiptTemplateCatalog.entries -KeySelector { param($entry) $entry.queue_id } -ValueSelector { param($entry) $entry.template_id }
$jobsReceiptTemplateCatalog["indexes"]["by_distribution_channel"] = New-IndexMap -Entries $jobsReceiptTemplateCatalog.entries -KeySelector { param($entry) $entry.distribution_channel_id } -ValueSelector { param($entry) $entry.template_id }
$jobsReceiptTemplateCatalog["indexes"]["by_retry_ledger"] = New-IndexMap -Entries $jobsReceiptTemplateCatalog.entries -KeySelector { param($entry) $entry.retry_ledger_id } -ValueSelector { param($entry) $entry.template_id }
$jobsReceiptTemplateCatalog["indexes"]["by_kernel"] = New-IndexMap -Entries $jobsPipelineKernels -KeySelector { param($entry) $entry.id } -ValueSelector { param($entry) @($jobsReceiptTemplateCatalog.entries | ForEach-Object { $_.template_id }) }

$jobsRetryLedgerCatalog = [ordered]@{}
$jobsRetryLedgerCatalog["catalog_id"] = "jobs_retry_ledger_catalog"
$jobsRetryLedgerCatalog["catalog_scope"] = "jobs_retry_ledger_and_worker_requeue_metadata"
$jobsRetryLedgerCatalog["tensor_pipeline_id"] = $jobsPipelineId
$jobsRetryLedgerCatalog["entry_count"] = $retryEntryExamples.Count
$jobsRetryLedgerCatalog["entries"] = @(
    $retryEntryExamples | ForEach-Object {
        [ordered]@{
            retry_ledger_id = $_.ledger_id
            retry_ledger_path = $_.ledger_path
            retry_ledger_kind = $_.ledger_kind
            dispatch_graph_id = $jobsDispatchGraphId
            queue_id = $jobsQueueId
            delivery_registry_id = $jobsDistributionChannelId
            delivery_approval_policy = $jobsDeliveryApprovalPolicy
            linked_receipt_ids = @($_.job_receipt_ids)
            states = @($_.states)
            resume_policies = @($_.resume_policies)
            reflection_catalog_id = $_.reflection_catalog_id
            linked_tensor_pipeline_ids = @($jobsPipelineId)
            linked_kernel_ids = @($jobsPipelinePassIds)
        }
    }
)
$jobsRetryLedgerCatalog["indexes"] = [ordered]@{}
$jobsRetryLedgerCatalog["artifact_roots"] = @(
    "generated/jobs/retries",
    "generated/runtime-reflection/jobs-retry-ledgers"
)
$jobsRetryLedgerCatalog["reflection_catalogs"] = @(
    "jobs_receipt_schema_catalog",
    "jobs_receipt_template_catalog",
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
Write-Host "Building jobs retry-ledger indexes"
$jobsRetryLedgerCatalog["indexes"]["by_retry_ledger"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.retry_ledger_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_dispatch_graph"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.dispatch_graph_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_queue"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.queue_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_delivery_registry"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.delivery_registry_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_state"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.states) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_resume_policy"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.resume_policies) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_receipt_id"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.linked_receipt_ids) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_kernel"] = New-IndexMap -Entries $jobsPipelineKernels -KeySelector { param($entry) $entry.id } -ValueSelector { param($entry) @($jobsRetryLedgerCatalog.entries | ForEach-Object { $_.retry_ledger_id }) }

$resourceReflectionCatalogId = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.reflection_catalog\.catalog_id = "([^"]+)"'
$resourceReflectionCatalogScope = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.reflection_catalog\.catalog_scope = "([^"]+)"'
$resourceReflectionExportRoot = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.reflection_catalog\.export_root = "([^"]+)"'
$resourceReflectionInspectionId = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.inspection_runtime\.inspection_id = "([^"]+)"'
$resourceReflectionInspectionScope = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.inspection_runtime\.inspection_scope = "([^"]+)"'
$resourceReflectionInspectionPolicy = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.inspection_runtime\.query_policy = "([^"]+)"'
$resourceReflectionCompatibilityId = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.compatibility_runtime\.compatibility_id = "([^"]+)"'
$resourceReflectionCompatibilityScope = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.compatibility_runtime\.compatibility_scope = "([^"]+)"'
$resourceReflectionCompatibilityPolicy = Get-RegexCapture -Text $resourceReflectionRuntimeSource -Pattern 'profile\.compatibility_runtime\.gate_policy = "([^"]+)"'

$resourceReflectionBuildGraph = $buildGraphs | Where-Object { $_.id -eq "resource_reflection_delivery_graph" } | Select-Object -First 1
$resourceReflectionChannel = $distributionChannels | Where-Object { $_.id -eq "resource_reflection_delivery_registry" } | Select-Object -First 1
$resourceReflectionPipeline = $tensorPipelines | Where-Object { $_.id -eq "resource_reflection_tensor_pipeline" } | Select-Object -First 1
$resourceReflectionKernel = $gpuKernels | Where-Object { $_.id -eq "resource_reflection_catalog_resolve" } | Select-Object -First 1

$resourceReflectionBuildGraphId = if ($null -ne $resourceReflectionBuildGraph) { [string]($resourceReflectionBuildGraph | Select-Object -ExpandProperty id -First 1) } else { "resource_reflection_delivery_graph" }
$resourceReflectionBuildGraphQueue = if ($null -ne $resourceReflectionBuildGraph) { [string]($resourceReflectionBuildGraph | Select-Object -ExpandProperty queue -First 1) } else { $null }
$resourceReflectionBuildGraphInputs = if ($null -ne $resourceReflectionBuildGraph) { @($resourceReflectionBuildGraph.inputs) } else { @() }
$resourceReflectionBuildGraphOutputs = if ($null -ne $resourceReflectionBuildGraph) { @($resourceReflectionBuildGraph.outputs) } else { @() }
$resourceReflectionChannelId = if ($null -ne $resourceReflectionChannel) { [string]($resourceReflectionChannel | Select-Object -ExpandProperty id -First 1) } else { "resource_reflection_delivery_registry" }
$resourceReflectionChannelKind = if ($null -ne $resourceReflectionChannel) { [string]($resourceReflectionChannel | Select-Object -ExpandProperty channel_kind -First 1) } else { $null }
$resourceReflectionChannelApprovalPolicy = if ($null -ne $resourceReflectionChannel) { [string]($resourceReflectionChannel | Select-Object -ExpandProperty approval_policy -First 1) } else { $null }
$resourceReflectionChannelArtifactRoots = if ($null -ne $resourceReflectionChannel) { @($resourceReflectionChannel.artifact_roots) } else { @() }
$resourceReflectionPipelineId = if ($null -ne $resourceReflectionPipeline) { [string]($resourceReflectionPipeline | Select-Object -ExpandProperty id -First 1) } else { "resource_reflection_tensor_pipeline" }
$resourceReflectionPipelineDomain = if ($null -ne $resourceReflectionPipeline) { [string]($resourceReflectionPipeline | Select-Object -ExpandProperty domain -First 1) } else { $null }
$resourceReflectionPipelinePriority = if ($null -ne $resourceReflectionPipeline) { [string]($resourceReflectionPipeline | Select-Object -ExpandProperty priority -First 1) } else { $null }
$resourceReflectionPipelineResidency = if ($null -ne $resourceReflectionPipeline) { [string]($resourceReflectionPipeline | Select-Object -ExpandProperty residency -First 1) } else { $null }
$resourceReflectionPipelinePassIds = if ($null -ne $resourceReflectionPipeline) { @($resourceReflectionPipeline.passes | ForEach-Object { [string]$_ } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique) } else { @() }
$resourceReflectionKernelId = if ($null -ne $resourceReflectionKernel) { [string]($resourceReflectionKernel | Select-Object -ExpandProperty id -First 1) } else { "resource_reflection_catalog_resolve" }

$resourceReflectionContract = @($runtimeContractsCatalog.entries | Where-Object { $_.contract_id -eq "resource_reflection_runtime" } | Select-Object -First 1)
$resourceRuntimeContract = @($runtimeContractsCatalog.entries | Where-Object { $_.contract_id -eq "resource_runtime" } | Select-Object -First 1)
$resourceReflectionContractEntries = @(
    $resourceReflectionContract | ForEach-Object {
        [ordered]@{
            contract_id = $_.contract_id
            source_path = $_.source_path
        }
    }
    $resourceRuntimeContract | ForEach-Object {
        [ordered]@{
            contract_id = $_.contract_id
            source_path = $_.source_path
        }
    }
)

$resourceReflectionKernelMetadata = [ordered]@{
    id = $resourceReflectionKernelId
    source_path = if ($null -ne $resourceReflectionKernel) { $resourceReflectionKernel.source_path } else { "src-kain/kernels/resource/resource_reflection_catalog_resolve.kn" }
    stage = if ($null -ne $resourceReflectionKernel) { $resourceReflectionKernel.stage } else { $null }
    entry = if ($null -ne $resourceReflectionKernel) { $resourceReflectionKernel.entry } else { "resource_reflection_catalog_resolve" }
    tensor_role = if ($null -ne $resourceReflectionKernel) { $resourceReflectionKernel.tensor_role } else { $null }
    dispatch_shape = if ($null -ne $resourceReflectionKernel) { @($resourceReflectionKernel.dispatch_shape) } else { @() }
    consumes = if ($null -ne $resourceReflectionKernel) { @($resourceReflectionKernel.consumes) } else { @() }
    produces = if ($null -ne $resourceReflectionKernel) { @($resourceReflectionKernel.produces) } else { @() }
}

$resourceReflectionGpuCatalogEntries = @(
    $gpuReflectionCatalog.entries |
        Where-Object { @($resourceReflectionPipelinePassIds + $resourceReflectionKernelId) -contains [string]$_.id } |
        Sort-Object id |
        ForEach-Object {
            [ordered]@{
                id = $_.id
                stage = $_.stage
                tensor_role = $_.tensor_role
                source_path = $_.source_path
                entry = $_.entry
                dispatch_shape = @($_.dispatch_shape)
                consumes = @($_.consumes)
                produces = @($_.produces)
            }
        }
)

$resourceReflectionContractCatalogEntries = @(
    $runtimeContractsCatalog.entries |
        Where-Object { @($resourceReflectionContractEntries | ForEach-Object { $_.contract_id }) -contains [string]$_.contract_id } |
        Sort-Object contract_id |
        ForEach-Object {
            [ordered]@{
                contract_id = $_.contract_id
                source_path = $_.source_path
            }
        }
)

$resourceReflectionEntries = New-Object System.Collections.Generic.List[object]
$resourceReflectionEntries.Add([ordered]@{
    descriptor_kind = "reflection_catalog"
    descriptor_id = $resourceReflectionCatalogId
    descriptor_scope = $resourceReflectionCatalogScope
    export_root = $resourceReflectionExportRoot
    tensor_pipeline_id = $resourceReflectionPipelineId
    tensor_pipeline_domain = $resourceReflectionPipelineDomain
    tensor_pipeline_priority = $resourceReflectionPipelinePriority
    tensor_pipeline_residency = $resourceReflectionPipelineResidency
    tensor_pipeline_pass_ids = @($resourceReflectionPipelinePassIds)
    build_graph_id = $resourceReflectionBuildGraphId
    build_graph_queue = $resourceReflectionBuildGraphQueue
    build_graph_inputs = @($resourceReflectionBuildGraphInputs)
    build_graph_outputs = @($resourceReflectionBuildGraphOutputs)
    distribution_channel_id = $resourceReflectionChannelId
    distribution_channel_kind = $resourceReflectionChannelKind
    distribution_approval_policy = $resourceReflectionChannelApprovalPolicy
    distribution_artifact_roots = @($resourceReflectionChannelArtifactRoots)
    gpu_kernel_id = $resourceReflectionKernelMetadata.id
    gpu_kernel_stage = $resourceReflectionKernelMetadata.stage
    gpu_kernel_entry = $resourceReflectionKernelMetadata.entry
    gpu_kernel_tensor_role = $resourceReflectionKernelMetadata.tensor_role
    gpu_kernel_dispatch_shape = @($resourceReflectionKernelMetadata.dispatch_shape)
    gpu_kernel_consumes = @($resourceReflectionKernelMetadata.consumes)
    gpu_kernel_produces = @($resourceReflectionKernelMetadata.produces)
    linked_contract_ids = @(
        $resourceReflectionContract | ForEach-Object { $_.contract_id }
        $resourceRuntimeContract | ForEach-Object { $_.contract_id }
    )
    linked_contract_paths = @(
        $resourceReflectionContract | ForEach-Object { $_.source_path }
        $resourceRuntimeContract | ForEach-Object { $_.source_path }
    )
    policy_name = "catalog_scope"
    policy_value = $resourceReflectionCatalogScope
    artifact_roots = @("generated/resource-reflection", "generated/distribution/resource-reflection")
    source_path = "src-kain/stdlib/three_d_runtime/resource_reflection_runtime.kn"
})
$resourceReflectionEntries.Add([ordered]@{
    descriptor_kind = "inspection_runtime"
    descriptor_id = $resourceReflectionInspectionId
    descriptor_scope = $resourceReflectionInspectionScope
    export_root = $resourceReflectionExportRoot
    tensor_pipeline_id = $resourceReflectionPipelineId
    tensor_pipeline_domain = $resourceReflectionPipelineDomain
    tensor_pipeline_priority = $resourceReflectionPipelinePriority
    tensor_pipeline_residency = $resourceReflectionPipelineResidency
    tensor_pipeline_pass_ids = @($resourceReflectionPipelinePassIds)
    build_graph_id = $resourceReflectionBuildGraphId
    build_graph_queue = $resourceReflectionBuildGraphQueue
    build_graph_inputs = @($resourceReflectionBuildGraphInputs)
    build_graph_outputs = @($resourceReflectionBuildGraphOutputs)
    distribution_channel_id = $resourceReflectionChannelId
    distribution_channel_kind = $resourceReflectionChannelKind
    distribution_approval_policy = $resourceReflectionChannelApprovalPolicy
    distribution_artifact_roots = @($resourceReflectionChannelArtifactRoots)
    gpu_kernel_id = $resourceReflectionKernelMetadata.id
    gpu_kernel_stage = $resourceReflectionKernelMetadata.stage
    gpu_kernel_entry = $resourceReflectionKernelMetadata.entry
    gpu_kernel_tensor_role = $resourceReflectionKernelMetadata.tensor_role
    gpu_kernel_dispatch_shape = @($resourceReflectionKernelMetadata.dispatch_shape)
    gpu_kernel_consumes = @($resourceReflectionKernelMetadata.consumes)
    gpu_kernel_produces = @($resourceReflectionKernelMetadata.produces)
    linked_contract_ids = @(
        $resourceReflectionContract | ForEach-Object { $_.contract_id }
        $resourceRuntimeContract | ForEach-Object { $_.contract_id }
    )
    linked_contract_paths = @(
        $resourceReflectionContract | ForEach-Object { $_.source_path }
        $resourceRuntimeContract | ForEach-Object { $_.source_path }
    )
    policy_name = "query_policy"
    policy_value = $resourceReflectionInspectionPolicy
    artifact_roots = @("generated/resource-reflection", "generated/distribution/resource-reflection")
    source_path = "src-kain/stdlib/three_d_runtime/resource_reflection_runtime.kn"
})
$resourceReflectionEntries.Add([ordered]@{
    descriptor_kind = "compatibility_runtime"
    descriptor_id = $resourceReflectionCompatibilityId
    descriptor_scope = $resourceReflectionCompatibilityScope
    export_root = $resourceReflectionExportRoot
    tensor_pipeline_id = $resourceReflectionPipelineId
    tensor_pipeline_domain = $resourceReflectionPipelineDomain
    tensor_pipeline_priority = $resourceReflectionPipelinePriority
    tensor_pipeline_residency = $resourceReflectionPipelineResidency
    tensor_pipeline_pass_ids = @($resourceReflectionPipelinePassIds)
    build_graph_id = $resourceReflectionBuildGraphId
    build_graph_queue = $resourceReflectionBuildGraphQueue
    build_graph_inputs = @($resourceReflectionBuildGraphInputs)
    build_graph_outputs = @($resourceReflectionBuildGraphOutputs)
    distribution_channel_id = $resourceReflectionChannelId
    distribution_channel_kind = $resourceReflectionChannelKind
    distribution_approval_policy = $resourceReflectionChannelApprovalPolicy
    distribution_artifact_roots = @($resourceReflectionChannelArtifactRoots)
    gpu_kernel_id = $resourceReflectionKernelMetadata.id
    gpu_kernel_stage = $resourceReflectionKernelMetadata.stage
    gpu_kernel_entry = $resourceReflectionKernelMetadata.entry
    gpu_kernel_tensor_role = $resourceReflectionKernelMetadata.tensor_role
    gpu_kernel_dispatch_shape = @($resourceReflectionKernelMetadata.dispatch_shape)
    gpu_kernel_consumes = @($resourceReflectionKernelMetadata.consumes)
    gpu_kernel_produces = @($resourceReflectionKernelMetadata.produces)
    linked_contract_ids = @(
        $resourceReflectionContract | ForEach-Object { $_.contract_id }
        $resourceRuntimeContract | ForEach-Object { $_.contract_id }
    )
    linked_contract_paths = @(
        $resourceReflectionContract | ForEach-Object { $_.source_path }
        $resourceRuntimeContract | ForEach-Object { $_.source_path }
    )
    policy_name = "gate_policy"
    policy_value = $resourceReflectionCompatibilityPolicy
    artifact_roots = @("generated/resource-reflection", "generated/distribution/resource-reflection")
    source_path = "src-kain/stdlib/three_d_runtime/resource_reflection_runtime.kn"
})

$resourceReflectionDescriptorRoot = "generated/resource-reflection/descriptors"
$resourceReflectionDescriptorDocuments = New-Object System.Collections.Generic.List[object]
foreach ($resourceReflectionEntry in $resourceReflectionEntries) {
    $descriptorPath = "$resourceReflectionDescriptorRoot/$($resourceReflectionEntry.descriptor_id).json"
    $resourceReflectionEntry["descriptor_path"] = $descriptorPath

    $resourceReflectionDescriptorDocuments.Add([ordered]@{
        descriptor_document_id = "$($resourceReflectionEntry.descriptor_id)_descriptor_document"
        descriptor_path = $descriptorPath
        descriptor_kind = $resourceReflectionEntry.descriptor_kind
        descriptor_id = $resourceReflectionEntry.descriptor_id
        descriptor_scope = $resourceReflectionEntry.descriptor_scope
        export_root = $resourceReflectionEntry.export_root
        source_path = $resourceReflectionEntry.source_path
        policy = [ordered]@{
            name = $resourceReflectionEntry.policy_name
            value = $resourceReflectionEntry.policy_value
        }
        runtime_links = [ordered]@{
            tensor_pipeline_id = $resourceReflectionEntry.tensor_pipeline_id
            tensor_pipeline_domain = $resourceReflectionEntry.tensor_pipeline_domain
            tensor_pipeline_priority = $resourceReflectionEntry.tensor_pipeline_priority
            tensor_pipeline_residency = $resourceReflectionEntry.tensor_pipeline_residency
            tensor_pipeline_pass_ids = @($resourceReflectionEntry.tensor_pipeline_pass_ids)
            build_graph_id = $resourceReflectionEntry.build_graph_id
            build_graph_queue = $resourceReflectionEntry.build_graph_queue
            build_graph_inputs = @($resourceReflectionEntry.build_graph_inputs)
            build_graph_outputs = @($resourceReflectionEntry.build_graph_outputs)
            distribution_channel_id = $resourceReflectionEntry.distribution_channel_id
            distribution_channel_kind = $resourceReflectionEntry.distribution_channel_kind
            distribution_approval_policy = $resourceReflectionEntry.distribution_approval_policy
            distribution_artifact_roots = @($resourceReflectionEntry.distribution_artifact_roots)
        }
        kernel = [ordered]@{
            id = $resourceReflectionEntry.gpu_kernel_id
            stage = $resourceReflectionEntry.gpu_kernel_stage
            entry = $resourceReflectionEntry.gpu_kernel_entry
            tensor_role = $resourceReflectionEntry.gpu_kernel_tensor_role
            dispatch_shape = @($resourceReflectionEntry.gpu_kernel_dispatch_shape)
            consumes = @($resourceReflectionEntry.gpu_kernel_consumes)
            produces = @($resourceReflectionEntry.gpu_kernel_produces)
        }
        contracts = @(
            $resourceReflectionContractEntries | ForEach-Object {
                [ordered]@{
                    contract_id = $_.contract_id
                    source_path = $_.source_path
                }
            }
        )
        linked_runtime_contract_entries = @($resourceReflectionContractCatalogEntries)
        linked_gpu_catalog_entries = @($resourceReflectionGpuCatalogEntries)
        artifact_roots = @($resourceReflectionEntry.artifact_roots)
    })
}

$resourceReflectionCatalog = [ordered]@{}
$resourceReflectionCatalog["catalog_id"] = $resourceReflectionCatalogId
$resourceReflectionCatalog["catalog_scope"] = $resourceReflectionCatalogScope
$resourceReflectionCatalog["tensor_pipeline_id"] = $resourceReflectionPipelineId
$resourceReflectionCatalog["manifest_source"] = "src-kain/stdlib/three_d_runtime/resource_reflection_runtime.kn"
$resourceReflectionCatalog["entry_count"] = $resourceReflectionEntries.Count
$resourceReflectionCatalog["entries"] = $resourceReflectionEntries.ToArray()
$resourceReflectionCatalog["descriptor_count"] = $resourceReflectionDescriptorDocuments.Count
$resourceReflectionCatalog["descriptor_root"] = $resourceReflectionDescriptorRoot
$resourceReflectionCatalog["descriptor_paths"] = @($resourceReflectionDescriptorDocuments | ForEach-Object { $_.descriptor_path })
$resourceReflectionCatalog["indexes"] = [ordered]@{}
$resourceReflectionCatalog["artifact_roots"] = @(
    "generated/resource-reflection",
    "generated/resource-reflection/descriptors",
    "generated/distribution/resource-reflection",
    "generated/runtime-reflection/contracts",
    "generated/runtime-reflection/gpu"
)
$resourceReflectionCatalog["reflection_catalogs"] = @(
    "runtime_contract_catalog",
    "gpu_reflection_catalog",
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
$resourceReflectionCatalog["linked_build_graph_id"] = $resourceReflectionBuildGraphId
$resourceReflectionCatalog["linked_distribution_channel_id"] = $resourceReflectionChannelId
$resourceReflectionCatalog["linked_tensor_pipeline_id"] = $resourceReflectionPipelineId
$resourceReflectionCatalog["linked_kernel_id"] = $resourceReflectionKernelMetadata.id
$resourceReflectionCatalog["linked_contract_ids"] = @($resourceReflectionContractEntries | ForEach-Object { $_.contract_id })
$resourceReflectionCatalog["linked_contract_paths"] = @($resourceReflectionContractEntries | ForEach-Object { $_.source_path })
$resourceReflectionCatalog["linked_runtime_contract_entries"] = @($resourceReflectionContractCatalogEntries)
$resourceReflectionCatalog["linked_gpu_catalog_entries"] = @($resourceReflectionGpuCatalogEntries)
Write-Host "Building resource reflection indexes"
$resourceReflectionCatalog["indexes"]["by_descriptor_id"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.descriptor_id } -ValueSelector { param($entry) $entry.descriptor_kind }
$resourceReflectionCatalog["indexes"]["by_descriptor_kind"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.descriptor_kind } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_descriptor_scope"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.descriptor_scope } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_policy_name"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.policy_name } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_policy_value"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.policy_value } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_tensor_pipeline"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.tensor_pipeline_id } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_tensor_pipeline_pass"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) @($entry.tensor_pipeline_pass_ids) } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_build_graph"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.build_graph_id } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_build_graph_queue"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.build_graph_queue } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_distribution_channel"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.distribution_channel_id } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_distribution_channel_kind"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.distribution_channel_kind } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_kernel"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.gpu_kernel_id } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_kernel_stage"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.gpu_kernel_stage } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_kernel_tensor_role"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.gpu_kernel_tensor_role } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_contract_id"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) @($entry.linked_contract_ids) } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_contract_path"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) @($entry.linked_contract_paths) } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_export_root"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.export_root } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_descriptor_path"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.descriptor_path } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_artifact_root"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) @($entry.artifact_roots) } -ValueSelector { param($entry) $entry.descriptor_id }

foreach ($runtimeCompatibilityDescriptorDocument in $runtimeCompatibilityDescriptorDocuments) {
    Write-JsonFile -Path (Join-Path $templateRoot $runtimeCompatibilityDescriptorDocument.descriptor_path) -Data $runtimeCompatibilityDescriptorDocument
}
Write-JsonFile -Path (Join-Path $generatedRoot "runtime-compatibility\catalog.json") -Data $runtimeCompatibilityCatalog
foreach ($resourceReflectionDescriptorDocument in $resourceReflectionDescriptorDocuments) {
    Write-JsonFile -Path (Join-Path $templateRoot $resourceReflectionDescriptorDocument.descriptor_path) -Data $resourceReflectionDescriptorDocument
}

Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "launch-profiles\catalog.json") -Data $launchCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "build-graphs\catalog.json") -Data $buildGraphCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "distribution\catalog.json") -Data $distributionCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "jobs-receipt-schemas\catalog.json") -Data $jobsReceiptSchemaCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "jobs-receipt-templates\catalog.json") -Data $jobsReceiptTemplateCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "jobs-retry-ledgers\catalog.json") -Data $jobsRetryLedgerCatalog
Write-JsonFile -Path (Join-Path $templateRoot "generated\resource-reflection\catalog.json") -Data $resourceReflectionCatalog

Write-Host "Updated runtime reflection catalogs: launch-profiles, build-graphs, distribution, jobs-receipt-schemas, jobs-receipt-templates, jobs-retry-ledgers, runtime-compatibility, resource-reflection"
} catch {
    Write-Host ("Generator failed on line " + $_.InvocationInfo.ScriptLineNumber)
    Write-Host $_.InvocationInfo.PositionMessage
    throw
}

