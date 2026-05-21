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
$rootExe = Join-Path $bladeRoot "kloner.exe"
$runtimeCacheRoot = Join-Path $bladeRoot ".kain\native_runtime\cache"
$runOut = Join-Path $bladeRoot ".kain\run"

& $kaintanaDesktopBuild
if ($SkipShaderCompile) {
    & $vulkainBuild -SkipShaderCompile -KainBin $KainBin
} else {
    & $vulkainBuild -KainBin $KainBin
}

$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
New-Item -ItemType Directory -Force -Path $runOut | Out-Null

$entry = Join-Path $bladeRoot "src\main.kn"

if ($KainBin) {
    & $compileScript -Entry $entry -OutputName "kloner.exe" -BazelConfig $BazelConfig -VerifyLlvm -KainBin $KainBin -CompilerBuild auto
} else {
    & $compileScript -Entry $entry -OutputName "kloner.exe" -BazelConfig $BazelConfig -VerifyLlvm
}

if (Test-Path $vulkainNativeBridge) {
    Copy-Item -LiteralPath $vulkainNativeBridge -Destination $rootVulkainBridge -Force
}

Push-Location $bladeRoot
try {
    if (!$NoRun) {
        if ($FrameBudget -gt 0) {
            $env:KLONER_FRAME_BUDGET = [string]$FrameBudget
        }
        try {
            & $rootExe
        }
        finally {
            Remove-Item Env:KLONER_FRAME_BUDGET -ErrorAction SilentlyContinue
        }
    }
}
finally {
    Pop-Location
}
