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

function Write-TextFile {
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (!(Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }

    Set-Content -Path $Path -Value $Content
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

    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($PropertyName)) {
            return $Object[$PropertyName]
        }

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

function New-DescriptorDocument {
    param(
        [string]$DescriptorDocumentId,
        [string]$DescriptorPath,
        [string]$DescriptorKind,
        [string]$DescriptorId,
        [string]$DescriptorScope,
        [object]$Policy,
        [object]$RuntimeLinks,
        [object]$CatalogFields
    )

    $document = [ordered]@{
        descriptor_document_id = $DescriptorDocumentId
        descriptor_path = $DescriptorPath
        descriptor_kind = $DescriptorKind
        descriptor_id = $DescriptorId
        descriptor_scope = $DescriptorScope
        policy = $Policy
        runtime_links = $RuntimeLinks
    }

    foreach ($key in $CatalogFields.Keys) {
        $document[$key] = $CatalogFields[$key]
    }

    return $document
}

try {
$templateRoot = (Resolve-Path $TemplateRoot).Path
$manifestsRoot = Join-Path $templateRoot "manifests"
$generatedRoot = Join-Path $templateRoot "generated"
$runtimeReflectionRoot = Join-Path $generatedRoot "runtime-reflection"
$tensorPipelineCatalogId = "tensor_pipeline_catalog"

$runtimeApps = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "runtime_apps.json"))
$workspacePresets = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "workspace_presets.json"))
$sources = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "sources.json"))
$engineSystems = Normalize-Array -Value (Read-JsonFile -Path (Join-Path $manifestsRoot "engine_systems.json"))
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

$sourcePathsById = @{}
foreach ($source in $sources) {
    $sourcePath = Get-OptionalValue -Object $source -PropertyName "source_path"
    $sourceId = Get-OptionalValue -Object $source -PropertyName "id"
    if (-not [string]::IsNullOrWhiteSpace([string]$sourcePath) -and -not [string]::IsNullOrWhiteSpace([string]$sourceId)) {
        $sourcePathsById[[string]$sourceId] = [string]$sourcePath
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

function Resolve-SourcePath {
    param(
        [object]$Object,
        [hashtable]$SourcePathsById,
        [string]$SourceId = $null
    )

    $sourcePath = Get-OptionalValue -Object $Object -PropertyName "source_path"
    if (-not [string]::IsNullOrWhiteSpace([string]$sourcePath)) {
        return [string]$sourcePath
    }

    $resolvedSourceId = if (-not [string]::IsNullOrWhiteSpace([string]$SourceId)) {
        [string]$SourceId
    } else {
        Resolve-SourceId -Object $Object -SourceIdsByPath $sourceIdsByPath
    }

    if (-not [string]::IsNullOrWhiteSpace([string]$resolvedSourceId) -and $SourcePathsById.ContainsKey([string]$resolvedSourceId)) {
        return [string]$SourcePathsById[[string]$resolvedSourceId]
    }

    return $null
}

$gpuReflectionEntries = @(
    $gpuReflectionCatalog.entries |
        Sort-Object id |
        ForEach-Object {
            [ordered]@{
                id = $_.id
                source_id = Resolve-SourceId -Object $_ -SourceIdsByPath $sourceIdsByPath
                source_path = Resolve-SourcePath -Object $_ -SourcePathsById $sourcePathsById
                stage = $_.stage
                tensor_role = $_.tensor_role
                entry = $_.entry
                dispatch_shape = @($_.dispatch_shape)
                consumes = @($_.consumes)
                produces = @($_.produces)
            }
        }
)

$gpuReflectionCatalog | Add-Member -NotePropertyName "source_registry_catalog" -NotePropertyValue "source_registry_catalog" -Force
$gpuReflectionCatalog.entries = $gpuReflectionEntries
$gpuReflectionCatalog | Add-Member -NotePropertyName "indexes" -NotePropertyValue ([ordered]@{}) -Force
$gpuReflectionCatalog | Add-Member -NotePropertyName "kernel_count" -NotePropertyValue $gpuReflectionEntries.Count -Force
$gpuReflectionCatalog | Add-Member -NotePropertyName "entry_count" -NotePropertyValue $gpuReflectionEntries.Count -Force
$gpuReflectionCatalog | Add-Member -NotePropertyName "tensor_pipeline_id" -NotePropertyValue $tensorPipelineCatalogId -Force
$gpuReflectionCatalog.indexes["by_kernel_id"] = New-IndexMap -Entries $gpuReflectionCatalog.entries -KeySelector { param($entry) $entry.id } -ValueSelector { param($entry) $entry.id }
$gpuReflectionCatalog.indexes["by_source_id"] = New-IndexMap -Entries $gpuReflectionCatalog.entries -KeySelector { param($entry) $entry.source_id } -ValueSelector { param($entry) $entry.id }
$gpuReflectionCatalog.indexes["by_source_path"] = New-IndexMap -Entries $gpuReflectionCatalog.entries -KeySelector { param($entry) $entry.source_path } -ValueSelector { param($entry) $entry.id }
$gpuReflectionCatalog.indexes["by_stage"] = New-IndexMap -Entries $gpuReflectionCatalog.entries -KeySelector { param($entry) $entry.stage } -ValueSelector { param($entry) $entry.id }
$gpuReflectionCatalog.indexes["by_tensor_role"] = New-IndexMap -Entries $gpuReflectionCatalog.entries -KeySelector { param($entry) $entry.tensor_role } -ValueSelector { param($entry) $entry.id }
$gpuReflectionCatalog | Add-Member -NotePropertyName "descriptor_count" -NotePropertyValue 1 -Force
$gpuReflectionCatalog | Add-Member -NotePropertyName "descriptor_root" -NotePropertyValue "generated/runtime-reflection/gpu/descriptors" -Force
$gpuReflectionCatalog | Add-Member -NotePropertyName "descriptor_paths" -NotePropertyValue @("generated/runtime-reflection/gpu/descriptors/gpu_reflection_catalog.json") -Force
$gpuKernelsById = @{}
foreach ($kernel in $gpuReflectionCatalog.entries) {
    $gpuKernelsById[[string]$kernel.id] = $kernel
}
$gpuReflectionCatalog.artifact_roots = @(
    "generated/spv",
    "generated/reflection",
    "generated/runtime-reflection/gpu",
    "generated/runtime-reflection/gpu/descriptors"
)
$gpuReflectionCatalog.reflection_catalogs = @(
    "schema_reflection_catalog",
    "runtime_contract_catalog",
    "source_registry_catalog"
)

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

$workspacePresetEntries = @($launchProfileEntries.ToArray() | Sort-Object preset_id)
$workspacePresetEntriesByPresetId = @{}
foreach ($entry in $workspacePresetEntries) {
    $workspacePresetEntriesByPresetId[$entry.preset_id] = $entry
}

$workspacePresetManifestEntries = New-Object System.Collections.Generic.List[object]
foreach ($preset in ($workspacePresets | Sort-Object id)) {
    $presetId = Get-OptionalValue -Object $preset -PropertyName "id"
    if ([string]::IsNullOrWhiteSpace([string]$presetId)) {
        continue
    }

    $runtimeApp = $runtimeAppsById[(Get-OptionalValue -Object $preset -PropertyName "runtime_app_id")]
    $launchProfile = $workspacePresetEntriesByPresetId[[string]$presetId]

    $workspacePresetManifestEntries.Add([ordered]@{
        preset_id = [string]$presetId
        preset_kind = Get-OptionalValue -Object $preset -PropertyName "preset_kind"
        focus_lane = Get-OptionalValue -Object $preset -PropertyName "focus_lane"
        runtime_app_id = Get-OptionalValue -Object $preset -PropertyName "runtime_app_id"
        runtime_app_source_id = Resolve-SourceId -Object $runtimeApp -SourceIdsByPath $sourceIdsByPath
        runtime_app_namespace = Get-OptionalValue -Object $runtimeApp -PropertyName "namespace"
        runtime_kind = Get-OptionalValue -Object $runtimeApp -PropertyName "runtime_kind"
        host_kind = if ($null -ne $runtimeApp) { Get-OptionalValue -Object $runtimeApp -PropertyName "host_kind" } elseif ($null -ne $launchProfile) { Get-OptionalValue -Object $launchProfile -PropertyName "host_kind" } else { Get-OptionalValue -Object $preset -PropertyName "host_kind" }
        launch_manifest_id = if ($null -ne $launchProfile) { Get-OptionalValue -Object $launchProfile -PropertyName "manifest_id" } else { $null }
        receipt_id = if ($null -ne $launchProfile) { Get-OptionalValue -Object $launchProfile -PropertyName "receipt_id" } else { $null }
    })
}

$workspacePresetIds = @($workspacePresetEntries | ForEach-Object { $_.preset_id } | Sort-Object -Unique)
$workspacePresetLaunchTemplatePath = "generated/schemas/workspace-presets/launch-manifests/templates/workspace-preset-launch-manifest.template.json"
$workspacePresetLaunchIndexPath = "generated/schemas/workspace-presets/launch-manifests/indexes/catalog.json"
$workspacePresetReceiptTemplatePath = "generated/schemas/workspace-presets/receipts/templates/workspace-preset-delivery-receipt.template.json"
$workspacePresetReceiptIndexPath = "generated/schemas/workspace-presets/receipts/indexes/catalog.json"
$workspacePresetLaunchSchemaIndex = Read-JsonFile -Path (Join-Path $templateRoot $workspacePresetLaunchIndexPath)
$workspacePresetReceiptSchemaIndex = Read-JsonFile -Path (Join-Path $templateRoot $workspacePresetReceiptIndexPath)
$workspacePresetLaunchTemplateIds = @(Normalize-Array -Value (Get-OptionalValue -Object $workspacePresetLaunchSchemaIndex -PropertyName "template_ids"))
$workspacePresetReceiptTemplateIds = @(Normalize-Array -Value (Get-OptionalValue -Object $workspacePresetReceiptSchemaIndex -PropertyName "template_ids"))
$workspacePresetCatalogDescriptorRoot = "generated/runtime-reflection/workspace-presets/descriptors"
$workspacePresetLaunchSchemaDescriptorRoot = "generated/runtime-reflection/workspace-preset-launch-schemas/descriptors"
$workspacePresetLaunchTemplateDescriptorRoot = "generated/runtime-reflection/workspace-preset-launch-templates/descriptors"
$workspacePresetReceiptSchemaDescriptorRoot = "generated/runtime-reflection/workspace-preset-receipt-schemas/descriptors"
$workspacePresetReceiptTemplateDescriptorRoot = "generated/runtime-reflection/workspace-preset-receipt-templates/descriptors"
$workspacePresetReceiptDescriptorRoot = "generated/runtime-reflection/workspace-preset-receipts/descriptors"
$workspacePresetLaunchReceiptBindingDescriptorRoot = "generated/runtime-reflection/workspace-preset-launch-receipt-bindings/descriptors"

$workspacePresetCatalogEntries = New-Object System.Collections.Generic.List[object]
foreach ($entry in $workspacePresetEntries) {
    $workspacePresetCatalogEntries.Add([ordered]@{
        preset_id = $entry.preset_id
        preset_kind = $entry.preset_kind
        focus_lane = $entry.focus_lane
        runtime_app_id = $entry.runtime_app_id
        runtime_app_source_id = $entry.runtime_app_source_id
        runtime_app_namespace = $entry.runtime_app_namespace
        host_kind = $entry.host_kind
        runtime_kind = $entry.runtime_kind
        launch_manifest_id = $entry.manifest_id
        receipt_id = $entry.receipt_id
        schema = [ordered]@{
            launch = $entry.launch_schema_id
            receipt = "workspace_preset_delivery_receipt_schema"
        }
        artifacts = [ordered]@{
            launch_manifest = $entry.launch_manifest_path
            materialization_receipt = $entry.receipt_path
        }
    })
}

$workspacePresetCatalog = [ordered]@{}
$workspacePresetCatalog["catalog_id"] = "workspace_preset_catalog"
$workspacePresetCatalog["catalog_scope"] = "workspace_preset_focus_lane_and_host_selection_metadata"
$workspacePresetCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$workspacePresetCatalog["manifest_source"] = "manifests/workspace_presets.json"
$workspacePresetCatalog["runtime_app_manifest"] = "manifests/runtime_apps.json"
$workspacePresetCatalog["distribution_manifest"] = "manifests/distribution_channels.json"
$workspacePresetCatalog["entry_count"] = $workspacePresetCatalogEntries.Count
$workspacePresetCatalog["entries"] = $workspacePresetCatalogEntries.ToArray()
$workspacePresetCatalog["indexes"] = [ordered]@{}
$workspacePresetCatalog["artifact_roots"] = @(
    "generated/workspace-presets/launch",
    "generated/workspace-presets/receipts",
    "generated/schemas/workspace-presets/launch-manifests",
    "generated/schemas/workspace-presets/receipts",
    "generated/runtime-reflection/workspace-preset-launch-receipt-bindings"
)
$workspacePresetCatalog["reflection_catalogs"] = @(
    "workspace_preset_launch_receipt_binding_catalog",
    "workspace_preset_launch_schema_catalog",
    "workspace_preset_launch_template_catalog",
    "workspace_preset_receipt_schema_catalog",
    "workspace_preset_receipt_template_catalog",
    "workspace_preset_receipt_catalog"
)
$workspacePresetCatalog["descriptor_count"] = 1
$workspacePresetCatalog["descriptor_root"] = $workspacePresetCatalogDescriptorRoot
$workspacePresetCatalog["descriptor_paths"] = @("$workspacePresetCatalogDescriptorRoot/$($workspacePresetCatalog["catalog_id"]).json")
Write-Host "Building workspace preset indexes"
$workspacePresetCatalog["indexes"]["by_preset_id"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.preset_id } -ValueSelector { param($entry) $entry.runtime_app_id }
$workspacePresetCatalog["indexes"]["by_preset_kind"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.preset_kind } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetCatalog["indexes"]["by_focus_lane"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.focus_lane } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetCatalog["indexes"]["by_runtime_app"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.runtime_app_id } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetCatalog["indexes"]["by_runtime_app_source_id"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.runtime_app_source_id } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetCatalog["indexes"]["by_runtime_kind"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.runtime_kind } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetCatalog["indexes"]["by_host_kind"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.host_kind } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetCatalog["indexes"]["by_launch_manifest_id"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.launch_manifest_id } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetCatalog["indexes"]["by_receipt_id"] = New-IndexMap -Entries $workspacePresetCatalog.entries -KeySelector { param($entry) $entry.receipt_id } -ValueSelector { param($entry) $entry.preset_id }

$workspacePresetLaunchSchemaCatalog = [ordered]@{}
$workspacePresetLaunchSchemaCatalog["catalog_id"] = "workspace_preset_launch_schema_catalog"
$workspacePresetLaunchSchemaCatalog["catalog_scope"] = "workspace_preset_launch_manifest_schema_and_bundle_binding_metadata"
$workspacePresetLaunchSchemaCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$workspacePresetLaunchSchemaCatalog["schema_id"] = "workspace_preset_launch_manifest_schema"
$workspacePresetLaunchSchemaCatalog["document_kind"] = "workspace_preset_launch_manifest_schema_bundle"
$workspacePresetLaunchSchemaCatalog["emitter_id"] = "workspace_preset_launch_manifest_schema_emitter"
$workspacePresetLaunchSchemaCatalog["schema_root"] = "generated/schemas/workspace-presets/launch-manifests"
$workspacePresetLaunchSchemaCatalog["template_root"] = "generated/schemas/workspace-presets/launch-manifests/templates"
$workspacePresetLaunchSchemaCatalog["index_root"] = "generated/schemas/workspace-presets/launch-manifests/indexes"
$workspacePresetLaunchSchemaCatalog["artifact_index"] = $workspacePresetLaunchIndexPath
$workspacePresetLaunchSchemaCatalog["examples"] = @(
    $workspacePresetEntries | ForEach-Object {
        [ordered]@{
            preset_id = $_.preset_id
            launch_manifest = $_.launch_manifest_path
        }
    }
)
$workspacePresetLaunchSchemaCatalog["indexes"] = [ordered]@{}
$workspacePresetLaunchSchemaCatalog["artifact_roots"] = @(
    "generated/workspace-presets/launch",
    "generated/schemas/workspace-presets/launch-manifests",
    "generated/schemas/workspace-presets/launch-manifests/templates",
    "generated/schemas/workspace-presets/launch-manifests/indexes"
)
$workspacePresetLaunchSchemaCatalog["reflection_catalogs"] = @(
    "workspace_preset_launch_template_catalog",
    "workspace_preset_catalog"
)
$workspacePresetLaunchSchemaCatalog["descriptor_count"] = 1
$workspacePresetLaunchSchemaCatalog["descriptor_root"] = $workspacePresetLaunchSchemaDescriptorRoot
$workspacePresetLaunchSchemaCatalog["descriptor_paths"] = @("$workspacePresetLaunchSchemaDescriptorRoot/$($workspacePresetLaunchSchemaCatalog["catalog_id"]).json")
$workspacePresetLaunchSchemaCatalog["indexes"]["by_preset_id"] = New-IndexMap -Entries $workspacePresetLaunchSchemaCatalog.examples -KeySelector { param($entry) $entry.preset_id } -ValueSelector { param($entry) $entry.launch_manifest }

$workspacePresetLaunchTemplateCatalog = [ordered]@{}
$workspacePresetLaunchTemplateCatalog["catalog_id"] = "workspace_preset_launch_template_catalog"
$workspacePresetLaunchTemplateCatalog["catalog_scope"] = "workspace_preset_launch_manifest_template_and_index_metadata"
$workspacePresetLaunchTemplateCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$workspacePresetLaunchTemplateCatalog["templates"] = @(
    [ordered]@{
        template_id = if ($workspacePresetLaunchTemplateIds.Count -gt 0) { [string]$workspacePresetLaunchTemplateIds[0] } else { "workspace_preset_launch_manifest_template_v1" }
        schema_id = "workspace_preset_launch_manifest_schema"
        template_path = $workspacePresetLaunchTemplatePath
        index_path = $workspacePresetLaunchIndexPath
        applies_to_presets = @($workspacePresetIds)
    }
)
$workspacePresetLaunchTemplateCatalog["indexes"] = [ordered]@{}
$workspacePresetLaunchTemplateCatalog["artifact_roots"] = @(
    "generated/schemas/workspace-presets/launch-manifests/templates",
    "generated/schemas/workspace-presets/launch-manifests/indexes"
)
$workspacePresetLaunchTemplateCatalog["reflection_catalogs"] = @(
    "workspace_preset_launch_schema_catalog",
    "workspace_preset_catalog"
)
$workspacePresetLaunchTemplateCatalog["descriptor_count"] = 1
$workspacePresetLaunchTemplateCatalog["descriptor_root"] = $workspacePresetLaunchTemplateDescriptorRoot
$workspacePresetLaunchTemplateCatalog["descriptor_paths"] = @("$workspacePresetLaunchTemplateDescriptorRoot/$($workspacePresetLaunchTemplateCatalog["catalog_id"]).json")
$workspacePresetLaunchTemplateCatalog["indexes"]["by_template_id"] = New-IndexMap -Entries $workspacePresetLaunchTemplateCatalog.templates -KeySelector { param($entry) $entry.template_id } -ValueSelector { param($entry) $entry.schema_id }
$workspacePresetLaunchTemplateCatalog["indexes"]["by_preset_id"] = New-IndexMap -Entries $workspacePresetLaunchTemplateCatalog.templates -KeySelector { param($entry) @($entry.applies_to_presets) } -ValueSelector { param($entry) $entry.template_id }

$workspacePresetReceiptSchemaCatalog = [ordered]@{}
$workspacePresetReceiptSchemaCatalog["catalog_id"] = "workspace_preset_receipt_schema_catalog"
$workspacePresetReceiptSchemaCatalog["catalog_scope"] = "workspace_preset_delivery_receipt_schema_and_promotion_contract_metadata"
$workspacePresetReceiptSchemaCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$workspacePresetReceiptSchemaCatalog["schema_id"] = "workspace_preset_delivery_receipt_schema"
$workspacePresetReceiptSchemaCatalog["document_kind"] = "workspace_preset_delivery_receipt_schema_bundle"
$workspacePresetReceiptSchemaCatalog["emitter_id"] = "workspace_preset_delivery_receipt_schema_emitter"
$workspacePresetReceiptSchemaCatalog["schema_root"] = "generated/schemas/workspace-presets/receipts"
$workspacePresetReceiptSchemaCatalog["template_root"] = "generated/schemas/workspace-presets/receipts/templates"
$workspacePresetReceiptSchemaCatalog["index_root"] = "generated/schemas/workspace-presets/receipts/indexes"
$workspacePresetReceiptSchemaCatalog["artifact_index"] = $workspacePresetReceiptIndexPath
$workspacePresetReceiptSchemaCatalog["examples"] = @(
    $workspacePresetEntries | ForEach-Object {
        [ordered]@{
            receipt_id = $_.receipt_id
            receipt_path = $_.receipt_path
        }
    } | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_.receipt_id) -and -not [string]::IsNullOrWhiteSpace([string]$_.receipt_path) }
)
$workspacePresetReceiptSchemaCatalog["indexes"] = [ordered]@{}
$workspacePresetReceiptSchemaCatalog["artifact_roots"] = @(
    "generated/workspace-presets/receipts",
    "generated/schemas/workspace-presets/receipts",
    "generated/schemas/workspace-presets/receipts/templates",
    "generated/schemas/workspace-presets/receipts/indexes"
)
$workspacePresetReceiptSchemaCatalog["reflection_catalogs"] = @(
    "workspace_preset_receipt_template_catalog",
    "workspace_preset_receipt_catalog"
)
$workspacePresetReceiptSchemaCatalog["descriptor_count"] = 1
$workspacePresetReceiptSchemaCatalog["descriptor_root"] = $workspacePresetReceiptSchemaDescriptorRoot
$workspacePresetReceiptSchemaCatalog["descriptor_paths"] = @("$workspacePresetReceiptSchemaDescriptorRoot/$($workspacePresetReceiptSchemaCatalog["catalog_id"]).json")
$workspacePresetReceiptSchemaCatalog["indexes"]["by_receipt_id"] = New-IndexMap -Entries $workspacePresetReceiptSchemaCatalog.examples -KeySelector { param($entry) $entry.receipt_id } -ValueSelector { param($entry) $entry.receipt_path }

$workspacePresetReceiptTemplateCatalog = [ordered]@{}
$workspacePresetReceiptTemplateCatalog["catalog_id"] = "workspace_preset_receipt_template_catalog"
$workspacePresetReceiptTemplateCatalog["catalog_scope"] = "workspace_preset_delivery_receipt_template_and_index_metadata"
$workspacePresetReceiptTemplateCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$workspacePresetReceiptTemplateCatalog["templates"] = @(
    [ordered]@{
        template_id = if ($workspacePresetReceiptTemplateIds.Count -gt 0) { [string]$workspacePresetReceiptTemplateIds[0] } else { "workspace_preset_delivery_receipt_template_v1" }
        schema_id = "workspace_preset_delivery_receipt_schema"
        template_path = $workspacePresetReceiptTemplatePath
        index_path = $workspacePresetReceiptIndexPath
        applies_to_presets = @($workspacePresetIds)
    }
)
$workspacePresetReceiptTemplateCatalog["indexes"] = [ordered]@{}
$workspacePresetReceiptTemplateCatalog["artifact_roots"] = @(
    "generated/schemas/workspace-presets/receipts/templates",
    "generated/schemas/workspace-presets/receipts/indexes"
)
$workspacePresetReceiptTemplateCatalog["reflection_catalogs"] = @(
    "workspace_preset_receipt_schema_catalog",
    "workspace_preset_receipt_catalog"
)
$workspacePresetReceiptTemplateCatalog["descriptor_count"] = 1
$workspacePresetReceiptTemplateCatalog["descriptor_root"] = $workspacePresetReceiptTemplateDescriptorRoot
$workspacePresetReceiptTemplateCatalog["descriptor_paths"] = @("$workspacePresetReceiptTemplateDescriptorRoot/$($workspacePresetReceiptTemplateCatalog["catalog_id"]).json")
$workspacePresetReceiptTemplateCatalog["indexes"]["by_template_id"] = New-IndexMap -Entries $workspacePresetReceiptTemplateCatalog.templates -KeySelector { param($entry) $entry.template_id } -ValueSelector { param($entry) $entry.schema_id }
$workspacePresetReceiptTemplateCatalog["indexes"]["by_preset_id"] = New-IndexMap -Entries $workspacePresetReceiptTemplateCatalog.templates -KeySelector { param($entry) @($entry.applies_to_presets) } -ValueSelector { param($entry) $entry.template_id }

$workspacePresetReceiptCatalogEntries = New-Object System.Collections.Generic.List[object]
foreach ($entry in $workspacePresetEntries) {
    if ([string]::IsNullOrWhiteSpace([string]$entry.receipt_id)) {
        continue
    }

    $workspacePresetReceiptCatalogEntries.Add([ordered]@{
        receipt_id = $entry.receipt_id
        preset_id = $entry.preset_id
        focus_lane = $entry.focus_lane
        runtime_app_id = $entry.runtime_app_id
        runtime_app_source_id = $entry.runtime_app_source_id
        launch_manifest_id = $entry.manifest_id
        receipt_path = $entry.receipt_path
        schema_id = "workspace_preset_delivery_receipt_schema"
        promotion_state = $entry.receipt_promotion_state
        delivery_registry_id = $entry.delivery_registry_id
        materializer_kernel_id = "workspace_preset_launch_receipt_resolve"
        reflection_catalog_id = "workspace_preset_receipt_catalog"
    })
}

$workspacePresetReceiptCatalog = [ordered]@{}
$workspacePresetReceiptCatalog["catalog_id"] = "workspace_preset_receipt_catalog"
$workspacePresetReceiptCatalog["catalog_scope"] = "workspace_preset_materialization_receipts_and_delivery_metadata"
$workspacePresetReceiptCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$workspacePresetReceiptCatalog["materializer_kernel_id"] = "workspace_preset_launch_receipt_resolve"
$workspacePresetReceiptCatalog["delivery_batch_id"] = "workspace_preset_delivery_batch"
$workspacePresetReceiptCatalog["delivery_registry_id"] = "workspace_preset_delivery_registry"
$workspacePresetReceiptCatalog["schema_id"] = "workspace_preset_delivery_receipt_schema"
$workspacePresetReceiptCatalog["entry_count"] = $workspacePresetReceiptCatalogEntries.Count
$workspacePresetReceiptCatalog["entries"] = $workspacePresetReceiptCatalogEntries.ToArray()
$workspacePresetReceiptCatalog["indexes"] = [ordered]@{}
$workspacePresetReceiptCatalog["artifact_roots"] = @(
    "generated/workspace-presets/receipts",
    "generated/runtime-reflection/workspace-preset-receipts"
)
$workspacePresetReceiptCatalog["reflection_catalogs"] = @(
    "workspace_preset_launch_receipt_binding_catalog",
    "workspace_preset_receipt_schema_catalog",
    "workspace_preset_receipt_template_catalog"
)
$workspacePresetReceiptCatalog["descriptor_count"] = 1
$workspacePresetReceiptCatalog["descriptor_root"] = $workspacePresetReceiptDescriptorRoot
$workspacePresetReceiptCatalog["descriptor_paths"] = @("$workspacePresetReceiptDescriptorRoot/$($workspacePresetReceiptCatalog["catalog_id"]).json")
$workspacePresetReceiptCatalog["indexes"]["by_receipt_id"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.receipt_id } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetReceiptCatalog["indexes"]["by_preset_id"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.preset_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetReceiptCatalog["indexes"]["by_focus_lane"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.focus_lane } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetReceiptCatalog["indexes"]["by_runtime_app"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.runtime_app_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetReceiptCatalog["indexes"]["by_runtime_app_source_id"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.runtime_app_source_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetReceiptCatalog["indexes"]["by_launch_manifest_id"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.launch_manifest_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetReceiptCatalog["indexes"]["by_promotion_state"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.promotion_state } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetReceiptCatalog["indexes"]["by_delivery_registry"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.delivery_registry_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetReceiptCatalog["indexes"]["by_kernel"] = New-IndexMap -Entries $workspacePresetReceiptCatalog.entries -KeySelector { param($entry) $entry.materializer_kernel_id } -ValueSelector { param($entry) $entry.receipt_id }

$workspacePresetLaunchReceiptBindingEntries = New-Object System.Collections.Generic.List[object]
foreach ($entry in $workspacePresetEntries) {
    if ([string]::IsNullOrWhiteSpace([string]$entry.receipt_id)) {
        continue
    }

    $workspacePresetLaunchReceiptBindingEntries.Add([ordered]@{
        preset_id = $entry.preset_id
        focus_lane = $entry.focus_lane
        runtime_app_id = $entry.runtime_app_id
        runtime_app_source_id = $entry.runtime_app_source_id
        launch_manifest_id = $entry.manifest_id
        launch_manifest_path = $entry.launch_manifest_path
        receipt_id = $entry.receipt_id
        receipt_path = $entry.receipt_path
        launch_schema_id = "workspace_preset_launch_manifest_schema"
        launch_template_id = if ($workspacePresetLaunchTemplateIds.Count -gt 0) { [string]$workspacePresetLaunchTemplateIds[0] } else { "workspace_preset_launch_manifest_template_v1" }
        receipt_schema_id = "workspace_preset_delivery_receipt_schema"
        receipt_template_id = if ($workspacePresetReceiptTemplateIds.Count -gt 0) { [string]$workspacePresetReceiptTemplateIds[0] } else { "workspace_preset_delivery_receipt_template_v1" }
        launch_index_path = $workspacePresetLaunchIndexPath
        receipt_index_path = $workspacePresetReceiptIndexPath
    })
}

$workspacePresetLaunchReceiptBindingCatalog = [ordered]@{}
$workspacePresetLaunchReceiptBindingCatalog["catalog_id"] = "workspace_preset_launch_receipt_binding_catalog"
$workspacePresetLaunchReceiptBindingCatalog["catalog_scope"] = "workspace_preset_launch_receipt_schema_template_and_delivery_binding_metadata"
$workspacePresetLaunchReceiptBindingCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$workspacePresetLaunchReceiptBindingCatalog["materializer_kernel_id"] = "workspace_preset_launch_receipt_resolve"
$workspacePresetLaunchReceiptBindingCatalog["build_graph_id"] = "workspace_preset_materialization_graph"
$workspacePresetLaunchReceiptBindingCatalog["delivery_registry_id"] = "workspace_preset_delivery_registry"
$workspacePresetLaunchReceiptBindingCatalog["launch_schema_catalog_id"] = "workspace_preset_launch_schema_catalog"
$workspacePresetLaunchReceiptBindingCatalog["launch_template_catalog_id"] = "workspace_preset_launch_template_catalog"
$workspacePresetLaunchReceiptBindingCatalog["receipt_schema_catalog_id"] = "workspace_preset_receipt_schema_catalog"
$workspacePresetLaunchReceiptBindingCatalog["receipt_template_catalog_id"] = "workspace_preset_receipt_template_catalog"
$workspacePresetLaunchReceiptBindingCatalog["entry_count"] = $workspacePresetLaunchReceiptBindingEntries.Count
$workspacePresetLaunchReceiptBindingCatalog["entries"] = $workspacePresetLaunchReceiptBindingEntries.ToArray()
$workspacePresetLaunchReceiptBindingCatalog["indexes"] = [ordered]@{}
$workspacePresetLaunchReceiptBindingCatalog["artifact_roots"] = @(
    "generated/workspace-presets/launch",
    "generated/workspace-presets/receipts",
    "generated/schemas/workspace-presets/launch-manifests",
    "generated/schemas/workspace-presets/launch-manifests/templates",
    "generated/schemas/workspace-presets/launch-manifests/indexes",
    "generated/schemas/workspace-presets/receipts",
    "generated/schemas/workspace-presets/receipts/templates",
    "generated/schemas/workspace-presets/receipts/indexes",
    "generated/runtime-reflection/workspace-preset-launch-receipt-bindings"
)
$workspacePresetLaunchReceiptBindingCatalog["reflection_catalogs"] = @(
    "workspace_preset_catalog",
    "workspace_preset_launch_schema_catalog",
    "workspace_preset_launch_template_catalog",
    "workspace_preset_receipt_schema_catalog",
    "workspace_preset_receipt_template_catalog",
    "workspace_preset_receipt_catalog"
)
$workspacePresetLaunchReceiptBindingCatalog["descriptor_count"] = 1
$workspacePresetLaunchReceiptBindingCatalog["descriptor_root"] = $workspacePresetLaunchReceiptBindingDescriptorRoot
$workspacePresetLaunchReceiptBindingCatalog["descriptor_paths"] = @("$workspacePresetLaunchReceiptBindingDescriptorRoot/$($workspacePresetLaunchReceiptBindingCatalog["catalog_id"]).json")
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_preset_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.preset_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_focus_lane"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.focus_lane } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_runtime_app"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.runtime_app_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_runtime_app_source_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.runtime_app_source_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_launch_manifest_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.launch_manifest_id } -ValueSelector { param($entry) $entry.receipt_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_receipt_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.receipt_id } -ValueSelector { param($entry) $entry.launch_manifest_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_launch_schema_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.launch_schema_id } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_receipt_schema_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.receipt_schema_id } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_launch_template_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.launch_template_id } -ValueSelector { param($entry) $entry.preset_id }
$workspacePresetLaunchReceiptBindingCatalog["indexes"]["by_receipt_template_id"] = New-IndexMap -Entries $workspacePresetLaunchReceiptBindingCatalog.entries -KeySelector { param($entry) $entry.receipt_template_id } -ValueSelector { param($entry) $entry.preset_id }

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
$launchCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$launchCatalog["profile_id"] = "runtime_bundle_launch_profile"
$launchCatalog["manifest_source"] = "manifests/workspace_presets.json"
$launchCatalog["runtime_app_manifest"] = "manifests/runtime_apps.json"
$launchCatalog["entry_count"] = $launchProfileEntries.Count
$launchCatalog["entries"] = $launchProfileEntries.ToArray()
$launchCatalog["indexes"] = [ordered]@{}
$launchCatalog["artifact_roots"] = @(
    "generated/workspace-presets/launch",
    "generated/workspace-presets/receipts",
    "generated/runtime-reflection/launch-profiles",
    "generated/runtime-reflection/launch-profiles/descriptors"
)
$launchCatalog["reflection_catalogs"] = @(
    "workspace_preset_catalog",
    "workspace_preset_receipt_catalog",
    "distribution_receipt_catalog"
)
$launchCatalog["descriptor_root"] = "generated/runtime-reflection/launch-profiles/descriptors"
$launchCatalog["descriptor_paths"] = @("generated/runtime-reflection/launch-profiles/descriptors/$($launchCatalog["catalog_id"]).json")
$launchCatalog["descriptor_count"] = @($launchCatalog["descriptor_paths"]).Count
Write-Host "Building launch indexes"
$launchCatalog["indexes"]["by_focus_lane"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.focus_lane } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_runtime_app"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.runtime_app_id } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_runtime_app_source_id"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.runtime_app_source_id } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_host_kind"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.host_kind } -ValueSelector { param($entry) $entry.preset_id }
$launchCatalog["indexes"]["by_delivery_registry"] = New-IndexMap -Entries $launchProfileEntries -KeySelector { param($entry) $entry.delivery_registry_id } -ValueSelector { param($entry) $entry.preset_id }

$runtimeAppEntries = New-Object System.Collections.Generic.List[object]
foreach ($runtimeApp in ($runtimeApps | Sort-Object id)) {
    $runtimeAppId = Get-OptionalValue -Object $runtimeApp -PropertyName "id"
    if ([string]::IsNullOrWhiteSpace([string]$runtimeAppId)) {
        continue
    }

    $resolvedSourceId = Resolve-SourceId -Object $runtimeApp -SourceIdsByPath $sourceIdsByPath
    $outputs = New-Object System.Collections.Generic.List[object]
    foreach ($output in (Normalize-Array -Value (Get-OptionalValue -Object $runtimeApp -PropertyName "outputs"))) {
        $outputs.Add([ordered]@{
            target = Get-OptionalValue -Object $output -PropertyName "target"
            path = Get-OptionalValue -Object $output -PropertyName "path"
        })
    }

    $runtimeAppEntries.Add([ordered]@{
        runtime_app_id = [string]$runtimeAppId
        label = Get-OptionalValue -Object $runtimeApp -PropertyName "label"
        namespace = Get-OptionalValue -Object $runtimeApp -PropertyName "namespace"
        source_id = $resolvedSourceId
        source_path = Resolve-SourcePath -Object $runtimeApp -SourcePathsById $sourcePathsById -SourceId $resolvedSourceId
        host_kind = Get-OptionalValue -Object $runtimeApp -PropertyName "host_kind"
        runtime_kind = Get-OptionalValue -Object $runtimeApp -PropertyName "runtime_kind"
        output_targets = @(
            $outputs.ToArray() |
                ForEach-Object { $_.target } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        outputs = @($outputs.ToArray())
    })
}

$runtimeAppCatalog = [ordered]@{}
$runtimeAppCatalog["catalog_id"] = "runtime_app_catalog"
$runtimeAppCatalog["catalog_scope"] = "runtime_app_host_runtime_output_metadata"
$runtimeAppCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$runtimeAppCatalog["manifest_source"] = "manifests/runtime_apps.json"
$runtimeAppCatalog["source_registry_catalog"] = "source_registry_catalog"
$runtimeAppCatalog["entry_count"] = $runtimeAppEntries.Count
$runtimeAppCatalog["entries"] = $runtimeAppEntries.ToArray()
$runtimeAppCatalog["indexes"] = [ordered]@{}
$runtimeAppCatalog["artifact_roots"] = @(
    "generated/runtime-reflection/runtime-apps",
    "generated/runtime-reflection/runtime-apps/descriptors"
)
$runtimeAppCatalog["reflection_catalogs"] = @(
    "source_registry_catalog",
    "launch_profile_catalog",
    "workspace_preset_catalog"
)
$runtimeAppCatalog["descriptor_count"] = 1
$runtimeAppCatalog["descriptor_root"] = "generated/runtime-reflection/runtime-apps/descriptors"
$runtimeAppCatalog["descriptor_paths"] = @("generated/runtime-reflection/runtime-apps/descriptors/runtime_app_catalog.json")
$runtimeAppCatalog["indexes"]["by_runtime_app_id"] = New-IndexMap -Entries $runtimeAppCatalog.entries -KeySelector { param($entry) $entry.runtime_app_id } -ValueSelector { param($entry) $entry.source_id }
$runtimeAppCatalog["indexes"]["by_source_id"] = New-IndexMap -Entries $runtimeAppCatalog.entries -KeySelector { param($entry) $entry.source_id } -ValueSelector { param($entry) $entry.runtime_app_id }
$runtimeAppCatalog["indexes"]["by_namespace"] = New-IndexMap -Entries $runtimeAppCatalog.entries -KeySelector { param($entry) $entry.namespace } -ValueSelector { param($entry) $entry.runtime_app_id }
$runtimeAppCatalog["indexes"]["by_host_kind"] = New-IndexMap -Entries $runtimeAppCatalog.entries -KeySelector { param($entry) $entry.host_kind } -ValueSelector { param($entry) $entry.runtime_app_id }
$runtimeAppCatalog["indexes"]["by_runtime_kind"] = New-IndexMap -Entries $runtimeAppCatalog.entries -KeySelector { param($entry) $entry.runtime_kind } -ValueSelector { param($entry) $entry.runtime_app_id }
$runtimeAppCatalog["indexes"]["by_output_target"] = New-IndexMap -Entries $runtimeAppCatalog.entries -KeySelector { param($entry) $entry.output_targets } -ValueSelector { param($entry) $entry.runtime_app_id }

$engineSystemEntries = New-Object System.Collections.Generic.List[object]
foreach ($engineSystem in ($engineSystems | Sort-Object id)) {
    $engineSystemId = Get-OptionalValue -Object $engineSystem -PropertyName "id"
    if ([string]::IsNullOrWhiteSpace([string]$engineSystemId)) {
        continue
    }

    $resolvedSourceId = Resolve-SourceId -Object $engineSystem -SourceIdsByPath $sourceIdsByPath
    $matchingRuntimeApps = @(
        $runtimeApps |
            Where-Object { (Resolve-SourceId -Object $_ -SourceIdsByPath $sourceIdsByPath) -eq $resolvedSourceId } |
            Sort-Object id
    )
    $matchingWorkspacePresets = @(
        $workspacePresetManifestEntries |
            Where-Object { (Get-OptionalValue -Object $_ -PropertyName "runtime_app_source_id") -eq $resolvedSourceId } |
            Sort-Object { Get-OptionalValue -Object $_ -PropertyName "preset_id" }
    )

    $engineSystemEntries.Add([ordered]@{
        engine_system_id = [string]$engineSystemId
        label = Get-OptionalValue -Object $engineSystem -PropertyName "label"
        lane = Get-OptionalValue -Object $engineSystem -PropertyName "lane"
        description = Get-OptionalValue -Object $engineSystem -PropertyName "description"
        required = [bool](Get-OptionalValue -Object $engineSystem -PropertyName "required")
        source_id = $resolvedSourceId
        source_path = Resolve-SourcePath -Object $engineSystem -SourcePathsById $sourcePathsById -SourceId $resolvedSourceId
        runtime_app_count = @($matchingRuntimeApps).Count
        runtime_app_ids = @($matchingRuntimeApps | ForEach-Object { $_.id })
        workspace_preset_count = @($matchingWorkspacePresets).Count
        workspace_preset_ids = @($matchingWorkspacePresets | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "preset_id" })
    })
}

$engineSystemCatalog = [ordered]@{}
$engineSystemCatalog["catalog_id"] = "engine_system_catalog"
$engineSystemCatalog["catalog_scope"] = "engine_system_lane_and_source_projection_metadata"
$engineSystemCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$engineSystemCatalog["manifest_source"] = "manifests/engine_systems.json"
$engineSystemCatalog["source_registry_catalog"] = "source_registry_catalog"
$engineSystemCatalog["runtime_app_manifest"] = "manifests/runtime_apps.json"
$engineSystemCatalog["workspace_preset_manifest"] = "manifests/workspace_presets.json"
$engineSystemCatalog["entry_count"] = $engineSystemEntries.Count
$engineSystemCatalog["entries"] = $engineSystemEntries.ToArray()
$engineSystemCatalog["indexes"] = [ordered]@{}
$engineSystemCatalog["artifact_roots"] = @(
    "generated/runtime-reflection/engine-systems",
    "generated/runtime-reflection/engine-systems/descriptors"
)
$engineSystemCatalog["reflection_catalogs"] = @(
    "source_registry_catalog",
    "runtime_app_catalog",
    "workspace_preset_catalog"
)
$engineSystemCatalog["descriptor_count"] = 1
$engineSystemCatalog["descriptor_root"] = "generated/runtime-reflection/engine-systems/descriptors"
$engineSystemCatalog["descriptor_paths"] = @("generated/runtime-reflection/engine-systems/descriptors/engine_system_catalog.json")
Write-Host "Building engine-system indexes"
$engineSystemCatalog["indexes"]["by_engine_system_id"] = New-IndexMap -Entries $engineSystemCatalog.entries -KeySelector { param($entry) $entry.engine_system_id } -ValueSelector { param($entry) $entry.source_id }
$engineSystemCatalog["indexes"]["by_lane"] = New-IndexMap -Entries $engineSystemCatalog.entries -KeySelector { param($entry) $entry.lane } -ValueSelector { param($entry) $entry.engine_system_id }
$engineSystemCatalog["indexes"]["by_source_id"] = New-IndexMap -Entries $engineSystemCatalog.entries -KeySelector { param($entry) $entry.source_id } -ValueSelector { param($entry) $entry.engine_system_id }
$engineSystemCatalog["indexes"]["by_required"] = New-IndexMap -Entries $engineSystemCatalog.entries -KeySelector { param($entry) $entry.required } -ValueSelector { param($entry) $entry.engine_system_id }
$engineSystemCatalog["indexes"]["by_runtime_app_id"] = New-IndexMap -Entries $engineSystemCatalog.entries -KeySelector { param($entry) @($entry.runtime_app_ids) } -ValueSelector { param($entry) $entry.engine_system_id }
$engineSystemCatalog["indexes"]["by_workspace_preset_id"] = New-IndexMap -Entries $engineSystemCatalog.entries -KeySelector { param($entry) @($entry.workspace_preset_ids) } -ValueSelector { param($entry) $entry.engine_system_id }

$sourceRegistryEntries = New-Object System.Collections.Generic.List[object]
foreach ($source in ($sources | Sort-Object id)) {
    $sourceId = Get-OptionalValue -Object $source -PropertyName "id"
    if ([string]::IsNullOrWhiteSpace([string]$sourceId)) {
        continue
    }

    $matchingRuntimeApps = @(
        $runtimeApps |
            Where-Object { (Resolve-SourceId -Object $_ -SourceIdsByPath $sourceIdsByPath) -eq $sourceId } |
            Sort-Object id
    )

$matchingWorkspacePresets = @(
        $workspacePresetManifestEntries |
            Where-Object { (Get-OptionalValue -Object $_ -PropertyName "runtime_app_source_id") -eq $sourceId } |
            Sort-Object { Get-OptionalValue -Object $_ -PropertyName "preset_id" }
    )

    $runtimeAppHostKinds = @($matchingRuntimeApps | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "host_kind" } | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique)
    $runtimeAppRuntimeKinds = @($matchingRuntimeApps | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "runtime_kind" } | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique)
    $runtimeAppOutputTargets = @(
        $matchingRuntimeApps |
            ForEach-Object { @($_.outputs) | ForEach-Object { $_.target } } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
    $workspacePresetHostKinds = @($matchingWorkspacePresets | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "host_kind" } | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique)
    $workspacePresetRuntimeKinds = @($matchingWorkspacePresets | ForEach-Object {
        $workspacePresetRuntimeApp = $runtimeAppsById[(Get-OptionalValue -Object $_ -PropertyName "runtime_app_id")]
        if ($null -ne $workspacePresetRuntimeApp) {
            Get-OptionalValue -Object $workspacePresetRuntimeApp -PropertyName "runtime_kind"
        } else {
            $null
        }
    } | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique)
    $projectionHostKinds = @($runtimeAppHostKinds + $workspacePresetHostKinds | Sort-Object -Unique)
    $projectionRuntimeKinds = @($runtimeAppRuntimeKinds + $workspacePresetRuntimeKinds | Sort-Object -Unique)

    $sourceRegistryEntries.Add([ordered]@{
        source_id = $sourceId
        source_path = Get-OptionalValue -Object $source -PropertyName "source_path"
        label = Get-OptionalValue -Object $source -PropertyName "label"
        domain = Get-OptionalValue -Object $source -PropertyName "domain"
        target = Get-OptionalValue -Object $source -PropertyName "target"
        runtime_app_count = @($matchingRuntimeApps).Count
        runtime_app_ids = @($matchingRuntimeApps | ForEach-Object { $_.id })
        runtime_app_labels = @($matchingRuntimeApps | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "label" })
        runtime_app_namespaces = @($matchingRuntimeApps | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "namespace" })
        runtime_app_host_kinds = $runtimeAppHostKinds
        runtime_app_runtime_kinds = $runtimeAppRuntimeKinds
        runtime_app_output_targets = $runtimeAppOutputTargets
        workspace_preset_count = @($matchingWorkspacePresets).Count
        workspace_preset_ids = @($matchingWorkspacePresets | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "preset_id" })
        workspace_preset_focus_lanes = @($matchingWorkspacePresets | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "focus_lane" })
        workspace_preset_runtime_app_ids = @($matchingWorkspacePresets | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "runtime_app_id" })
        workspace_preset_host_kinds = $workspacePresetHostKinds
        workspace_preset_runtime_kinds = $workspacePresetRuntimeKinds
        workspace_preset_launch_manifest_ids = @($matchingWorkspacePresets | Where-Object { -not [string]::IsNullOrWhiteSpace([string](Get-OptionalValue -Object $_ -PropertyName "launch_manifest_id")) } | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "launch_manifest_id" })
        workspace_preset_receipt_ids = @($matchingWorkspacePresets | Where-Object { -not [string]::IsNullOrWhiteSpace([string](Get-OptionalValue -Object $_ -PropertyName "receipt_id")) } | ForEach-Object { Get-OptionalValue -Object $_ -PropertyName "receipt_id" })
        projection_count = @($matchingRuntimeApps).Count + @($matchingWorkspacePresets).Count
        projection_host_kinds = $projectionHostKinds
        projection_runtime_kinds = $projectionRuntimeKinds
        has_runtime_app_projection = (@($matchingRuntimeApps).Count -gt 0)
        has_workspace_preset_projection = (@($matchingWorkspacePresets).Count -gt 0)
    })
}

$sourceRegistryCatalog = [ordered]@{}
$sourceRegistryCatalog["catalog_id"] = "source_registry_catalog"
$sourceRegistryCatalog["catalog_scope"] = "shared_source_registry_and_projection_metadata"
$sourceRegistryCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$sourceRegistryCatalog["manifest_source"] = "manifests/sources.json"
$sourceRegistryCatalog["runtime_app_manifest"] = "manifests/runtime_apps.json"
$sourceRegistryCatalog["workspace_preset_manifest"] = "manifests/workspace_presets.json"
$sourceRegistryCatalog["entry_count"] = $sourceRegistryEntries.Count
$sourceRegistryCatalog["entries"] = $sourceRegistryEntries.ToArray()
$sourceRegistryCatalog["indexes"] = [ordered]@{}
$sourceRegistryDescriptorRoot = "generated/runtime-reflection/source-registry/descriptors"
$sourceRegistryCatalog["artifact_roots"] = @(
    "generated/runtime-reflection/source-registry",
    $sourceRegistryDescriptorRoot,
    "generated/runtime-reflection/launch-profiles",
    "generated/runtime-reflection/workspace-presets"
)
$sourceRegistryCatalog["reflection_catalogs"] = @(
    "launch_profile_catalog",
    "workspace_preset_catalog"
)
$sourceRegistryDescriptorDocument = New-DescriptorDocument -DescriptorDocumentId "source_registry_catalog_descriptor_document" -DescriptorPath "$sourceRegistryDescriptorRoot/source_registry_catalog.json" -DescriptorKind "source_registry" -DescriptorId "source_registry_catalog" -DescriptorScope "shared_source_registry_and_projection_metadata" -Policy ([ordered]@{ name = "catalog_scope"; value = "shared_source_registry_and_projection_metadata" }) -RuntimeLinks ([ordered]@{ tensor_pipeline_id = $tensorPipelineCatalogId }) -CatalogFields ([ordered]@{
    catalog_id = $sourceRegistryCatalog.catalog_id
    catalog_scope = $sourceRegistryCatalog.catalog_scope
    manifest_source = $sourceRegistryCatalog.manifest_source
    runtime_app_manifest = $sourceRegistryCatalog.runtime_app_manifest
    workspace_preset_manifest = $sourceRegistryCatalog.workspace_preset_manifest
    entry_count = $sourceRegistryCatalog.entry_count
    artifact_roots = @(
        "generated/runtime-reflection/source-registry",
        $sourceRegistryDescriptorRoot,
        "generated/runtime-reflection/launch-profiles",
        "generated/runtime-reflection/workspace-presets"
    )
    reflection_catalogs = @(
        "launch_profile_catalog",
        "workspace_preset_catalog"
    )
    index_names = @(
        "by_source_id",
        "by_source_path",
        "by_domain",
        "by_target",
        "by_runtime_app_id",
        "by_focus_lane",
        "by_host_kind",
        "by_runtime_kind",
        "by_preset_id",
        "by_workspace_preset_runtime_app_id",
        "by_workspace_preset_launch_manifest_id",
        "by_workspace_preset_receipt_id"
    )
})
Write-Host "Building source registry indexes"
$sourceRegistryCatalog["indexes"]["by_source_id"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) $entry.source_id } -ValueSelector { param($entry) $entry.source_path }
$sourceRegistryCatalog["indexes"]["by_source_path"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) $entry.source_path } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_domain"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) $entry.domain } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_target"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) $entry.target } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_runtime_app_id"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.runtime_app_ids) } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_focus_lane"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.workspace_preset_focus_lanes) } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_host_kind"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.projection_host_kinds) } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_runtime_kind"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.projection_runtime_kinds) } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_preset_id"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.workspace_preset_ids) } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_workspace_preset_runtime_app_id"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.workspace_preset_runtime_app_ids) } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_workspace_preset_launch_manifest_id"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.workspace_preset_launch_manifest_ids) } -ValueSelector { param($entry) $entry.source_id }
$sourceRegistryCatalog["indexes"]["by_workspace_preset_receipt_id"] = New-IndexMap -Entries $sourceRegistryEntries -KeySelector { param($entry) @($entry.workspace_preset_receipt_ids) } -ValueSelector { param($entry) $entry.source_id }

$engineSystemDescriptorDocument = New-DescriptorDocument -DescriptorDocumentId "engine_system_catalog_descriptor_document" -DescriptorPath "generated/runtime-reflection/engine-systems/descriptors/engine_system_catalog.json" -DescriptorKind "engine_system" -DescriptorId "engine_system_catalog" -DescriptorScope "engine_system_lane_and_source_projection_metadata" -Policy ([ordered]@{ name = "catalog_scope"; value = "engine_system_lane_and_source_projection_metadata" }) -RuntimeLinks ([ordered]@{
    tensor_pipeline_id = $tensorPipelineCatalogId
    source_registry_catalog_id = "source_registry_catalog"
    runtime_app_catalog_id = "runtime_app_catalog"
    workspace_preset_catalog_id = "workspace_preset_catalog"
}) -CatalogFields ([ordered]@{
    catalog_id = $engineSystemCatalog.catalog_id
    catalog_scope = $engineSystemCatalog.catalog_scope
    manifest_source = $engineSystemCatalog.manifest_source
    source_registry_catalog = $engineSystemCatalog.source_registry_catalog
    runtime_app_manifest = $engineSystemCatalog.runtime_app_manifest
    workspace_preset_manifest = $engineSystemCatalog.workspace_preset_manifest
    entry_count = $engineSystemCatalog.entry_count
    artifact_roots = @($engineSystemCatalog["artifact_roots"])
    reflection_catalogs = @($engineSystemCatalog["reflection_catalogs"])
    index_names = @(
        "by_engine_system_id",
        "by_lane",
        "by_source_id",
        "by_required",
        "by_runtime_app_id",
        "by_workspace_preset_id"
    )
})

$buildGraphCatalog = [ordered]@{}
$buildGraphCatalog["catalog_id"] = "build_graph_catalog"
$buildGraphCatalog["catalog_scope"] = "build_graph_queue_output_and_promotion_metadata"
$buildGraphCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
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
$buildGraphCatalog["descriptor_root"] = "generated/runtime-reflection/build-graphs/descriptors"
$buildGraphCatalog["descriptor_paths"] = @("generated/runtime-reflection/build-graphs/descriptors/build_graph_catalog.json")
$buildGraphCatalog["descriptor_count"] = @($buildGraphCatalog["descriptor_paths"]).Count
Write-Host "Building build-graph indexes"
$buildGraphCatalog["indexes"]["by_queue"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) $entry.queue } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_graph_kind"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) $entry.graph_kind } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_input_manifest"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) @($entry.inputs) } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_output_root"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) @($entry.outputs) } -ValueSelector { param($entry) $entry.id }
$buildGraphCatalog["indexes"]["by_distribution_channel"] = New-IndexMap -Entries $buildGraphEntries -KeySelector { param($entry) @($entry.linked_distribution_channels) } -ValueSelector { param($entry) $entry.id }

$buildGraphDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($buildGraphCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$($buildGraphCatalog["descriptor_root"])/build_graph_catalog.json" `
    -DescriptorKind "build_graph_catalog" `
    -DescriptorId $buildGraphCatalog["catalog_id"] `
    -DescriptorScope $buildGraphCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "graph_policy"
        value = $buildGraphCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $buildGraphCatalog["tensor_pipeline_id"]
        manifest_source = $buildGraphCatalog["manifest_source"]
        distribution_catalog_id = "distribution_receipt_catalog"
        runtime_contract_catalog_id = "runtime_contract_catalog"
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $buildGraphCatalog["catalog_id"]
        catalog_scope = $buildGraphCatalog["catalog_scope"]
        tensor_pipeline_id = $buildGraphCatalog["tensor_pipeline_id"]
        manifest_source = $buildGraphCatalog["manifest_source"]
        graph_count = $buildGraphCatalog["graph_count"]
        entries = @($buildGraphCatalog["entries"])
        indexes = $buildGraphCatalog["indexes"]
        artifact_roots = @($buildGraphCatalog["artifact_roots"])
        reflection_catalogs = @($buildGraphCatalog["reflection_catalogs"])
        descriptor_count = $buildGraphCatalog["descriptor_count"]
        descriptor_root = $buildGraphCatalog["descriptor_root"]
        descriptor_paths = @($buildGraphCatalog["descriptor_paths"])
        index_names = @($buildGraphCatalog["indexes"].Keys | Sort-Object)
    })

$distributionCatalog = [ordered]@{}
$distributionCatalog["catalog_id"] = "distribution_receipt_catalog"
$distributionCatalog["catalog_scope"] = "distribution_channel_delivery_and_receipt_metadata"
$distributionCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$distributionCatalog["manifest_source"] = "manifests/distribution_channels.json"
$distributionCatalog["channel_count"] = $distributionEntries.Count
$distributionCatalog["entries"] = $distributionEntries.ToArray()
$distributionCatalog["indexes"] = [ordered]@{}
$distributionCatalog["artifact_roots"] = @("generated/runtime-reflection/distribution")
$distributionCatalog["reflection_catalogs"] = @(
    "build_graph_catalog",
    "launch_profile_catalog"
)
$distributionCatalog["descriptor_root"] = "generated/runtime-reflection/distribution/descriptors"
$distributionCatalog["descriptor_paths"] = @("generated/runtime-reflection/distribution/descriptors/distribution_receipt_catalog.json")
$distributionCatalog["descriptor_count"] = @($distributionCatalog["descriptor_paths"]).Count
Write-Host "Building distribution indexes"
$distributionCatalog["indexes"]["by_channel_kind"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) $entry.channel_kind } -ValueSelector { param($entry) $entry.id }
$distributionCatalog["indexes"]["by_approval_policy"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) $entry.approval_policy } -ValueSelector { param($entry) $entry.id }
$distributionCatalog["indexes"]["by_artifact_root"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) @($entry.artifact_roots) } -ValueSelector { param($entry) $entry.id }
$distributionCatalog["indexes"]["by_build_graph"] = New-IndexMap -Entries $distributionEntries -KeySelector { param($entry) @($entry.linked_build_graphs) } -ValueSelector { param($entry) $entry.id }

$distributionDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($distributionCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$($distributionCatalog["descriptor_root"])/distribution_receipt_catalog.json" `
    -DescriptorKind "distribution_receipt_catalog" `
    -DescriptorId $distributionCatalog["catalog_id"] `
    -DescriptorScope $distributionCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "delivery_policy"
        value = $distributionCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $distributionCatalog["tensor_pipeline_id"]
        manifest_source = $distributionCatalog["manifest_source"]
        build_graph_catalog_id = "build_graph_catalog"
        launch_profile_catalog_id = "launch_profile_catalog"
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $distributionCatalog["catalog_id"]
        catalog_scope = $distributionCatalog["catalog_scope"]
        tensor_pipeline_id = $distributionCatalog["tensor_pipeline_id"]
        manifest_source = $distributionCatalog["manifest_source"]
        channel_count = $distributionCatalog["channel_count"]
        entries = @($distributionCatalog["entries"])
        indexes = $distributionCatalog["indexes"]
        artifact_roots = @($distributionCatalog["artifact_roots"])
        reflection_catalogs = @($distributionCatalog["reflection_catalogs"])
        descriptor_count = $distributionCatalog["descriptor_count"]
        descriptor_root = $distributionCatalog["descriptor_root"]
        descriptor_paths = @($distributionCatalog["descriptor_paths"])
        index_names = @($distributionCatalog["indexes"].Keys | Sort-Object)
    })

$tensorPipelineDescriptorRoot = "generated/runtime-reflection/tensor-pipelines/descriptors"
$tensorPipelineEntries = New-Object System.Collections.Generic.List[object]
foreach ($pipeline in ($tensorPipelines | Sort-Object id)) {
    $pipelineId = [string](Get-OptionalValue -Object $pipeline -PropertyName "id")
    if ([string]::IsNullOrWhiteSpace($pipelineId)) {
        continue
    }

    $passIds = @($pipeline.passes | ForEach-Object { [string]$_ } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
    $passMetadata = @(
        $passIds | ForEach-Object {
            $passId = $_
            $kernel = if ($gpuKernelsById.ContainsKey($passId)) { $gpuKernelsById[$passId] } else { $null }
            [ordered]@{
                pass_id = $passId
                source_id = if ($null -ne $kernel) { $kernel.source_id } else { $null }
                source_path = if ($null -ne $kernel) { $kernel.source_path } else { Resolve-SourcePath -Object $kernel -SourcePathsById $sourcePathsById -SourceId $kernel.source_id }
                stage = if ($null -ne $kernel) { $kernel.stage } else { $null }
                tensor_role = if ($null -ne $kernel) { $kernel.tensor_role } else { $null }
                entry = if ($null -ne $kernel) { $kernel.entry } else { $null }
                dispatch_shape = if ($null -ne $kernel) { @($kernel.dispatch_shape) } else { @() }
                consumes = if ($null -ne $kernel) { @($kernel.consumes) } else { @() }
                produces = if ($null -ne $kernel) { @($kernel.produces) } else { @() }
            }
        }
    )

    $tensorPipelineEntries.Add([pscustomobject][ordered]@{
        tensor_pipeline_id = $pipelineId
        label = Get-OptionalValue -Object $pipeline -PropertyName "label"
        domain = Get-OptionalValue -Object $pipeline -PropertyName "domain"
        priority = Get-OptionalValue -Object $pipeline -PropertyName "priority"
        residency = Get-OptionalValue -Object $pipeline -PropertyName "residency"
        pass_ids = $passIds
        pass_count = $passIds.Count
        passes = $passMetadata
        pass_source_ids = Flatten-UniqueValues -Entries $passMetadata -CollectionSelector { param($pass) @($pass.source_id) }
        pass_source_paths = Flatten-UniqueValues -Entries $passMetadata -CollectionSelector { param($pass) @($pass.source_path) }
        pass_stages = Flatten-UniqueValues -Entries $passMetadata -CollectionSelector { param($pass) @($pass.stage) }
        pass_tensor_roles = Flatten-UniqueValues -Entries $passMetadata -CollectionSelector { param($pass) @($pass.tensor_role) }
    })
}

$tensorPipelineCatalog = [ordered]@{}
$tensorPipelineCatalog["catalog_id"] = "tensor_pipeline_catalog"
$tensorPipelineCatalog["catalog_scope"] = "tensor_pipeline_dispatch_and_pass_metadata"
$tensorPipelineCatalog["tensor_pipeline_id"] = $tensorPipelineCatalogId
$tensorPipelineCatalog["manifest_source"] = "manifests/tensor_pipelines.json"
$tensorPipelineCatalog["entry_count"] = $tensorPipelineEntries.Count
$tensorPipelinePassCount = 0
foreach ($entry in $tensorPipelineEntries) {
    $tensorPipelinePassCount += (Get-OptionalValue -Object $entry -PropertyName "pass_count")
}
$tensorPipelineCatalog["pass_count"] = $tensorPipelinePassCount
$tensorPipelineCatalog["entries"] = $tensorPipelineEntries.ToArray()
$tensorPipelineCatalog["indexes"] = [ordered]@{}
$tensorPipelineCatalog["artifact_roots"] = @(
    "generated/runtime-reflection/tensor-pipelines",
    $tensorPipelineDescriptorRoot
)
$tensorPipelineCatalog["reflection_catalogs"] = @(
    "gpu_reflection_catalog",
    "source_registry_catalog"
)
$tensorPipelineCatalog["descriptor_root"] = $tensorPipelineDescriptorRoot
$tensorPipelineCatalog["descriptor_paths"] = @("$tensorPipelineDescriptorRoot/tensor_pipeline_catalog.json")
$tensorPipelineCatalog["descriptor_count"] = @($tensorPipelineCatalog["descriptor_paths"]).Count

Write-Host "Building tensor pipeline indexes"
$tensorPipelineCatalog["indexes"]["by_tensor_pipeline"] = New-IndexMap -Entries $tensorPipelineEntries -KeySelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "tensor_pipeline_id") } -ValueSelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "tensor_pipeline_id") }
$tensorPipelineCatalog["indexes"]["by_domain"] = New-IndexMap -Entries $tensorPipelineEntries -KeySelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "domain") } -ValueSelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "tensor_pipeline_id") }
$tensorPipelineCatalog["indexes"]["by_residency"] = New-IndexMap -Entries $tensorPipelineEntries -KeySelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "residency") } -ValueSelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "tensor_pipeline_id") }
$tensorPipelineCatalog["indexes"]["by_priority"] = New-IndexMap -Entries $tensorPipelineEntries -KeySelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "priority") } -ValueSelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "tensor_pipeline_id") }
$tensorPipelineCatalog["indexes"]["by_pass_id"] = New-IndexMap -Entries $tensorPipelineEntries -KeySelector { param($entry) @(Get-OptionalValue -Object $entry -PropertyName "pass_ids") } -ValueSelector { param($entry) (Get-OptionalValue -Object $entry -PropertyName "tensor_pipeline_id") }
$tensorPipelinePassEntries = @(
    $tensorPipelineEntries | ForEach-Object {
        $entry = $_
        $passes = @(Get-OptionalValue -Object $entry -PropertyName "passes")
        $pipelineId = Get-OptionalValue -Object $entry -PropertyName "tensor_pipeline_id"
        $passes | ForEach-Object {
            [ordered]@{
                tensor_pipeline_id = $pipelineId
                pass_id = $_.pass_id
                tensor_role = $_.tensor_role
                stage = $_.stage
                source_id = $_.source_id
                source_path = $_.source_path
            }
        }
    }
)
$tensorPipelineCatalog["indexes"]["by_tensor_role"] = New-IndexMap -Entries $tensorPipelinePassEntries -KeySelector { param($entry) $entry.tensor_role } -ValueSelector { param($entry) $entry.tensor_pipeline_id }
$tensorPipelineCatalog["indexes"]["by_stage"] = New-IndexMap -Entries $tensorPipelinePassEntries -KeySelector { param($entry) $entry.stage } -ValueSelector { param($entry) $entry.tensor_pipeline_id }
$tensorPipelineCatalog["indexes"]["by_pass_source_id"] = New-IndexMap -Entries $tensorPipelinePassEntries -KeySelector { param($entry) $entry.source_id } -ValueSelector { param($entry) $entry.tensor_pipeline_id }
$tensorPipelineCatalog["indexes"]["by_pass_source_path"] = New-IndexMap -Entries $tensorPipelinePassEntries -KeySelector { param($entry) $entry.source_path } -ValueSelector { param($entry) $entry.tensor_pipeline_id }

$tensorPipelineDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($tensorPipelineCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath ($tensorPipelineCatalog["descriptor_paths"][0]) `
    -DescriptorKind "tensor_pipeline_catalog" `
    -DescriptorId $tensorPipelineCatalog["catalog_id"] `
    -DescriptorScope $tensorPipelineCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "tensor_pipeline_policy"
        value = $tensorPipelineCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $tensorPipelineCatalog["tensor_pipeline_id"]
        manifest_source = $tensorPipelineCatalog["manifest_source"]
        source_registry_catalog_id = "source_registry_catalog"
        gpu_reflection_catalog_id = "gpu_reflection_catalog"
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $tensorPipelineCatalog["catalog_id"]
        catalog_scope = $tensorPipelineCatalog["catalog_scope"]
        tensor_pipeline_id = $tensorPipelineCatalog["tensor_pipeline_id"]
        manifest_source = $tensorPipelineCatalog["manifest_source"]
        entry_count = $tensorPipelineCatalog["entry_count"]
        pass_count = $tensorPipelineCatalog["pass_count"]
        entries = @($tensorPipelineCatalog["entries"])
        indexes = $tensorPipelineCatalog["indexes"]
        artifact_roots = @($tensorPipelineCatalog["artifact_roots"])
        reflection_catalogs = @($tensorPipelineCatalog["reflection_catalogs"])
        descriptor_count = $tensorPipelineCatalog["descriptor_count"]
        descriptor_root = $tensorPipelineCatalog["descriptor_root"]
        descriptor_paths = @($tensorPipelineCatalog["descriptor_paths"])
        index_names = @($tensorPipelineCatalog["indexes"].Keys | Sort-Object)
    })

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
    $runtimeAppSourceId = Resolve-SourceId -Object $runtimeApp -SourceIdsByPath $sourceIdsByPath

    if (!$runtimeCompatibilityRowsByKey.ContainsKey($rowKey)) {
        $runtimeCompatibilityRowsByKey[$rowKey] = New-Object System.Collections.Generic.List[object]
    }

    $runtimeCompatibilityRowsByKey[$rowKey].Add([ordered]@{
        runtime_app_id = $runtimeApp.id
        runtime_app_source_id = $runtimeAppSourceId
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
        source_path = Resolve-SourcePath -Object $runtimeApp -SourcePathsById $sourcePathsById -SourceId $runtimeAppSourceId
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
        source_path = if ($null -ne $kernel) { Get-OptionalValue -Object $kernel -PropertyName "source_path" } else { $null }
        stage = if ($null -ne $kernel) { Get-OptionalValue -Object $kernel -PropertyName "stage" } else { $null }
        tensor_role = if ($null -ne $kernel) { Get-OptionalValue -Object $kernel -PropertyName "tensor_role" } else { $null }
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

$terminalJobStates = @("completed", "failed", "canceled")
$nonTerminalJobStates = @("queued", "running", "retry_pending", "blocked", "paused")

$receiptExamples = @(
    $jobsReceiptEntries | ForEach-Object {
        $jobState = [string]$_.data.job_state
        $normalizedJobState = $jobState.ToLowerInvariant()
        $isTerminalState = $terminalJobStates -contains $normalizedJobState

        [ordered]@{
            receipt_id = $_.data.receipt_id
            receipt_path = $_.path
            queue_id = $_.data.queue_id
            dispatch_graph_id = $_.data.dispatch_graph_id
            distribution_channel_id = $_.data.distribution_channel_id
            retry_ledger_id = $_.data.retry_ledger_id
            job_state = $jobState
            promotion_state = $_.data.promotion_state
            lifecycle_class = if ($isTerminalState) { "terminal" } else { "in_flight" }
            artifact_kinds = @($_.data.artifacts | ForEach-Object { $_.kind })
            submitted_at = if ($null -ne $_.data.timestamps) { $_.data.timestamps.submitted_at } else { $null }
            completed_at = if ($null -ne $_.data.timestamps) { $_.data.timestamps.completed_at } else { $null }
        }
    }
)

$retryEntryExamples = @(
    $jobsRetryEntries | ForEach-Object {
        $retryData = $_.data
        $states = @()
        $resumePolicies = @()
        $jobReceiptIds = @()
        $stateTransitions = @()
        $activeReceiptIds = @()
        $terminalReceiptIds = @()
        $previousState = $null

        foreach ($ledgerEntry in @($retryData.entries)) {
            $ledgerState = if ($null -ne $ledgerEntry.state) { [string]$ledgerEntry.state } else { $null }
            $ledgerReceiptId = if ($null -ne $ledgerEntry.job_receipt_id) { [string]$ledgerEntry.job_receipt_id } else { $null }

            if (-not [string]::IsNullOrWhiteSpace($ledgerState)) {
                $states += $ledgerState
                if (-not [string]::IsNullOrWhiteSpace($previousState)) {
                    $stateTransitions += "$previousState->$ledgerState"
                }
                $normalizedLedgerState = $ledgerState.ToLowerInvariant()
                if (-not [string]::IsNullOrWhiteSpace($ledgerReceiptId)) {
                    if ($terminalJobStates -contains $normalizedLedgerState) {
                        $terminalReceiptIds += $ledgerReceiptId
                    } elseif ($nonTerminalJobStates -contains $normalizedLedgerState) {
                        $activeReceiptIds += $ledgerReceiptId
                    } else {
                        $activeReceiptIds += $ledgerReceiptId
                    }
                }
                $previousState = $ledgerState
            }

            if ($null -ne $ledgerEntry.resume_policy) { $resumePolicies += $ledgerEntry.resume_policy }
            if (-not [string]::IsNullOrWhiteSpace($ledgerReceiptId)) { $jobReceiptIds += $ledgerReceiptId }
        }

        [ordered]@{
            ledger_id = $retryData.ledger_id
            ledger_path = $_.path
            ledger_kind = $retryData.ledger_kind
            reflection_catalog_id = $retryData.reflection_catalog_id
            states = @($states | Sort-Object -Unique)
            resume_policies = @($resumePolicies | Sort-Object -Unique)
            job_receipt_ids = @($jobReceiptIds | Sort-Object -Unique)
            active_receipt_ids = @($activeReceiptIds | Sort-Object -Unique)
            terminal_receipt_ids = @($terminalReceiptIds | Sort-Object -Unique)
            state_transitions = @($stateTransitions | Sort-Object -Unique)
            latest_state = if ($null -ne $previousState) { $previousState } else { $null }
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
    "generated/runtime-reflection/jobs-receipt-schemas",
    "generated/runtime-reflection/jobs-receipt-schemas/descriptors"
)
$jobsReceiptSchemaCatalog["reflection_catalogs"] = @(
    "jobs_receipt_template_catalog",
    "jobs_retry_ledger_catalog",
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
$jobsReceiptSchemaCatalog["descriptor_count"] = 1
$jobsReceiptSchemaCatalog["descriptor_root"] = "generated/runtime-reflection/jobs-receipt-schemas/descriptors"
$jobsReceiptSchemaCatalog["descriptor_paths"] = @("generated/runtime-reflection/jobs-receipt-schemas/descriptors/jobs_receipt_schema_catalog.json")
Write-Host "Building jobs receipt-schema indexes"
$jobsReceiptSchemaCatalog["indexes"]["by_queue"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.queue_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_dispatch_graph"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.dispatch_graph_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_distribution_channel"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.distribution_channel_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_retry_ledger"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.retry_ledger_id } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_job_state"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.job_state } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_promotion_state"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.promotion_state } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_lifecycle_class"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) $entry.lifecycle_class } -ValueSelector { param($entry) $entry.receipt_id }
$jobsReceiptSchemaCatalog["indexes"]["by_artifact_kind"] = New-IndexMap -Entries $receiptExamples -KeySelector { param($entry) @($entry.artifact_kinds) } -ValueSelector { param($entry) $entry.receipt_id }
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
    "generated/runtime-reflection/jobs-receipt-templates",
    "generated/runtime-reflection/jobs-receipt-templates/descriptors"
)
$jobsReceiptTemplateCatalog["reflection_catalogs"] = @(
    "jobs_receipt_schema_catalog",
    "jobs_retry_ledger_catalog",
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
$jobsReceiptTemplateCatalog["descriptor_count"] = 1
$jobsReceiptTemplateCatalog["descriptor_root"] = "generated/runtime-reflection/jobs-receipt-templates/descriptors"
$jobsReceiptTemplateCatalog["descriptor_paths"] = @("generated/runtime-reflection/jobs-receipt-templates/descriptors/jobs_receipt_template_catalog.json")
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
            active_receipt_ids = @($_.active_receipt_ids)
            terminal_receipt_ids = @($_.terminal_receipt_ids)
            states = @($_.states)
            latest_state = $_.latest_state
            state_transitions = @($_.state_transitions)
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
    "generated/runtime-reflection/jobs-retry-ledgers",
    "generated/runtime-reflection/jobs-retry-ledgers/descriptors"
)
$jobsRetryLedgerCatalog["reflection_catalogs"] = @(
    "jobs_receipt_schema_catalog",
    "jobs_receipt_template_catalog",
    "build_graph_catalog",
    "distribution_receipt_catalog"
)
$jobsRetryLedgerCatalog["descriptor_count"] = 1
$jobsRetryLedgerCatalog["descriptor_root"] = "generated/runtime-reflection/jobs-retry-ledgers/descriptors"
$jobsRetryLedgerCatalog["descriptor_paths"] = @("generated/runtime-reflection/jobs-retry-ledgers/descriptors/jobs_retry_ledger_catalog.json")
Write-Host "Building jobs retry-ledger indexes"
$jobsRetryLedgerCatalog["indexes"]["by_retry_ledger"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.retry_ledger_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_dispatch_graph"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.dispatch_graph_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_queue"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.queue_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_delivery_registry"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.delivery_registry_id } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_state"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.states) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_latest_state"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) $entry.latest_state } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_state_transition"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.state_transitions) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_resume_policy"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.resume_policies) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_receipt_id"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.linked_receipt_ids) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_active_receipt_id"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.active_receipt_ids) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_terminal_receipt_id"] = New-IndexMap -Entries $jobsRetryLedgerCatalog.entries -KeySelector { param($entry) @($entry.terminal_receipt_ids) } -ValueSelector { param($entry) $entry.retry_ledger_id }
$jobsRetryLedgerCatalog["indexes"]["by_kernel"] = New-IndexMap -Entries $jobsPipelineKernels -KeySelector { param($entry) $entry.id } -ValueSelector { param($entry) @($jobsRetryLedgerCatalog.entries | ForEach-Object { $_.retry_ledger_id }) }

$jobsReceiptSchemaDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($jobsReceiptSchemaCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath ($jobsReceiptSchemaCatalog["descriptor_paths"][0]) `
    -DescriptorKind "jobs_receipt_schema_catalog" `
    -DescriptorId $jobsReceiptSchemaCatalog["catalog_id"] `
    -DescriptorScope $jobsReceiptSchemaCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "receipt_schema_policy"
        value = $jobsReceiptSchemaCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $jobsReceiptSchemaCatalog["tensor_pipeline_id"]
        schema_id = $jobsReceiptSchemaCatalog["schema_id"]
        queue_id = $jobsReceiptSchemaCatalog["entries"][0].queue_id
        dispatch_graph_id = $jobsReceiptSchemaCatalog["entries"][0].dispatch_graph_id
        distribution_channel_id = $jobsReceiptSchemaCatalog["entries"][0].distribution_channel_id
        retry_ledger_id = $jobsReceiptSchemaCatalog["entries"][0].retry_ledger_id
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $jobsReceiptSchemaCatalog["catalog_id"]
        catalog_scope = $jobsReceiptSchemaCatalog["catalog_scope"]
        tensor_pipeline_id = $jobsReceiptSchemaCatalog["tensor_pipeline_id"]
        schema_id = $jobsReceiptSchemaCatalog["schema_id"]
        document_kind = $jobsReceiptSchemaCatalog["document_kind"]
        schema_root = $jobsReceiptSchemaCatalog["schema_root"]
        template_root = $jobsReceiptSchemaCatalog["template_root"]
        index_root = $jobsReceiptSchemaCatalog["index_root"]
        artifact_index = $jobsReceiptSchemaCatalog["artifact_index"]
        entry_count = $jobsReceiptSchemaCatalog["entry_count"]
        entries = @($jobsReceiptSchemaCatalog["entries"])
        indexes = $jobsReceiptSchemaCatalog["indexes"]
        artifact_roots = @($jobsReceiptSchemaCatalog["artifact_roots"])
        reflection_catalogs = @($jobsReceiptSchemaCatalog["reflection_catalogs"])
        descriptor_count = $jobsReceiptSchemaCatalog["descriptor_count"]
        descriptor_root = $jobsReceiptSchemaCatalog["descriptor_root"]
        descriptor_paths = @($jobsReceiptSchemaCatalog["descriptor_paths"])
    })

$jobsReceiptTemplateDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($jobsReceiptTemplateCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath ($jobsReceiptTemplateCatalog["descriptor_paths"][0]) `
    -DescriptorKind "jobs_receipt_template_catalog" `
    -DescriptorId $jobsReceiptTemplateCatalog["catalog_id"] `
    -DescriptorScope $jobsReceiptTemplateCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "receipt_template_policy"
        value = $jobsReceiptTemplateCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $jobsReceiptTemplateCatalog["tensor_pipeline_id"]
        schema_id = $jobsReceiptTemplateCatalog["schema_id"]
        index_path = $jobsReceiptTemplateCatalog["index_path"]
        queue_id = $jobsReceiptTemplateCatalog["entries"][0].queue_id
        dispatch_graph_id = $jobsReceiptTemplateCatalog["entries"][0].dispatch_graph_id
        distribution_channel_id = $jobsReceiptTemplateCatalog["entries"][0].distribution_channel_id
        retry_ledger_id = $jobsReceiptTemplateCatalog["entries"][0].retry_ledger_id
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $jobsReceiptTemplateCatalog["catalog_id"]
        catalog_scope = $jobsReceiptTemplateCatalog["catalog_scope"]
        tensor_pipeline_id = $jobsReceiptTemplateCatalog["tensor_pipeline_id"]
        schema_id = $jobsReceiptTemplateCatalog["schema_id"]
        entry_count = $jobsReceiptTemplateCatalog["entry_count"]
        entries = @($jobsReceiptTemplateCatalog["entries"])
        indexes = $jobsReceiptTemplateCatalog["indexes"]
        template_ids = @($jobsReceiptTemplateCatalog["template_ids"])
        index_path = $jobsReceiptTemplateCatalog["index_path"]
        artifact_roots = @($jobsReceiptTemplateCatalog["artifact_roots"])
        reflection_catalogs = @($jobsReceiptTemplateCatalog["reflection_catalogs"])
        descriptor_count = $jobsReceiptTemplateCatalog["descriptor_count"]
        descriptor_root = $jobsReceiptTemplateCatalog["descriptor_root"]
        descriptor_paths = @($jobsReceiptTemplateCatalog["descriptor_paths"])
    })

$jobsRetryLedgerDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($jobsRetryLedgerCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath ($jobsRetryLedgerCatalog["descriptor_paths"][0]) `
    -DescriptorKind "jobs_retry_ledger_catalog" `
    -DescriptorId $jobsRetryLedgerCatalog["catalog_id"] `
    -DescriptorScope $jobsRetryLedgerCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "retry_ledger_policy"
        value = $jobsRetryLedgerCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $jobsRetryLedgerCatalog["tensor_pipeline_id"]
        dispatch_graph_id = $jobsRetryLedgerCatalog["entries"][0].dispatch_graph_id
        queue_id = $jobsRetryLedgerCatalog["entries"][0].queue_id
        delivery_registry_id = $jobsRetryLedgerCatalog["entries"][0].delivery_registry_id
        reflection_catalog_id = $jobsRetryLedgerCatalog["entries"][0].reflection_catalog_id
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $jobsRetryLedgerCatalog["catalog_id"]
        catalog_scope = $jobsRetryLedgerCatalog["catalog_scope"]
        tensor_pipeline_id = $jobsRetryLedgerCatalog["tensor_pipeline_id"]
        entry_count = $jobsRetryLedgerCatalog["entry_count"]
        entries = @($jobsRetryLedgerCatalog["entries"])
        indexes = $jobsRetryLedgerCatalog["indexes"]
        artifact_roots = @($jobsRetryLedgerCatalog["artifact_roots"])
        reflection_catalogs = @($jobsRetryLedgerCatalog["reflection_catalogs"])
        descriptor_count = $jobsRetryLedgerCatalog["descriptor_count"]
        descriptor_root = $jobsRetryLedgerCatalog["descriptor_root"]
        descriptor_paths = @($jobsRetryLedgerCatalog["descriptor_paths"])
    })

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
    source_id = Resolve-SourceId -Object $resourceReflectionKernel -SourceIdsByPath $sourceIdsByPath
    source_path = if ($null -ne $resourceReflectionKernel) { Get-OptionalValue -Object $resourceReflectionKernel -PropertyName "source_path" } else { "src-kain/kernels/resource/resource_reflection_catalog_resolve.kn" }
    stage = if ($null -ne $resourceReflectionKernel) { Get-OptionalValue -Object $resourceReflectionKernel -PropertyName "stage" } else { $null }
    entry = if ($null -ne $resourceReflectionKernel) { Get-OptionalValue -Object $resourceReflectionKernel -PropertyName "entry" } else { "resource_reflection_catalog_resolve" }
    tensor_role = if ($null -ne $resourceReflectionKernel) { Get-OptionalValue -Object $resourceReflectionKernel -PropertyName "tensor_role" } else { $null }
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
                source_id = $_.source_id
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
    gpu_kernel_source_id = $resourceReflectionKernelMetadata.source_id
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
    gpu_kernel_source_id = $resourceReflectionKernelMetadata.source_id
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
    gpu_kernel_source_id = $resourceReflectionKernelMetadata.source_id
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
            source_id = $resourceReflectionEntry.gpu_kernel_source_id
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
$resourceReflectionCatalog["indexes"]["by_kernel_source_id"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.gpu_kernel_source_id } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_kernel_stage"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.gpu_kernel_stage } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_kernel_tensor_role"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.gpu_kernel_tensor_role } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_contract_id"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) @($entry.linked_contract_ids) } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_contract_path"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) @($entry.linked_contract_paths) } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_export_root"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.export_root } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_descriptor_path"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) $entry.descriptor_path } -ValueSelector { param($entry) $entry.descriptor_id }
$resourceReflectionCatalog["indexes"]["by_artifact_root"] = New-IndexMap -Entries $resourceReflectionCatalog.entries -KeySelector { param($entry) @($entry.artifact_roots) } -ValueSelector { param($entry) $entry.descriptor_id }

$workspacePresetCatalogDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($workspacePresetCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$workspacePresetCatalogDescriptorRoot/$($workspacePresetCatalog["catalog_id"]).json" `
    -DescriptorKind "workspace_preset_catalog" `
    -DescriptorId $workspacePresetCatalog["catalog_id"] `
    -DescriptorScope $workspacePresetCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "catalog_policy"
        value = $workspacePresetCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $workspacePresetCatalog["tensor_pipeline_id"]
        runtime_app_manifest = $workspacePresetCatalog["runtime_app_manifest"]
        distribution_manifest = $workspacePresetCatalog["distribution_manifest"]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $workspacePresetCatalog["catalog_id"]
        catalog_scope = $workspacePresetCatalog["catalog_scope"]
        tensor_pipeline_id = $workspacePresetCatalog["tensor_pipeline_id"]
        manifest_source = $workspacePresetCatalog["manifest_source"]
        runtime_app_manifest = $workspacePresetCatalog["runtime_app_manifest"]
        distribution_manifest = $workspacePresetCatalog["distribution_manifest"]
        entry_count = $workspacePresetCatalog["entry_count"]
        entries = @($workspacePresetCatalog["entries"])
        indexes = $workspacePresetCatalog["indexes"]
        artifact_roots = @($workspacePresetCatalog["artifact_roots"])
        reflection_catalogs = @($workspacePresetCatalog["reflection_catalogs"])
        descriptor_count = $workspacePresetCatalog["descriptor_count"]
        descriptor_root = $workspacePresetCatalog["descriptor_root"]
        descriptor_paths = @($workspacePresetCatalog["descriptor_paths"])
    })

$workspacePresetLaunchSchemaDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($workspacePresetLaunchSchemaCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$workspacePresetLaunchSchemaDescriptorRoot/$($workspacePresetLaunchSchemaCatalog["catalog_id"]).json" `
    -DescriptorKind "workspace_preset_launch_schema" `
    -DescriptorId $workspacePresetLaunchSchemaCatalog["catalog_id"] `
    -DescriptorScope $workspacePresetLaunchSchemaCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "schema_policy"
        value = $workspacePresetLaunchSchemaCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $workspacePresetLaunchSchemaCatalog["tensor_pipeline_id"]
        schema_root = $workspacePresetLaunchSchemaCatalog["schema_root"]
        template_root = $workspacePresetLaunchSchemaCatalog["template_root"]
        index_root = $workspacePresetLaunchSchemaCatalog["index_root"]
        artifact_index = $workspacePresetLaunchSchemaCatalog["artifact_index"]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $workspacePresetLaunchSchemaCatalog["catalog_id"]
        catalog_scope = $workspacePresetLaunchSchemaCatalog["catalog_scope"]
        tensor_pipeline_id = $workspacePresetLaunchSchemaCatalog["tensor_pipeline_id"]
        schema_id = $workspacePresetLaunchSchemaCatalog["schema_id"]
        document_kind = $workspacePresetLaunchSchemaCatalog["document_kind"]
        emitter_id = $workspacePresetLaunchSchemaCatalog["emitter_id"]
        schema_root = $workspacePresetLaunchSchemaCatalog["schema_root"]
        template_root = $workspacePresetLaunchSchemaCatalog["template_root"]
        index_root = $workspacePresetLaunchSchemaCatalog["index_root"]
        artifact_index = $workspacePresetLaunchSchemaCatalog["artifact_index"]
        examples = @($workspacePresetLaunchSchemaCatalog["examples"])
        indexes = $workspacePresetLaunchSchemaCatalog["indexes"]
        artifact_roots = @($workspacePresetLaunchSchemaCatalog["artifact_roots"])
        reflection_catalogs = @($workspacePresetLaunchSchemaCatalog["reflection_catalogs"])
        descriptor_count = $workspacePresetLaunchSchemaCatalog["descriptor_count"]
        descriptor_root = $workspacePresetLaunchSchemaCatalog["descriptor_root"]
        descriptor_paths = @($workspacePresetLaunchSchemaCatalog["descriptor_paths"])
    })

$workspacePresetLaunchTemplateDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($workspacePresetLaunchTemplateCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$workspacePresetLaunchTemplateDescriptorRoot/$($workspacePresetLaunchTemplateCatalog["catalog_id"]).json" `
    -DescriptorKind "workspace_preset_launch_template" `
    -DescriptorId $workspacePresetLaunchTemplateCatalog["catalog_id"] `
    -DescriptorScope $workspacePresetLaunchTemplateCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "template_policy"
        value = $workspacePresetLaunchTemplateCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $workspacePresetLaunchTemplateCatalog["tensor_pipeline_id"]
        template_root = $workspacePresetLaunchTemplateCatalog["artifact_roots"][0]
        index_root = $workspacePresetLaunchTemplateCatalog["artifact_roots"][1]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $workspacePresetLaunchTemplateCatalog["catalog_id"]
        catalog_scope = $workspacePresetLaunchTemplateCatalog["catalog_scope"]
        tensor_pipeline_id = $workspacePresetLaunchTemplateCatalog["tensor_pipeline_id"]
        templates = @($workspacePresetLaunchTemplateCatalog["templates"])
        indexes = $workspacePresetLaunchTemplateCatalog["indexes"]
        artifact_roots = @($workspacePresetLaunchTemplateCatalog["artifact_roots"])
        reflection_catalogs = @($workspacePresetLaunchTemplateCatalog["reflection_catalogs"])
        descriptor_count = $workspacePresetLaunchTemplateCatalog["descriptor_count"]
        descriptor_root = $workspacePresetLaunchTemplateCatalog["descriptor_root"]
        descriptor_paths = @($workspacePresetLaunchTemplateCatalog["descriptor_paths"])
    })

$workspacePresetReceiptSchemaDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($workspacePresetReceiptSchemaCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$workspacePresetReceiptSchemaDescriptorRoot/$($workspacePresetReceiptSchemaCatalog["catalog_id"]).json" `
    -DescriptorKind "workspace_preset_receipt_schema" `
    -DescriptorId $workspacePresetReceiptSchemaCatalog["catalog_id"] `
    -DescriptorScope $workspacePresetReceiptSchemaCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "schema_policy"
        value = $workspacePresetReceiptSchemaCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $workspacePresetReceiptSchemaCatalog["tensor_pipeline_id"]
        schema_root = $workspacePresetReceiptSchemaCatalog["schema_root"]
        template_root = $workspacePresetReceiptSchemaCatalog["template_root"]
        index_root = $workspacePresetReceiptSchemaCatalog["index_root"]
        artifact_index = $workspacePresetReceiptSchemaCatalog["artifact_index"]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $workspacePresetReceiptSchemaCatalog["catalog_id"]
        catalog_scope = $workspacePresetReceiptSchemaCatalog["catalog_scope"]
        tensor_pipeline_id = $workspacePresetReceiptSchemaCatalog["tensor_pipeline_id"]
        schema_id = $workspacePresetReceiptSchemaCatalog["schema_id"]
        document_kind = $workspacePresetReceiptSchemaCatalog["document_kind"]
        emitter_id = $workspacePresetReceiptSchemaCatalog["emitter_id"]
        schema_root = $workspacePresetReceiptSchemaCatalog["schema_root"]
        template_root = $workspacePresetReceiptSchemaCatalog["template_root"]
        index_root = $workspacePresetReceiptSchemaCatalog["index_root"]
        artifact_index = $workspacePresetReceiptSchemaCatalog["artifact_index"]
        examples = @($workspacePresetReceiptSchemaCatalog["examples"])
        indexes = $workspacePresetReceiptSchemaCatalog["indexes"]
        artifact_roots = @($workspacePresetReceiptSchemaCatalog["artifact_roots"])
        reflection_catalogs = @($workspacePresetReceiptSchemaCatalog["reflection_catalogs"])
        descriptor_count = $workspacePresetReceiptSchemaCatalog["descriptor_count"]
        descriptor_root = $workspacePresetReceiptSchemaCatalog["descriptor_root"]
        descriptor_paths = @($workspacePresetReceiptSchemaCatalog["descriptor_paths"])
    })

$workspacePresetReceiptTemplateDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($workspacePresetReceiptTemplateCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$workspacePresetReceiptTemplateDescriptorRoot/$($workspacePresetReceiptTemplateCatalog["catalog_id"]).json" `
    -DescriptorKind "workspace_preset_receipt_template" `
    -DescriptorId $workspacePresetReceiptTemplateCatalog["catalog_id"] `
    -DescriptorScope $workspacePresetReceiptTemplateCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "template_policy"
        value = $workspacePresetReceiptTemplateCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $workspacePresetReceiptTemplateCatalog["tensor_pipeline_id"]
        template_root = $workspacePresetReceiptTemplateCatalog["artifact_roots"][0]
        index_root = $workspacePresetReceiptTemplateCatalog["artifact_roots"][1]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $workspacePresetReceiptTemplateCatalog["catalog_id"]
        catalog_scope = $workspacePresetReceiptTemplateCatalog["catalog_scope"]
        tensor_pipeline_id = $workspacePresetReceiptTemplateCatalog["tensor_pipeline_id"]
        templates = @($workspacePresetReceiptTemplateCatalog["templates"])
        indexes = $workspacePresetReceiptTemplateCatalog["indexes"]
        artifact_roots = @($workspacePresetReceiptTemplateCatalog["artifact_roots"])
        reflection_catalogs = @($workspacePresetReceiptTemplateCatalog["reflection_catalogs"])
        descriptor_count = $workspacePresetReceiptTemplateCatalog["descriptor_count"]
        descriptor_root = $workspacePresetReceiptTemplateCatalog["descriptor_root"]
        descriptor_paths = @($workspacePresetReceiptTemplateCatalog["descriptor_paths"])
    })

$workspacePresetReceiptDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($workspacePresetReceiptCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$workspacePresetReceiptDescriptorRoot/$($workspacePresetReceiptCatalog["catalog_id"]).json" `
    -DescriptorKind "workspace_preset_receipt" `
    -DescriptorId $workspacePresetReceiptCatalog["catalog_id"] `
    -DescriptorScope $workspacePresetReceiptCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "receipt_policy"
        value = $workspacePresetReceiptCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $workspacePresetReceiptCatalog["tensor_pipeline_id"]
        materializer_kernel_id = $workspacePresetReceiptCatalog["materializer_kernel_id"]
        delivery_batch_id = $workspacePresetReceiptCatalog["delivery_batch_id"]
        delivery_registry_id = $workspacePresetReceiptCatalog["delivery_registry_id"]
        schema_id = $workspacePresetReceiptCatalog["schema_id"]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $workspacePresetReceiptCatalog["catalog_id"]
        catalog_scope = $workspacePresetReceiptCatalog["catalog_scope"]
        tensor_pipeline_id = $workspacePresetReceiptCatalog["tensor_pipeline_id"]
        materializer_kernel_id = $workspacePresetReceiptCatalog["materializer_kernel_id"]
        delivery_batch_id = $workspacePresetReceiptCatalog["delivery_batch_id"]
        delivery_registry_id = $workspacePresetReceiptCatalog["delivery_registry_id"]
        schema_id = $workspacePresetReceiptCatalog["schema_id"]
        entry_count = $workspacePresetReceiptCatalog["entry_count"]
        entries = @($workspacePresetReceiptCatalog["entries"])
        indexes = $workspacePresetReceiptCatalog["indexes"]
        artifact_roots = @($workspacePresetReceiptCatalog["artifact_roots"])
        reflection_catalogs = @($workspacePresetReceiptCatalog["reflection_catalogs"])
        descriptor_count = $workspacePresetReceiptCatalog["descriptor_count"]
        descriptor_root = $workspacePresetReceiptCatalog["descriptor_root"]
        descriptor_paths = @($workspacePresetReceiptCatalog["descriptor_paths"])
    })

$workspacePresetLaunchReceiptBindingDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($workspacePresetLaunchReceiptBindingCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath "$workspacePresetLaunchReceiptBindingDescriptorRoot/$($workspacePresetLaunchReceiptBindingCatalog["catalog_id"]).json" `
    -DescriptorKind "workspace_preset_launch_receipt_binding" `
    -DescriptorId $workspacePresetLaunchReceiptBindingCatalog["catalog_id"] `
    -DescriptorScope $workspacePresetLaunchReceiptBindingCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "binding_policy"
        value = $workspacePresetLaunchReceiptBindingCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $workspacePresetLaunchReceiptBindingCatalog["tensor_pipeline_id"]
        materializer_kernel_id = $workspacePresetLaunchReceiptBindingCatalog["materializer_kernel_id"]
        build_graph_id = $workspacePresetLaunchReceiptBindingCatalog["build_graph_id"]
        delivery_registry_id = $workspacePresetLaunchReceiptBindingCatalog["delivery_registry_id"]
        launch_schema_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["launch_schema_catalog_id"]
        launch_template_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["launch_template_catalog_id"]
        receipt_schema_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["receipt_schema_catalog_id"]
        receipt_template_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["receipt_template_catalog_id"]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $workspacePresetLaunchReceiptBindingCatalog["catalog_id"]
        catalog_scope = $workspacePresetLaunchReceiptBindingCatalog["catalog_scope"]
        tensor_pipeline_id = $workspacePresetLaunchReceiptBindingCatalog["tensor_pipeline_id"]
        materializer_kernel_id = $workspacePresetLaunchReceiptBindingCatalog["materializer_kernel_id"]
        build_graph_id = $workspacePresetLaunchReceiptBindingCatalog["build_graph_id"]
        delivery_registry_id = $workspacePresetLaunchReceiptBindingCatalog["delivery_registry_id"]
        launch_schema_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["launch_schema_catalog_id"]
        launch_template_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["launch_template_catalog_id"]
        receipt_schema_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["receipt_schema_catalog_id"]
        receipt_template_catalog_id = $workspacePresetLaunchReceiptBindingCatalog["receipt_template_catalog_id"]
        entry_count = $workspacePresetLaunchReceiptBindingCatalog["entry_count"]
        entries = @($workspacePresetLaunchReceiptBindingCatalog["entries"])
        indexes = $workspacePresetLaunchReceiptBindingCatalog["indexes"]
        artifact_roots = @($workspacePresetLaunchReceiptBindingCatalog["artifact_roots"])
        reflection_catalogs = @($workspacePresetLaunchReceiptBindingCatalog["reflection_catalogs"])
        descriptor_count = $workspacePresetLaunchReceiptBindingCatalog["descriptor_count"]
        descriptor_root = $workspacePresetLaunchReceiptBindingCatalog["descriptor_root"]
        descriptor_paths = @($workspacePresetLaunchReceiptBindingCatalog["descriptor_paths"])
    })

$gpuReflectionDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($gpuReflectionCatalog.catalog_id)_descriptor_document" `
    -DescriptorPath "generated/runtime-reflection/gpu/descriptors/$($gpuReflectionCatalog.catalog_id).json" `
    -DescriptorKind "gpu_reflection_catalog" `
    -DescriptorId $gpuReflectionCatalog.catalog_id `
    -DescriptorScope $gpuReflectionCatalog.catalog_scope `
    -Policy ([ordered]@{
        name = "kernel_policy"
        value = $gpuReflectionCatalog.catalog_scope
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $gpuReflectionCatalog.tensor_pipeline_id
        manifest_source = $gpuReflectionCatalog.manifest_source
        source_registry_catalog_id = $gpuReflectionCatalog.source_registry_catalog
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $gpuReflectionCatalog.catalog_id
        catalog_scope = $gpuReflectionCatalog.catalog_scope
        tensor_pipeline_id = $gpuReflectionCatalog.tensor_pipeline_id
        manifest_source = $gpuReflectionCatalog.manifest_source
        source_registry_catalog = $gpuReflectionCatalog.source_registry_catalog
        kernel_count = $gpuReflectionCatalog.kernel_count
        entry_count = $gpuReflectionCatalog.entry_count
        entries = @($gpuReflectionCatalog.entries)
        indexes = $gpuReflectionCatalog.indexes
        artifact_roots = @($gpuReflectionCatalog.artifact_roots)
        reflection_catalogs = @($gpuReflectionCatalog.reflection_catalogs)
        descriptor_count = $gpuReflectionCatalog.descriptor_count
        descriptor_root = $gpuReflectionCatalog.descriptor_root
        descriptor_paths = @($gpuReflectionCatalog.descriptor_paths)
        index_names = @($gpuReflectionCatalog.indexes.Keys | Sort-Object)
    })

$runtimeAppDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($runtimeAppCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath ($runtimeAppCatalog["descriptor_paths"][0]) `
    -DescriptorKind "runtime_app_catalog" `
    -DescriptorId $runtimeAppCatalog["catalog_id"] `
    -DescriptorScope $runtimeAppCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "runtime_app_policy"
        value = $runtimeAppCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $runtimeAppCatalog["tensor_pipeline_id"]
        manifest_source = $runtimeAppCatalog["manifest_source"]
        source_registry_catalog_id = $runtimeAppCatalog["source_registry_catalog"]
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $runtimeAppCatalog["catalog_id"]
        catalog_scope = $runtimeAppCatalog["catalog_scope"]
        tensor_pipeline_id = $runtimeAppCatalog["tensor_pipeline_id"]
        manifest_source = $runtimeAppCatalog["manifest_source"]
        source_registry_catalog = $runtimeAppCatalog["source_registry_catalog"]
        entry_count = $runtimeAppCatalog["entry_count"]
        entries = @($runtimeAppCatalog["entries"])
        indexes = $runtimeAppCatalog["indexes"]
        artifact_roots = @($runtimeAppCatalog["artifact_roots"])
        reflection_catalogs = @($runtimeAppCatalog["reflection_catalogs"])
        descriptor_count = $runtimeAppCatalog["descriptor_count"]
        descriptor_root = $runtimeAppCatalog["descriptor_root"]
        descriptor_paths = @($runtimeAppCatalog["descriptor_paths"])
    })

$launchProfileDescriptorDocument = New-DescriptorDocument `
    -DescriptorDocumentId "$($launchCatalog["catalog_id"])_descriptor_document" `
    -DescriptorPath ($launchCatalog["descriptor_paths"][0]) `
    -DescriptorKind "launch_profile_catalog" `
    -DescriptorId $launchCatalog["catalog_id"] `
    -DescriptorScope $launchCatalog["catalog_scope"] `
    -Policy ([ordered]@{
        name = "launch_profile_policy"
        value = $launchCatalog["catalog_scope"]
    }) `
    -RuntimeLinks ([ordered]@{
        tensor_pipeline_id = $launchCatalog["tensor_pipeline_id"]
        manifest_source = $launchCatalog["manifest_source"]
        runtime_app_manifest = $launchCatalog["runtime_app_manifest"]
        source_registry_catalog_id = "source_registry_catalog"
        runtime_app_catalog_id = "runtime_app_catalog"
        workspace_preset_catalog_id = "workspace_preset_catalog"
        workspace_preset_receipt_catalog_id = "workspace_preset_receipt_catalog"
        distribution_receipt_catalog_id = "distribution_receipt_catalog"
    }) `
    -CatalogFields ([ordered]@{
        catalog_id = $launchCatalog["catalog_id"]
        catalog_scope = $launchCatalog["catalog_scope"]
        tensor_pipeline_id = $launchCatalog["tensor_pipeline_id"]
        profile_id = $launchCatalog["profile_id"]
        manifest_source = $launchCatalog["manifest_source"]
        runtime_app_manifest = $launchCatalog["runtime_app_manifest"]
        entry_count = $launchCatalog["entry_count"]
        entries = @($launchCatalog["entries"])
        indexes = $launchCatalog["indexes"]
        artifact_roots = @($launchCatalog["artifact_roots"])
        reflection_catalogs = @($launchCatalog["reflection_catalogs"])
        descriptor_count = $launchCatalog["descriptor_count"]
        descriptor_root = $launchCatalog["descriptor_root"]
        descriptor_paths = @($launchCatalog["descriptor_paths"])
    })

foreach ($runtimeCompatibilityDescriptorDocument in $runtimeCompatibilityDescriptorDocuments) {
    Write-JsonFile -Path (Join-Path $templateRoot $runtimeCompatibilityDescriptorDocument.descriptor_path) -Data $runtimeCompatibilityDescriptorDocument
}
Write-JsonFile -Path (Join-Path $templateRoot $gpuReflectionDescriptorDocument.descriptor_path) -Data $gpuReflectionDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $buildGraphDescriptorDocument.descriptor_path) -Data $buildGraphDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $distributionDescriptorDocument.descriptor_path) -Data $distributionDescriptorDocument
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\launch-profiles\README.md") -Content @'
# Launch Profile Catalog

This folder contains the committed launch-profile reflection snapshot generated
from the workspace preset and runtime app manifests.

The snapshot stays manifest-driven and binds each launch profile back to the
shared source registry through `runtime_app_source_id`, so downstream tools can
query preset/runtime bindings without rebuilding the join locally.

Contents:

- `catalog.json`: launch-profile metadata with focus-lane, runtime-app, host,
  delivery-registry, and source-aware indexes
- `descriptors/launch_profile_catalog.json`: committed descriptor document for
  the launch-profile catalog contract and artifact roots
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\launch-profiles\descriptors\README.md") -Content @'
# Launch Profile Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the launch
profile catalog.

The files here are generated from the same manifest-driven launch surface that
powers `generated/runtime-reflection/launch-profiles/catalog.json`. They keep
the launch, receipt, and runtime-app bindings available through a single
descriptor document.

Contents:

- `launch_profile_catalog.json`: launch-profile metadata and descriptor-rooted
  catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-compatibility\descriptors\README.md") -Content @'
# Runtime Compatibility Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshots for the
runtime-compatibility catalog.

The files here are generated from the same manifest-driven compatibility
surface that powers `generated/runtime-compatibility/catalog.json`. They are
kept alongside the catalog so downstream tools can open a single descriptor
document when they only need one compatibility view instead of the full
matrix.

Contents:

- `runtime_compatibility_matrix.json`: matrix-scoped compatibility metadata
- `runtime_compatibility_window.json`: backend/target window metadata
- `runtime_launch_readiness.json`: launch-readiness and gate metadata
- `runtime_feature_pack_windows.json`: manifest-derived feature-pack and
  budget-window tier views

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\gpu\descriptors\README.md") -Content @'
# GPU Reflection Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the GPU
kernel reflection catalog.

The files here are generated from the same manifest-driven GPU surface that
powers `generated/runtime-reflection/gpu/catalog.json`. The catalog is now
descriptor-rooted and projects `source_id` values from `manifests/sources.json`
so downstream tools can join against the shared source registry without using
`source_path` as the only lookup key.
The authored `manifests/gpu_kernels.json` file is source-id-first, so the
generator reconstructs `source_path` from the shared registry when it writes
the committed reflection snapshot.

Contents:

- `gpu_reflection_catalog.json`: GPU kernel metadata, `source_id` projections,
  and descriptor-rooted index names

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\gpu\README.md") -Content @'
# GPU Kernel Catalog

This folder contains the committed GPU kernel reflection snapshot generated
from `manifests/gpu_kernels.json`.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/gpu/descriptors` alongside `source_id`
projections resolved from `manifests/sources.json`.
The authored `manifests/gpu_kernels.json` file is source-id-first, so the
generator reconstructs `source_path` from the shared registry instead of
repeating it in the kernel manifest.

Contents:

- `catalog.json`: GPU kernel metadata with `source_id`, `source_path`,
  dispatch, tensor role, stage, artifact-root, and index metadata
- `descriptors/gpu_reflection_catalog.json`: committed descriptor document for
  the GPU kernel catalog contract and artifact roots
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\build-graphs\descriptors\README.md") -Content @'
# Build Graph Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the build
graph catalog.

The files here are generated from the same manifest-driven build-graph surface
that powers `generated/runtime-reflection/build-graphs/catalog.json`. The
catalog is now descriptor-rooted so downstream tools can inspect the queued,
graph-kind, input, output, and distribution-channel joins from a single
descriptor document instead of reopening the full catalog.

Contents:

- `build_graph_catalog.json`: build-graph metadata, index names, and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\build-graphs\README.md") -Content @'
# Build Graph Catalog

This folder contains the committed build-graph reflection snapshot generated
from `manifests/build_graphs.json`.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/build-graphs/descriptors` so downstream
tools can inspect the queue/output/distribution surface without rebuilding the
full manifest joins.

Contents:

- `catalog.json`: build-graph metadata with queue, graph-kind, input, output,
  and linked distribution-channel indexes
- `descriptors/build_graph_catalog.json`: committed descriptor document for the
  build-graph catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\distribution\descriptors\README.md") -Content @'
# Distribution Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the
distribution receipt catalog.

The files here are generated from the same manifest-driven distribution surface
that powers `generated/runtime-reflection/distribution/catalog.json`. The
catalog is now descriptor-rooted so downstream tools can inspect the channel,
approval, artifact-root, and build-graph joins from a single descriptor
document instead of reopening the full catalog.

Contents:

- `distribution_receipt_catalog.json`: distribution metadata, index names, and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\distribution\README.md") -Content @'
# Distribution Receipt Catalog

This folder contains the committed distribution-receipt snapshot generated
from `manifests/distribution_channels.json`.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/distribution/descriptors` so downstream
tools can inspect the delivery surface without rebuilding the full manifest
joins.

Contents:

- `catalog.json`: distribution-channel metadata with channel-kind,
  approval-policy, artifact-root, and linked build-graph indexes
- `descriptors/distribution_receipt_catalog.json`: committed descriptor
  document for the distribution catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "gpu\catalog.json") -Data $gpuReflectionCatalog
Write-JsonFile -Path (Join-Path $generatedRoot "runtime-compatibility\catalog.json") -Data $runtimeCompatibilityCatalog
Write-JsonFile -Path (Join-Path $templateRoot $runtimeAppDescriptorDocument.descriptor_path) -Data $runtimeAppDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $launchProfileDescriptorDocument.descriptor_path) -Data $launchProfileDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $workspacePresetCatalogDescriptorDocument.descriptor_path) -Data $workspacePresetCatalogDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $workspacePresetLaunchSchemaDescriptorDocument.descriptor_path) -Data $workspacePresetLaunchSchemaDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $workspacePresetLaunchTemplateDescriptorDocument.descriptor_path) -Data $workspacePresetLaunchTemplateDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $workspacePresetReceiptSchemaDescriptorDocument.descriptor_path) -Data $workspacePresetReceiptSchemaDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $workspacePresetReceiptTemplateDescriptorDocument.descriptor_path) -Data $workspacePresetReceiptTemplateDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $workspacePresetReceiptDescriptorDocument.descriptor_path) -Data $workspacePresetReceiptDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $workspacePresetLaunchReceiptBindingDescriptorDocument.descriptor_path) -Data $workspacePresetLaunchReceiptBindingDescriptorDocument
foreach ($resourceReflectionDescriptorDocument in $resourceReflectionDescriptorDocuments) {
    Write-JsonFile -Path (Join-Path $templateRoot $resourceReflectionDescriptorDocument.descriptor_path) -Data $resourceReflectionDescriptorDocument
}

Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "launch-profiles\catalog.json") -Data $launchCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "workspace-presets\catalog.json") -Data $workspacePresetCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "workspace-preset-launch-schemas\catalog.json") -Data $workspacePresetLaunchSchemaCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "workspace-preset-launch-templates\catalog.json") -Data $workspacePresetLaunchTemplateCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "workspace-preset-receipt-schemas\catalog.json") -Data $workspacePresetReceiptSchemaCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "workspace-preset-receipt-templates\catalog.json") -Data $workspacePresetReceiptTemplateCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "workspace-preset-receipts\catalog.json") -Data $workspacePresetReceiptCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "workspace-preset-launch-receipt-bindings\catalog.json") -Data $workspacePresetLaunchReceiptBindingCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "source-registry\catalog.json") -Data $sourceRegistryCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "runtime-apps\catalog.json") -Data $runtimeAppCatalog
Write-JsonFile -Path (Join-Path $templateRoot $engineSystemDescriptorDocument.descriptor_path) -Data $engineSystemDescriptorDocument
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "engine-systems\catalog.json") -Data $engineSystemCatalog
Write-JsonFile -Path (Join-Path $templateRoot $sourceRegistryDescriptorDocument.descriptor_path) -Data $sourceRegistryDescriptorDocument
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "build-graphs\catalog.json") -Data $buildGraphCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "distribution\catalog.json") -Data $distributionCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "jobs-receipt-schemas\catalog.json") -Data $jobsReceiptSchemaCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "jobs-receipt-templates\catalog.json") -Data $jobsReceiptTemplateCatalog
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "jobs-retry-ledgers\catalog.json") -Data $jobsRetryLedgerCatalog
Write-JsonFile -Path (Join-Path $templateRoot $jobsReceiptSchemaDescriptorDocument.descriptor_path) -Data $jobsReceiptSchemaDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $jobsReceiptTemplateDescriptorDocument.descriptor_path) -Data $jobsReceiptTemplateDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot $jobsRetryLedgerDescriptorDocument.descriptor_path) -Data $jobsRetryLedgerDescriptorDocument
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\jobs-receipt-schemas\README.md") -Content @'
# Jobs Receipt Schema Catalog

This folder contains the committed jobs receipt-schema reflection snapshot
generated from the jobs dispatch and retry manifests.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/jobs-receipt-schemas/descriptors` so
downstream tools can inspect the job receipt schema contract without reopening
only the full catalog.

Contents:

- `catalog.json`: receipt-schema metadata with queue, dispatch-graph,
  distribution-channel, retry-ledger, and tensor-pipeline indexes
- `descriptors/jobs_receipt_schema_catalog.json`: committed descriptor
  document for the receipt-schema catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\jobs-receipt-schemas\descriptors\README.md") -Content @'
# Jobs Receipt Schema Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the jobs
receipt-schema catalog.

The files here are generated from the same manifest-driven jobs surface that
powers `generated/runtime-reflection/jobs-receipt-schemas/catalog.json`.
They keep the schema, queue, dispatch-graph, distribution-channel, and
retry-ledger joins available through a single descriptor document.

Contents:

- `jobs_receipt_schema_catalog.json`: receipt-schema metadata and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\jobs-receipt-templates\README.md") -Content @'
# Jobs Receipt Template Catalog

This folder contains the committed jobs receipt-template reflection snapshot
generated from the jobs dispatch and retry manifests.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/jobs-receipt-templates/descriptors` so
downstream tools can inspect the job receipt template contract without
reopening only the full catalog.

Contents:

- `catalog.json`: receipt-template metadata with schema, index, queue,
  dispatch-graph, distribution-channel, and retry-ledger joins
- `descriptors/jobs_receipt_template_catalog.json`: committed descriptor
  document for the receipt-template catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\jobs-receipt-templates\descriptors\README.md") -Content @'
# Jobs Receipt Template Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the jobs
receipt-template catalog.

The files here are generated from the same manifest-driven jobs surface that
powers `generated/runtime-reflection/jobs-receipt-templates/catalog.json`.
They keep the schema, index, queue, dispatch-graph, distribution-channel, and
retry-ledger joins available through a single descriptor document.

Contents:

- `jobs_receipt_template_catalog.json`: receipt-template metadata and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\jobs-retry-ledgers\README.md") -Content @'
# Jobs Retry Ledger Catalog

This folder contains the committed jobs retry-ledger reflection snapshot
generated from the jobs dispatch and retry manifests.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/jobs-retry-ledgers/descriptors` so
downstream tools can inspect the retry and worker-requeue contract without
reopening only the full catalog.

Contents:

- `catalog.json`: retry-ledger metadata with queue, dispatch-graph,
  delivery-registry, state, and resume-policy indexes
- `descriptors/jobs_retry_ledger_catalog.json`: committed descriptor document
  for the retry-ledger catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\jobs-retry-ledgers\descriptors\README.md") -Content @'
# Jobs Retry Ledger Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the jobs
retry-ledger catalog.

The files here are generated from the same manifest-driven jobs surface that
powers `generated/runtime-reflection/jobs-retry-ledgers/catalog.json`. They
keep the dispatch-graph, queue, delivery-registry, receipt, and resume-policy
joins available through a single descriptor document.

Contents:

- `jobs_retry_ledger_catalog.json`: retry-ledger metadata and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\tensor-pipelines\README.md") -Content @'
# Tensor Pipeline Catalog

This folder contains the committed tensor-pipeline reflection snapshot generated
from `manifests/tensor_pipelines.json`.

The catalog stays manifest-driven and joins each tensor pipeline to its
authored passes plus resolved GPU kernel metadata where available from
`generated/runtime-reflection/gpu/catalog.json`. It keeps pipeline domain,
priority, residency, and pass metadata queryable without reopening the manifest.

Contents:

- `catalog.json`: tensor-pipeline metadata with domain, priority, residency,
  pass, tensor-role, and stage indexes
- `descriptors/tensor_pipeline_catalog.json`: committed descriptor document for
  the tensor-pipeline catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-TextFile -Path (Join-Path $templateRoot "generated\runtime-reflection\tensor-pipelines\descriptors\README.md") -Content @'
# Tensor Pipeline Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the tensor
pipeline catalog.

The files here are generated from the same manifest-driven tensor surface that
powers `generated/runtime-reflection/tensor-pipelines/catalog.json`. They keep
the domain, priority, residency, GPU-kernel, and pass metadata available
through a single descriptor document.

Contents:

- `tensor_pipeline_catalog.json`: tensor-pipeline metadata and descriptor-rooted
  catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
'@
Write-JsonFile -Path (Join-Path $runtimeReflectionRoot "tensor-pipelines\catalog.json") -Data $tensorPipelineCatalog
Write-JsonFile -Path (Join-Path $templateRoot $tensorPipelineDescriptorDocument.descriptor_path) -Data $tensorPipelineDescriptorDocument
Write-JsonFile -Path (Join-Path $templateRoot "generated\resource-reflection\catalog.json") -Data $resourceReflectionCatalog

Write-Host "Updated runtime reflection catalogs: launch-profiles, workspace-presets, workspace-preset-launch-schemas, workspace-preset-launch-templates, workspace-preset-receipt-schemas, workspace-preset-receipt-templates, workspace-preset-receipts, workspace-preset-launch-receipt-bindings, runtime-apps, engine-systems, source-registry, gpu, tensor-pipelines, build-graphs, distribution, jobs-receipt-schemas, jobs-receipt-templates, jobs-retry-ledgers, runtime-compatibility, resource-reflection"
} catch {
    Write-Host ("Generator failed on line " + $_.InvocationInfo.ScriptLineNumber)
    Write-Host $_.InvocationInfo.PositionMessage
    throw
}


