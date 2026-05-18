param(
    [switch]$NoRun,
    [switch]$SkipShaderCompile,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [string]$KainBin = $env:KAIN_BIN
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$exampleRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$vulkainRoot = Resolve-Path (Join-Path $exampleRoot "..\..")
$repoRoot = Resolve-Path (Join-Path $vulkainRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\kain-blade-workspace\scripts\compile_kain_blade_to_root.ps1"
$prebuildScript = Join-Path $vulkainRoot "build-vulkain.ps1"
$rootExe = Join-Path $exampleRoot "vulkain-math-bounce.exe"
$runtimeCacheRoot = Join-Path $exampleRoot ".kain\native_runtime\cache"

if ($SkipShaderCompile) {
    & $prebuildScript -SkipShaderCompile
} else {
    & $prebuildScript
}

$env:VULKAIN_BLADE_ROOT = $vulkainRoot
$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $exampleRoot ".kain\run") | Out-Null
$entry = Join-Path $exampleRoot "src\main.kn"

if ($KainBin) {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain-math-bounce.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -KainBin $KainBin `
        -CompilerBuild auto
} else {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain-math-bounce.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -CompilerBuild auto
}

if (!$NoRun) {
    Push-Location $exampleRoot
    try {
        & $rootExe
    } finally {
        Pop-Location
    }
}

