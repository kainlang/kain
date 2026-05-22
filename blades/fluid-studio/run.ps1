param(
    [switch]$NoRun,
    [switch]$SkipShaderCompile,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [int]$FrameBudget = 0,
    [string]$KainBin = $env:KAIN_BIN
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$kaintanaDesktopBuild = Join-Path $repoRoot "blades\kaintana\build-desktop.ps1"
$vulkainBuild = Join-Path $repoRoot "blades\vulkain\build-vulkain.ps1"
$vulkainNativeBridge = Join-Path $repoRoot "blades\vulkain\.kain\native\vulkain_bridge.dll"
$rootVulkainBridge = Join-Path $bladeRoot "vulkain_bridge.dll"
$rootExe = Join-Path $bladeRoot "fluid-studio.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"
$runOut = Join-Path $bladeRoot ".kain\run"
$shaderOutDir = Join-Path $bladeRoot ".kain\gpu\fluid_studio"
$surfaceEntry = Join-Path $bladeRoot "src\fluid_surface.frag.kn"
$computeEntry = Join-Path $bladeRoot "src\fluid_compute.kn"
$fragmentSpv = Join-Path $shaderOutDir "fluid_surface.frag.spv"

function Resolve-KainBinary {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $candidates = @(
        (Join-Path $repoRoot "target\debug\kain.exe"),
        (Join-Path $repoRoot "target\release\kain.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    Push-Location $repoRoot
    try {
        & cargo build -p cli --bin kain
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build -p cli --bin kain failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }

    $built = Join-Path $repoRoot "target\debug\kain.exe"
    if (!(Test-Path $built)) {
        throw "Expected built Kain binary was not found: $built"
    }
    return (Resolve-Path $built).Path
}

& $kaintanaDesktopBuild
if ($SkipShaderCompile) {
    & $vulkainBuild -SkipShaderCompile -KainBin $KainBin
}
else {
    & $vulkainBuild -KainBin $KainBin
}

$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
New-Item -ItemType Directory -Force -Path $runOut | Out-Null
New-Item -ItemType Directory -Force -Path $shaderOutDir | Out-Null

$entry = Join-Path $bladeRoot "src\main.kn"
$resolvedKain = Resolve-KainBinary -Requested $KainBin
$spirvVal = $env:KAIN_PLATFORM_VULKAN_SPIRV_VAL

Push-Location $bladeRoot
try {
    & $resolvedKain check $computeEntry --target spirv
    if ($LASTEXITCODE -ne 0) {
        throw "kain check for the fluid compute shader set failed with exit code $LASTEXITCODE"
    }

    & $resolvedKain check $surfaceEntry --target spirv
    if ($LASTEXITCODE -ne 0) {
        throw "kain check for the fluid surface shader failed with exit code $LASTEXITCODE"
    }

    if (!$SkipShaderCompile) {
        & $resolvedKain $surfaceEntry -t spirv -o $fragmentSpv
        if ($LASTEXITCODE -ne 0) {
            throw "Kain surface SPIR-V compilation failed with exit code $LASTEXITCODE"
        }
    }
}
finally {
    Pop-Location
}

if ($spirvVal -and (Test-Path $spirvVal) -and (Test-Path $fragmentSpv)) {
    & $spirvVal --target-env vulkan1.3 $fragmentSpv
    if ($LASTEXITCODE -ne 0) {
        throw "spirv-val rejected $fragmentSpv with exit code $LASTEXITCODE"
    }
}

if ($KainBin) {
    & $compileScript -Entry $entry -OutputName "fluid-studio.exe" -BazelConfig $BazelConfig -VerifyLlvm -KainBin $resolvedKain -CompilerBuild auto
}
else {
    & $compileScript -Entry $entry -OutputName "fluid-studio.exe" -BazelConfig $BazelConfig -VerifyLlvm -KainBin $resolvedKain -CompilerBuild auto
}

if (Test-Path $vulkainNativeBridge) {
    Copy-Item -LiteralPath $vulkainNativeBridge -Destination $rootVulkainBridge -Force
}

Push-Location $bladeRoot
try {
    if (!$NoRun) {
        if ($FrameBudget -gt 0) {
            $env:FLUID_STUDIO_FRAME_BUDGET = [string]$FrameBudget
        }
        try {
            & $rootExe
        }
        finally {
            Remove-Item Env:FLUID_STUDIO_FRAME_BUDGET -ErrorAction SilentlyContinue
        }
    }
}
finally {
    Pop-Location
}
