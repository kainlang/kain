param(
    [switch]$NoRun,
    [switch]$SkipShaderCompile,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1"
$prebuildScript = Join-Path $bladeRoot "build-vulkain.ps1"
$nativeBridge = Join-Path $bladeRoot ".kain\native\vulkain_bridge.dll"
$rootBridge = Join-Path $bladeRoot "vulkain_bridge.dll"
$rootExe = Join-Path $bladeRoot "vulkain.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"

if ($SkipShaderCompile) {
    & $prebuildScript -SkipShaderCompile
} else {
    & $prebuildScript
}

$env:VULKAIN_BLADE_ROOT = $bladeRoot
$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
$entry = Join-Path $bladeRoot "src\main.kn"

& $compileScript -Entry $entry -OutputName "vulkain.exe" -BazelConfig $BazelConfig -VerifyLlvm

Copy-Item -LiteralPath $nativeBridge -Destination $rootBridge -Force

if (!$NoRun) {
    Push-Location $bladeRoot
    try {
        & $rootExe
    } finally {
        Pop-Location
    }
}
