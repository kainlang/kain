param(
    [string]$VulkanSdk = $env:VULKAN_SDK,
    [string]$Clang = "",
    [switch]$SkipShaderCompile
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$nativeRoot = Join-Path $bladeRoot "native"
$shaderRoot = Join-Path $nativeRoot "shaders"
$gpuOut = Join-Path $bladeRoot ".kain\gpu\vulkan_window"
$nativeOut = Join-Path $bladeRoot ".kain\native"

if (!$VulkanSdk -or !(Test-Path $VulkanSdk)) {
    $candidate = "C:\VulkanSDK\1.4.341.1"
    if (Test-Path $candidate) {
        $VulkanSdk = $candidate
    } else {
        throw "VULKAN_SDK is not set and no fallback Vulkan SDK was found."
    }
}

if (!$Clang) {
    $bundled = Join-Path $repoRoot "toolchain\llvm\bin\clang.exe"
    if (Test-Path $bundled) {
        $Clang = $bundled
    } else {
        $clangCommand = Get-Command clang -ErrorAction SilentlyContinue
        if (!$clangCommand) {
            throw "clang was not found."
        }
        $Clang = $clangCommand.Source
    }
}

$glslang = Join-Path $VulkanSdk "Bin\glslangValidator.exe"
$spirvVal = Join-Path $VulkanSdk "Bin\spirv-val.exe"
$vulkanInclude = Join-Path $VulkanSdk "Include"

New-Item -ItemType Directory -Force -Path $gpuOut | Out-Null
New-Item -ItemType Directory -Force -Path $nativeOut | Out-Null

$vertexSpv = Join-Path $gpuOut "kquantum_particles.vert.spv"
$fragmentSpv = Join-Path $gpuOut "kquantum_particles.frag.spv"

if (!(Test-Path $spirvVal)) {
    throw "spirv-val was not found at $spirvVal"
}

if ($SkipShaderCompile) {
    if (!(Test-Path $vertexSpv) -or !(Test-Path $fragmentSpv)) {
        throw "SkipShaderCompile requested, but existing Vulkan window SPIR-V artifacts were not found under $gpuOut"
    }
} else {
    if (!(Test-Path $glslang)) {
        throw "glslangValidator was not found at $glslang"
    }
    & $glslang -V --target-env vulkan1.3 (Join-Path $shaderRoot "kquantum_particles.vert") -o $vertexSpv
    if ($LASTEXITCODE -ne 0) {
        throw "vertex shader SPIR-V compilation failed with exit code $LASTEXITCODE"
    }

    & $glslang -V --target-env vulkan1.3 (Join-Path $shaderRoot "kquantum_particles.frag") -o $fragmentSpv
    if ($LASTEXITCODE -ne 0) {
        throw "fragment shader SPIR-V compilation failed with exit code $LASTEXITCODE"
    }
}

& $spirvVal --target-env vulkan1.3 $vertexSpv
if ($LASTEXITCODE -ne 0) {
    throw "vertex shader spirv-val failed with exit code $LASTEXITCODE"
}

& $spirvVal --target-env vulkan1.3 $fragmentSpv
if ($LASTEXITCODE -ne 0) {
    throw "fragment shader spirv-val failed with exit code $LASTEXITCODE"
}

$objectPath = Join-Path $nativeOut "kquantum_vulkan_bridge.obj"

& $Clang -std=c11 -O2 -D_CRT_SECURE_NO_WARNINGS -I $vulkanInclude -I $nativeRoot -c (Join-Path $nativeRoot "kquantum_vulkan_bridge.c") -o $objectPath
if ($LASTEXITCODE -ne 0) {
    throw "kquantum Vulkan bridge object compilation failed with exit code $LASTEXITCODE"
}

Write-Host "[PASS] Vulkan vertex SPIR-V: $vertexSpv"
Write-Host "[PASS] Vulkan fragment SPIR-V: $fragmentSpv"
Write-Host "[PASS] Vulkan C FFI object: $objectPath"
