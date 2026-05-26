param(
    [switch]$NoRun,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [string]$KainBin = $env:KAIN_BIN,
    [string]$Config = "",
    [string]$BundlePath = "",
    [string]$RealtimeBundlePath = "",
    [string]$SpvPath = "",
    [string]$VertexPath = "",
    [string]$FragmentPath = "",
    [string]$VertexEntryPoint = "",
    [string]$FragmentEntryPoint = "",
    [string]$ScanRoots = "",
    [int]$FrameBudget = 0
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$vulkainBuildScript = Join-Path $repoRoot "blades\vulkain\build-vulkain.ps1"
$platformScript = Join-Path $repoRoot "blades\vulkain\scripts\vulkan-platform.ps1"
$sampleShader = Join-Path $bladeRoot "shaders\spirv_visualizer_samples.kn"
$sampleOutput = Join-Path $bladeRoot ".kain\gpu\samples"
$sampleRoot = Join-Path $sampleOutput "spirv_visualizer_samples"
$sampleSpv = Join-Path $sampleRoot "spirv_visualizer_samples.spv"
$defaultVertexSpv = Join-Path $repoRoot "blades\vulkain\.kain\gpu\basic_window\vulkain_basic.vert.spv"
$defaultFragmentSpv = Join-Path $repoRoot "blades\vulkain\.kain\gpu\basic_window\vulkain_basic.frag.spv"
$nativeBridge = Join-Path $repoRoot "blades\vulkain\.kain\native\vulkain_bridge.dll"
$rootBridge = Join-Path $bladeRoot "vulkain_bridge.dll"
$rootExe = Join-Path $bladeRoot "spirv-visualizer.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"
$legacyCatalog = Join-Path $bladeRoot ".kain\run\spirv_visualizer_catalog.json"
$legacyReport = Join-Path $bladeRoot ".kain\run\spirv_visualizer_report.txt"

function Resolve-ExistingBazelKainBinary {
    $bazelCommand = Get-Command bazel -ErrorAction SilentlyContinue
    if (!$bazelCommand) {
        return $null
    }

    Push-Location $repoRoot
    try {
        $bazelBinText = & $bazelCommand.Source info bazel-bin "--config=$BazelConfig"
        if ($LASTEXITCODE -ne 0) {
            return $null
        }

        $ansiEscape = [char]27
        $bazelBin = $bazelBinText |
            ForEach-Object { ($_ -replace "$ansiEscape\[[0-9;]*m", "").Trim() } |
            Where-Object { $_ -and ($_ -match "[:/\\]") } |
            Select-Object -Last 1

        foreach ($candidate in @(
            (Join-Path $bazelBin "crates\cli\kain.exe"),
            (Join-Path $bazelBin "kain.exe")
        )) {
            if ($candidate -and (Test-Path $candidate)) {
                return (Resolve-Path $candidate).Path
            }
        }
    } finally {
        Pop-Location
    }

    return $null
}

function Resolve-KainBinary {
    if ($KainBin -and (Test-Path $KainBin)) {
        return (Resolve-Path $KainBin).Path
    }

    $bazelResolved = Resolve-ExistingBazelKainBinary
    if ($bazelResolved) {
        return $bazelResolved
    }

    foreach ($candidate in @(
        (Join-Path $repoRoot "target\debug\kain.exe"),
        (Join-Path $repoRoot "target\release\kain.exe")
    )) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

New-Item -ItemType Directory -Force -Path $sampleOutput | Out-Null
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
foreach ($stale in @($legacyCatalog, $legacyReport)) {
    if (Test-Path $stale) {
        Remove-Item -LiteralPath $stale -Force
    }
}

$resolvedKain = Resolve-KainBinary
& $vulkainBuildScript -KainBin $resolvedKain

if (!$resolvedKain) {
    $resolvedKain = Resolve-KainBinary
}
if ($resolvedKain) {
    & $resolvedKain gpu-artifacts $sampleShader --output $sampleRoot
    if ($LASTEXITCODE -ne 0) {
        throw "kain gpu-artifacts failed with exit code $LASTEXITCODE"
    }
}

. $platformScript
$platform = Sync-VulkainPlatformPackage -BladeRoot (Join-Path $repoRoot "blades\vulkain") -KainBin $resolvedKain
$spirvVal = $platform.SpirvValPath
if ($spirvVal -and (Test-Path $spirvVal) -and (Test-Path $sampleRoot)) {
    Get-ChildItem -LiteralPath $sampleRoot -Recurse -Filter *.spv | ForEach-Object {
        & $spirvVal --target-env vulkan1.3 $_.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "spirv-val failed for $($_.FullName) with exit code $LASTEXITCODE"
        }
    }
}

$env:SPIRV_VISUALIZER_SAMPLE_ROOT = $sampleRoot
$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
if ($Config) {
    $env:SPIRV_VISUALIZER_CONFIG = $Config
}
if ($BundlePath) {
    $env:SPIRV_VISUALIZER_BUNDLE_PATH = $BundlePath
}
if ($RealtimeBundlePath) {
    $env:SPIRV_VISUALIZER_REALTIME_BUNDLE_PATH = $RealtimeBundlePath
}
if ($SpvPath) {
    $env:SPIRV_VISUALIZER_SPV_PATH = $SpvPath
}
if ($VertexPath) {
    $env:SPIRV_VISUALIZER_VERTEX_PATH = $VertexPath
} elseif (Test-Path $defaultVertexSpv) {
    $env:SPIRV_VISUALIZER_VERTEX_PATH = $defaultVertexSpv
}
if ($FragmentPath) {
    $env:SPIRV_VISUALIZER_FRAGMENT_PATH = $FragmentPath
} elseif (Test-Path $sampleSpv) {
    $env:SPIRV_VISUALIZER_FRAGMENT_PATH = $sampleSpv
}
if ($VertexEntryPoint) {
    $env:SPIRV_VISUALIZER_VERTEX_ENTRY_POINT = $VertexEntryPoint
}
if ($FragmentEntryPoint) {
    $env:SPIRV_VISUALIZER_FRAGMENT_ENTRY_POINT = $FragmentEntryPoint
} elseif (!$FragmentPath -and (Test-Path $sampleSpv)) {
    $env:SPIRV_VISUALIZER_FRAGMENT_ENTRY_POINT = "SpirvCapabilitySpectrum"
}
if ($ScanRoots) {
    $env:SPIRV_VISUALIZER_SCAN_ROOTS = $ScanRoots
}
if ($FrameBudget -gt 0) {
    $env:SPIRV_VISUALIZER_FRAME_BUDGET = [string]$FrameBudget
}

$entry = Join-Path $bladeRoot "src\main.kn"
if ($resolvedKain) {
    & $compileScript `
        -Entry $entry `
        -OutputName "spirv-visualizer.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -KainBin $resolvedKain `
        -CompilerBuild auto
} else {
    & $compileScript `
        -Entry $entry `
        -OutputName "spirv-visualizer.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -CompilerBuild auto
}

Copy-Item -LiteralPath $nativeBridge -Destination $rootBridge -Force

if (!$NoRun) {
    Push-Location $bladeRoot
    try {
        & $rootExe
    } finally {
        Pop-Location
    }
}
