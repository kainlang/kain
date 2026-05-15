param(
    [switch]$NoRun,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1"
$prebuildScript = Join-Path $bladeRoot "build-opengl.ps1"
$rootExe = Join-Path $bladeRoot "opengl.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"

& $prebuildScript

$env:OPENGL_BLADE_ROOT = $bladeRoot
$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
$entry = Join-Path $bladeRoot "src\main.kn"

& $compileScript -Entry $entry -OutputName "opengl.exe" -BazelConfig $BazelConfig -VerifyLlvm

if (!$NoRun) {
    Push-Location $bladeRoot
    try {
        & $rootExe
    } finally {
        Pop-Location
    }
}
