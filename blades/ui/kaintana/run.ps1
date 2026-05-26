param(
    [switch]$NoRun,
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
$desktopBuildScript = Join-Path $bladeRoot "build-desktop.ps1"
$rootExe = Join-Path $bladeRoot "kaintana.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"

& $desktopBuildScript

$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
$entry = Join-Path $bladeRoot "src\main.kn"

if ($KainBin) {
    & $compileScript -Entry $entry -OutputName "kaintana.exe" -BazelConfig $BazelConfig -VerifyLlvm -KainBin $KainBin -CompilerBuild auto
} else {
    & $compileScript -Entry $entry -OutputName "kaintana.exe" -BazelConfig $BazelConfig -VerifyLlvm
}

if (!$NoRun) {
    Push-Location $bladeRoot
    try {
        if ($FrameBudget -gt 0) {
            $env:KAINTANA_EXAMPLES_FRAME_BUDGET = [string]$FrameBudget
        }
        & $rootExe
    } finally {
        Remove-Item Env:KAINTANA_EXAMPLES_FRAME_BUDGET -ErrorAction SilentlyContinue
        Pop-Location
    }
}
