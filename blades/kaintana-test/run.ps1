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
$compileScript = Join-Path $repoRoot ".agents\skills\lang-blades\scripts\compile_kain_blade_to_root.ps1"
$kaintanaDesktopBuild = Join-Path $repoRoot "blades\kaintana\build-desktop.ps1"
$rootExe = Join-Path $bladeRoot "kaintana-test.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"
$runOut = Join-Path $bladeRoot ".kain\run"

& $kaintanaDesktopBuild

$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
New-Item -ItemType Directory -Force -Path $runOut | Out-Null

$entry = Join-Path $bladeRoot "src\main.kn"

if ($KainBin) {
    & $compileScript -Entry $entry -OutputName "kaintana-test.exe" -BazelConfig $BazelConfig -VerifyLlvm -KainBin $KainBin -CompilerBuild auto
} else {
    & $compileScript -Entry $entry -OutputName "kaintana-test.exe" -BazelConfig $BazelConfig -VerifyLlvm
}

Push-Location $bladeRoot
try {
    if (!$NoRun) {
        if ($FrameBudget -gt 0) {
            $env:KAINTANA_TEST_FRAME_BUDGET = [string]$FrameBudget
        }
        try {
            & $rootExe
        }
        finally {
            Remove-Item Env:KAINTANA_TEST_FRAME_BUDGET -ErrorAction SilentlyContinue
        }
    }
} finally {
    Pop-Location
}
