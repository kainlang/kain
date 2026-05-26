param(
    [switch]$NoRun,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [string]$KainBin = $env:KAIN_BIN
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$vulkainBuildScript = Join-Path $repoRoot "blades\vulkain\build-vulkain.ps1"
$vulkainNativeBridge = Join-Path $repoRoot "blades\vulkain\.kain\native\vulkain_bridge.dll"
$rootBridge = Join-Path $bladeRoot "vulkain_bridge.dll"
$rootExe = Join-Path $bladeRoot "kaintana-vulkan.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"

& $vulkainBuildScript

$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
$entry = Join-Path $bladeRoot "src\main.kn"

if ($KainBin) {
    & $compileScript -Entry $entry -OutputName "kaintana-vulkan.exe" -BazelConfig $BazelConfig -VerifyLlvm -KainBin $KainBin -CompilerBuild auto
} else {
    & $compileScript -Entry $entry -OutputName "kaintana-vulkan.exe" -BazelConfig $BazelConfig -VerifyLlvm
}

if (Test-Path $vulkainNativeBridge) {
    Copy-Item -LiteralPath $vulkainNativeBridge -Destination $rootBridge -Force
}

if (!$NoRun) {
    Push-Location $bladeRoot
    try {
        & $rootExe
    } finally {
        Pop-Location
    }
}
