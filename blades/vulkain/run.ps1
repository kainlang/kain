param(
    [switch]$NoRun,
    [switch]$SkipShaderCompile,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [string]$KainBin = $env:KAIN_BIN
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
    & $prebuildScript -SkipShaderCompile
} else {
    & $prebuildScript
}

$env:VULKAIN_BLADE_ROOT = $bladeRoot
$env:KAIN_RUNTIME_CACHE_DIR = $runtimeCacheRoot
New-Item -ItemType Directory -Force -Path $runtimeCacheRoot | Out-Null
$entry = Join-Path $bladeRoot "src\main.kn"

$resolvedKainBin = if ($KainBin) { $KainBin } else { $null }
if ($resolvedKainBin) {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -KainBin $resolvedKainBin `
        -CompilerBuild auto
} else {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain.exe" `
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
