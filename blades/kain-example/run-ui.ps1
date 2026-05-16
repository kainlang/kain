param(
    [string]$KainBin = $env:KAIN_BIN,
    [switch]$Interactive
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeDir "..\..")
$sourcePath = Join-Path $bladeDir "src\ui.kn"
$outputDir = Join-Path $repoRoot "target\kain-example"
$outputExe = Join-Path $outputDir "kain_example_workbench.exe"
$outputLl = Join-Path $outputDir "kain_example_workbench.ll"
$outputBmp = Join-Path $outputDir "kain_example_workbench.bmp"
$labsDir = Join-Path $repoRoot "labs\cookiecutter\outputs"
$showcaseReport = Join-Path $labsDir "showcase_report.txt"

function Resolve-KainBinary {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $candidatePaths = @(
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
    if (Test-Path $outputBmp) {
        Remove-Item -LiteralPath $outputBmp -Force
    }

    & $resolvedKain check $sourcePath --target llvm
    if ($LASTEXITCODE -ne 0) {
        throw "kain check failed with exit code $LASTEXITCODE"
    }

    & $resolvedKain $sourcePath -t llvm -o $outputExe
    if ($LASTEXITCODE -ne 0) {
        throw "native llvm compile failed with exit code $LASTEXITCODE"
    }

    if (!(Test-Path $outputExe)) {
        throw "Expected executable was not created: $outputExe"
    }
    if (!(Test-Path $outputLl)) {
        throw "Expected LLVM IR was not created: $outputLl"
    }

    & (Join-Path $repoRoot "toolchain\llvm\bin\llvm-as.exe") $outputLl -o (Join-Path $outputDir "kain_example_workbench.bc")
    if ($LASTEXITCODE -ne 0) {
        throw "llvm-as verification failed with exit code $LASTEXITCODE"
    }

    $llvm = Get-Content -Raw -Path $outputLl
    $requiredEvidence = @(
        "call i64 @abi_ui_host_attach(",
        "call i64 @abi_ui_node_set_state_string(",
        "call i64 @abi_ui_draw_resource(",
        "call i64 @abi_input_push_agent_intent(",
        "call i64 @abi_graphics_draw_mesh(",
        "call i64 @abi_actor_abi_version(",
        "call i64 @abi_entangle_registered_count(",
        "call i64 @abi_fs_write_text(",
        "call i64 @abi_ui_host_present("
    )

    foreach ($pattern in $requiredEvidence) {
        if (!$llvm.Contains($pattern)) {
            throw "LLVM output missing workbench evidence: $pattern"
        }
    }

    $runExitCode = 0
    try {
        if (!$Interactive) {
            $env:KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH = $outputBmp
            $env:KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES = "8"
        }

        & $outputExe
        $runExitCode = $LASTEXITCODE
    }
    finally {
        Remove-Item Env:KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH -ErrorAction SilentlyContinue
        Remove-Item Env:KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES -ErrorAction SilentlyContinue
    }

    if ($runExitCode -ne 0) {
        throw "kain_example_workbench.exe exited with code $runExitCode"
    }

    if (!(Test-Path $showcaseReport)) {
        throw "Expected labs report was not created: $showcaseReport"
    }

    if (!$Interactive) {
        if (!(Test-Path $outputBmp)) {
            throw "Expected screenshot was not created: $outputBmp"
        }
        if ((Get-Item $outputBmp).Length -le 1024) {
            throw "Screenshot was too small: $outputBmp"
        }
    }

    Write-Host "[PASS] workbench built and executed: $outputExe"
    Write-Host "[PASS] labs report: $showcaseReport"
    if ($Interactive) {
        Write-Host "[INFO] Interactive run completed after the window closed or the frame budget ended."
    } else {
        Write-Host "[PASS] screenshot: $outputBmp"
    }
}
finally {
    Pop-Location
}
