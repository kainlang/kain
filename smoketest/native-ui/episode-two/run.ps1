param(
    [string]$KainBin = $env:KAIN_BIN,
    [switch]$Interactive
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$episodeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $episodeDir "..\..\..")
$sourcePath = Join-Path $episodeDir "main.kn"
$outputDir = Join-Path $episodeDir "outputs"
$outputExe = Join-Path $outputDir "episode-two.exe"
$outputLl = Join-Path $outputDir "episode-two.ll"
$screenshotBmp = Join-Path $outputDir "episode-two.bmp"

function Resolve-KainBinary {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $candidatePaths = @(
        (Join-Path $repoRoot "target\codex-native-ui-win32\debug\kain.exe"),
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
    if (Test-Path $screenshotBmp) {
        Remove-Item -LiteralPath $screenshotBmp -Force
    }

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
        "call i64 @kain_native_ui_node_set_state_string(",
        "call i64 @kain_native_ui_draw_resource(",
        "call i64 @kain_native_input_push_agent_intent(",
        "call i64 @kain_native_graphics_draw_mesh(",
        "call i64 @kain_native_actor_abi_version(",
        "call i64 @kain_native_entangle_registered_count(",
        "call i64 @kain_native_ui_host_present("
    )

    foreach ($pattern in $requiredEvidence) {
        if (!$llvm.Contains($pattern)) {
            throw "LLVM output missing episode-two evidence: $pattern"
        }
    }

    $runExitCode = 0
    try {
        if (!$Interactive) {
            $env:KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH = $screenshotBmp
            $env:KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES = "5"
        }

        & $outputExe
        $runExitCode = $LASTEXITCODE
    }
    finally {
        Remove-Item Env:KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH -ErrorAction SilentlyContinue
        Remove-Item Env:KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES -ErrorAction SilentlyContinue
    }

    if ($runExitCode -ne 0) {
        throw "episode-two.exe exited with code $runExitCode"
    }

    if (!$Interactive) {
        if (!(Test-Path $screenshotBmp)) {
            throw "Expected screenshot was not created: $screenshotBmp"
        }
        if ((Get-Item $screenshotBmp).Length -le 1024) {
            throw "Screenshot was too small: $screenshotBmp"
        }
    }

    Write-Host "[PASS] native-ui episode-two built and executed: $outputExe"
    if ($Interactive) {
        Write-Host "[INFO] Interactive run completed after the window closed or the frame budget ended."
    } else {
        Write-Host "[PASS] screenshot: $screenshotBmp"
    }
}
finally {
    Pop-Location
}
