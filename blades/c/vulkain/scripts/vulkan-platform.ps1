Set-StrictMode -Version Latest

function Resolve-VulkainSdkRoot {
    param([string]$RequestedSdkRoot)

    $candidates = @()
    if ($RequestedSdkRoot) {
        $candidates += $RequestedSdkRoot
    }
    if ($env:KAIN_PLATFORM_VULKAN_SDK_ROOT) {
        $candidates += $env:KAIN_PLATFORM_VULKAN_SDK_ROOT
    }
    if ($env:VULKAN_SDK) {
        $candidates += $env:VULKAN_SDK
    }

    $sdkParent = "C:\VulkanSDK"
    if (Test-Path $sdkParent) {
        $versioned = Get-ChildItem -LiteralPath $sdkParent -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
        foreach ($dir in $versioned) {
            $candidates += $dir.FullName
        }
    }

    foreach ($candidate in $candidates) {
        if (!$candidate) {
            continue
        }
        $resolved = Resolve-PlatformPathIfPresent -Path $candidate
        if (!$resolved) {
            continue
        }
        if (
            (Test-Path (Join-Path $resolved "Include\vulkan\vulkan.h")) -and
            (Test-Path (Join-Path $resolved "Lib\vulkan-1.lib"))
        ) {
            return $resolved
        }
    }

    throw "No Vulkan SDK was found. Pass -VulkanSdk, set KAIN_PLATFORM_VULKAN_SDK_ROOT, or install a VulkanSDK under C:\VulkanSDK."
}

function Resolve-PlatformPathIfPresent {
    param([string]$Path)

    if (!$Path) {
        return $null
    }
    if (Test-Path $Path) {
        return (Resolve-Path $Path).Path
    }
    return $null
}

function Resolve-VulkainRepoRoot {
    param([string]$BladeRoot)

    return (Resolve-Path (Join-Path $BladeRoot "..\..")).Path
}

function Invoke-VulkainKain {
    param(
        [string]$RepoRoot,
        [string]$KainBin,
        [string[]]$Arguments
    )

    if ($KainBin -and (Test-Path $KainBin)) {
        & $KainBin @Arguments
    } else {
        Push-Location $RepoRoot
        try {
            & cargo run -q -p cli --bin kain -- @Arguments
        } finally {
            Pop-Location
        }
    }

    if ($LASTEXITCODE -ne 0) {
        throw "kain command failed: $($Arguments -join ' ')"
    }
}

function Convert-VulkainLockPath {
    param(
        [string]$BladeRoot,
        [string]$RawPath
    )

    if (!$RawPath) {
        return $null
    }

    if ($RawPath -eq ".") {
        return $BladeRoot
    }

    $normalized = $RawPath
    if ($normalized.StartsWith("//?/")) {
        $normalized = $normalized.Substring(4)
    }
    $normalized = $normalized -replace "/", "\"
    if ([System.IO.Path]::IsPathRooted($normalized)) {
        if (Test-Path $normalized) {
            return (Resolve-Path $normalized).Path
        }
        return $normalized
    }

    $candidate = Join-Path $BladeRoot $normalized
    if (Test-Path $candidate) {
        return (Resolve-Path $candidate).Path
    }
    return $candidate
}

function Resolve-VulkainIncludeRoot {
    param([string]$HeaderPath)

    if (!$HeaderPath) {
        return $null
    }

    $parent = Split-Path -Parent $HeaderPath
    if (!$parent) {
        return $null
    }

    $stem = [System.IO.Path]::GetFileNameWithoutExtension($HeaderPath)
    $parentLeaf = Split-Path -Leaf $parent
    if (
        $stem -and
        $parentLeaf -and
        $stem.Equals($parentLeaf, [System.StringComparison]::OrdinalIgnoreCase)
    ) {
        $grandParent = Split-Path -Parent $parent
        if ($grandParent) {
            return $grandParent
        }
    }

    return $parent
}

function Resolve-ExistingPath {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        $resolved = Resolve-PlatformPathIfPresent -Path $candidate
        if ($resolved) {
            return $resolved
        }
    }
    return $null
}

function Sync-VulkainPlatformPackage {
    param(
        [string]$BladeRoot,
        [string]$KainBin = $env:KAIN_BIN,
        [string]$VulkanSdk = $env:VULKAN_SDK
    )

    $repoRoot = Resolve-VulkainRepoRoot -BladeRoot $BladeRoot
    $sdkRoot = Resolve-VulkainSdkRoot -RequestedSdkRoot $VulkanSdk
    $lockRoot = Join-Path $BladeRoot ".kain\platform\vulkan"
    $reportPath = Join-Path $lockRoot "vulkan_report.json"

    New-Item -ItemType Directory -Force -Path $lockRoot | Out-Null

    $importArgs = @(
        "import", "platform", "vulkan",
        "--sdk", $sdkRoot,
        "--output", $lockRoot,
        "--report-json", $reportPath
    )
    $null = Invoke-VulkainKain -RepoRoot $repoRoot -KainBin $KainBin -Arguments $importArgs

    $lockPath = Join-Path $lockRoot "vulkan.lock"
    if (!(Test-Path $lockPath)) {
        throw "Expected Vulkan platform lock was not generated: $lockPath"
    }

    $lock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
    $headerPath = $null
    if ($lock.resolved_headers.Count -gt 0) {
        $headerPath = Convert-VulkainLockPath -BladeRoot $BladeRoot -RawPath $lock.resolved_headers[0].path
    }
    $includeRoot = Resolve-VulkainIncludeRoot -HeaderPath $headerPath
    $dllPath = $null
    if ($lock.resolved_libraries.Count -gt 0) {
        $dllPath = Convert-VulkainLockPath -BladeRoot $BladeRoot -RawPath $lock.resolved_libraries[0].path
    }
    $importLibPath = $null
    if ($lock.resolved_import_libraries.Count -gt 0) {
        $importLibPath = Convert-VulkainLockPath -BladeRoot $BladeRoot -RawPath $lock.resolved_import_libraries[0].path
    }
    $registryPath = $null
    if ($lock.registry_files.Count -gt 0) {
        $registryPath = Convert-VulkainLockPath -BladeRoot $BladeRoot -RawPath $lock.registry_files[0].path
    }
    $generatedModulePath = $null
    if ($lock.generated_modules.Count -gt 1) {
        $generatedModulePath = Convert-VulkainLockPath -BladeRoot $BladeRoot -RawPath $lock.generated_modules[1]
    }
    $glslangPath = Resolve-ExistingPath -Candidates @(
        (Join-Path $sdkRoot "Bin\glslangValidator.exe"),
        (Join-Path $sdkRoot "Bin\glslangValidator"),
        (Join-Path $sdkRoot "Bin32\glslangValidator.exe")
    )
    $spirvValPath = Resolve-ExistingPath -Candidates @(
        (Join-Path $sdkRoot "Bin\spirv-val.exe"),
        (Join-Path $sdkRoot "Bin\spirv-val"),
        (Join-Path $sdkRoot "Bin32\spirv-val.exe")
    )

    if (!$includeRoot) {
        throw "Vulkan platform lock did not expose a usable header/include path."
    }
    if (!$dllPath) {
        throw "Vulkan platform lock did not expose a usable Vulkan loader DLL path."
    }
    if (!$glslangPath) {
        throw "glslangValidator was not found under the resolved Vulkan SDK root: $sdkRoot"
    }
    if (!$spirvValPath) {
        throw "spirv-val was not found under the resolved Vulkan SDK root: $sdkRoot"
    }

    $env:KAIN_PLATFORM_VULKAN_LOCK = $lockPath
    $env:KAIN_PLATFORM_VULKAN_BINDING_REPORT = $reportPath
    $env:KAIN_PLATFORM_VULKAN_SDK_ROOT = $sdkRoot
    $env:KAIN_PLATFORM_VULKAN_HEADER = $headerPath
    $env:KAIN_PLATFORM_VULKAN_INCLUDE = $includeRoot
    $env:KAIN_PLATFORM_VULKAN_DLL = $dllPath
    if ($importLibPath) {
        $env:KAIN_PLATFORM_VULKAN_IMPORT_LIB = $importLibPath
    }
    if ($registryPath) {
        $env:KAIN_PLATFORM_VULKAN_REGISTRY = $registryPath
    }
    if ($generatedModulePath) {
        $env:KAIN_PLATFORM_VULKAN_GENERATED_MODULE = $generatedModulePath
    }
    $env:KAIN_PLATFORM_VULKAN_GLSLANG = $glslangPath
    $env:KAIN_PLATFORM_VULKAN_SPIRV_VAL = $spirvValPath

    # Transitional compatibility for any existing Vulkain scripts still expecting the SDK root.
    $env:VULKAN_SDK = $sdkRoot

    return [pscustomobject]@{
        LockPath = $lockPath
        ReportPath = $reportPath
        SdkRoot = $sdkRoot
        HeaderPath = $headerPath
        IncludeRoot = $includeRoot
        DllPath = $dllPath
        ImportLibraryPath = $importLibPath
        RegistryPath = $registryPath
        GeneratedModulePath = $generatedModulePath
        GlslangPath = $glslangPath
        SpirvValPath = $spirvValPath
        TargetTriple = [string]$lock.target_triple
        DispatchModel = [string]$lock.dispatch_model
    }
}
