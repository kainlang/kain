param(
    [string]$KainBin = $env:KAIN_BIN
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$pilotDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $pilotDir "..\..\..")
$sourcePath = Join-Path $pilotDir "main.kn"
$outputDir = Join-Path $pilotDir "outputs"
$outputExe = Join-Path $outputDir "pilot.exe"
$outputLl = Join-Path $outputDir "pilot.ll"

function Resolve-KainBinary {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $candidatePaths = @(
        (Join-Path $repoRoot "target\codex-native-ui-host-services-cli\debug\kain.exe"),
        (Join-Path $repoRoot "target\debug\kain.exe"),
        (Join-Path $repoRoot "target\release\kain.exe")
    )

    foreach ($candidate in $candidatePaths) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    $pathCommand = Get-Command kain -ErrorAction SilentlyContinue
    if ($pathCommand) {
        return $pathCommand.Source
    }

    throw "Unable to find kain.exe. Build it with `cargo build -p cli` or pass -KainBin."
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$resolvedKain = Resolve-KainBinary -Requested $KainBin

Push-Location $repoRoot
try {
    & $resolvedKain check $sourcePath --target llvm
    if ($LASTEXITCODE -ne 0) {
        throw "kain check failed with exit code $LASTEXITCODE"
    }

    & $resolvedKain build $sourcePath --target llvm --output $outputExe
    if ($LASTEXITCODE -ne 0) {
        throw "kain build failed with exit code $LASTEXITCODE"
    }

    if (!(Test-Path $outputExe)) {
        throw "Expected executable was not created: $outputExe"
    }
    if (!(Test-Path $outputLl)) {
        throw "Expected LLVM IR was not created: $outputLl"
    }

    $llvm = Get-Content -Raw -Path $outputLl
    $requiredEvidence = @(
        "call i64 @kain_native_ui_host_attach(",
        "call i64 @kain_native_ui_hot_reload_begin(",
        "call i64 @kain_native_ui_font_create(",
        "call i64 @kain_native_ui_draw_resource(",
        "call i64 @kain_native_ui_host_present("
    )

    foreach ($pattern in $requiredEvidence) {
        if (!$llvm.Contains($pattern)) {
            throw "LLVM output missing native UI evidence: $pattern"
        }
    }

    & $outputExe
    $runExitCode = $LASTEXITCODE
    if ($runExitCode -ne 0) {
        throw "pilot.exe exited with code $runExitCode"
    }

    Write-Host "[PASS] native-ui pilot built and executed: $outputExe"
}
finally {
    Pop-Location
}
