param(
    [string]$ConfigPath = "",
    [string]$VulkanSdk = $env:VULKAN_SDK
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (!$ConfigPath) {
    $ConfigPath = Join-Path $bladeRoot "config\\zender.runtime.json"
} elseif (![System.IO.Path]::IsPathRooted($ConfigPath)) {
    $ConfigPath = Join-Path $bladeRoot $ConfigPath
}

$configPath = (Resolve-Path $ConfigPath).Path
$configDir = Split-Path -Parent $configPath
$config = Get-Content -Path $configPath -Raw | ConvertFrom-Json

function Resolve-ConfigRelativePath {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return $PathValue
    }
    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return $PathValue
    }
    return [System.IO.Path]::GetFullPath((Join-Path $configDir $PathValue))
}

if (!$VulkanSdk -or !(Test-Path $VulkanSdk)) {
    $candidateRoot = "C:\\VulkanSDK"
    if (Test-Path $candidateRoot) {
        $candidate = Get-ChildItem -Path $candidateRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($candidate) {
            $VulkanSdk = $candidate.FullName
        }
    }
    if (!$VulkanSdk -or !(Test-Path $VulkanSdk)) {
        throw "VULKAN_SDK is not set and no Vulkan SDK install was discovered under C:\\VulkanSDK."
    }
}

$glslang = Join-Path $VulkanSdk "Bin\\glslangValidator.exe"
$spirvVal = Join-Path $VulkanSdk "Bin\\spirv-val.exe"
if (!(Test-Path $glslang)) {
    throw "glslangValidator was not found at $glslang"
}
if (!(Test-Path $spirvVal)) {
    throw "spirv-val was not found at $spirvVal"
}

$includeRoot = Join-Path $VulkanSdk "Include"
$headerPath = Join-Path $includeRoot "vulkan\\vulkan.h"
if (!(Test-Path $headerPath)) {
    throw "vulkan.h was not found at $headerPath"
}

$env:KAIN_PLATFORM_VULKAN_SDK_ROOT = $VulkanSdk
$env:KAIN_PLATFORM_VULKAN_SDK = $VulkanSdk
$env:KAIN_PLATFORM_VULKAN_INCLUDE = $includeRoot
$env:KAIN_PLATFORM_VULKAN_GLSLANG = $glslang
$env:KAIN_PLATFORM_VULKAN_SPIRV_VAL = $spirvVal
$env:VULKAN_SDK = $VulkanSdk

$shaderRoot = Join-Path $bladeRoot "native\\shaders"
$vertexSource = Join-Path $shaderRoot "zender_particles.vert"
$fragmentSource = Join-Path $shaderRoot "zender_particles.frag"
$vertexSpv = Resolve-ConfigRelativePath $config.app.vertex_shader_path
$fragmentSpv = Resolve-ConfigRelativePath $config.app.fragment_shader_path

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $vertexSpv) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fragmentSpv) | Out-Null

& $glslang -V --target-env vulkan1.3 $vertexSource -o $vertexSpv
if ($LASTEXITCODE -ne 0) {
    throw "vertex shader SPIR-V compilation failed with exit code $LASTEXITCODE"
}

& $glslang -V --target-env vulkan1.3 $fragmentSource -o $fragmentSpv
if ($LASTEXITCODE -ne 0) {
    throw "fragment shader SPIR-V compilation failed with exit code $LASTEXITCODE"
}

& $spirvVal --target-env vulkan1.3 $vertexSpv
if ($LASTEXITCODE -ne 0) {
    throw "vertex shader spirv-val failed with exit code $LASTEXITCODE"
}

& $spirvVal --target-env vulkan1.3 $fragmentSpv
if ($LASTEXITCODE -ne 0) {
    throw "fragment shader spirv-val failed with exit code $LASTEXITCODE"
}

Write-Host "[PASS] Vulkan vertex SPIR-V: $vertexSpv"
Write-Host "[PASS] Vulkan fragment SPIR-V: $fragmentSpv"
