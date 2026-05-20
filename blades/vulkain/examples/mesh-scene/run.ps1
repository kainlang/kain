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
$rootExe = Join-Path $exampleRoot "vulkain-mesh-scene.exe"
$runtimeCacheRoot = Join-Path $exampleRoot ".kain\native_runtime\cache"

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

if ($SkipShaderCompile) {
    & $prebuildScript -SkipShaderCompile -KainBin $KainBin
} else {
    & $prebuildScript -KainBin $KainBin
}

$env:VULKAIN_BLADE_ROOT = $vulkainRoot
$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $exampleRoot ".kain\run") | Out-Null
$entry = Join-Path $exampleRoot "src\main.kn"

$resolvedKainBin = if ($KainBin) { $KainBin } else { $null }
if ($resolvedKainBin) {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain-mesh-scene.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -KainBin $resolvedKainBin `
        -CompilerBuild auto
} else {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain-mesh-scene.exe" `
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
