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
$gpuOut = Join-Path $bladeRoot ".kain\gpu\basic_window"
$nativeOut = Join-Path $bladeRoot ".kain\native"
$runOut = Join-Path $bladeRoot ".kain\run"

function Get-VulkainDynamicLibraryName([string]$BaseName) {
    if ($IsWindows -or $env:OS -eq "Windows_NT") {
        return "$BaseName.dll"
    }
    if ($IsMacOS) {
        return "lib$BaseName.dylib"
    }
    return "lib$BaseName.so"
}

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
New-Item -ItemType Directory -Force -Path $runOut | Out-Null

$vertexSpv = Join-Path $gpuOut "vulkain_basic.vert.spv"
$fragmentSpv = Join-Path $gpuOut "vulkain_basic.frag.spv"

if (!(Test-Path $spirvVal)) {
    throw "spirv-val was not found at $spirvVal"
}

if ($SkipShaderCompile) {
    if (!(Test-Path $vertexSpv) -or !(Test-Path $fragmentSpv)) {
        throw "SkipShaderCompile requested, but existing Vulkain SPIR-V artifacts were not found under $gpuOut"
    }
} else {
    if (!(Test-Path $glslang)) {
        throw "glslangValidator was not found at $glslang"
    }

    & $glslang -V --target-env vulkan1.3 (Join-Path $shaderRoot "vulkain_basic.vert") -o $vertexSpv
    if ($LASTEXITCODE -ne 0) {
        throw "vertex shader SPIR-V compilation failed with exit code $LASTEXITCODE"
    }

    & $glslang -V --target-env vulkan1.3 (Join-Path $shaderRoot "vulkain_basic.frag") -o $fragmentSpv
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

$sharedLibraryPath = Join-Path $nativeOut (Get-VulkainDynamicLibraryName "vulkain_bridge")
$importLibraryPath = Join-Path $nativeOut "vulkain_bridge.lib"

$clangArgs = @(
    "-shared",
    "-O2",
    "-D_CRT_SECURE_NO_WARNINGS",
    "-I", $vulkanInclude,
    "-I", $nativeRoot,
    (Join-Path $nativeRoot "vulkain_bridge.c"),
    "-o", $sharedLibraryPath,
    "-luser32"
)

if ($IsWindows -or $env:OS -eq "Windows_NT") {
    $clangArgs += "-Wl,/IMPLIB:$importLibraryPath"
}

& $Clang @clangArgs
if ($LASTEXITCODE -ne 0) {
    throw "vulkain shared library compilation failed with exit code $LASTEXITCODE"
}

Write-Host "[PASS] Vulkain vertex SPIR-V: $vertexSpv"
Write-Host "[PASS] Vulkain fragment SPIR-V: $fragmentSpv"
Write-Host "[PASS] Vulkain shared library: $sharedLibraryPath"
