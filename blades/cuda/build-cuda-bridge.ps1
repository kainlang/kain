param(
    [string]$Clang = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$nativeRoot = Join-Path $bladeRoot "native"
$nativeOut = Join-Path $bladeRoot ".kain\native"
$runOut = Join-Path $bladeRoot ".kain\run"
$exePath = Join-Path $nativeOut "cuda_visual_verify.exe"

if (!$Clang) {
    $bundled = Join-Path $repoRoot "toolchain\llvm\bin\clang++.exe"
    if (Test-Path $bundled) {
        $Clang = $bundled
    } else {
        $clangxx = Get-Command clang++ -ErrorAction SilentlyContinue
        if ($clangxx) {
            $Clang = $clangxx.Source
        } else {
            $clang = Get-Command clang -ErrorAction SilentlyContinue
            if (!$clang) {
                throw "clang++ or clang was not found."
            }
            $Clang = $clang.Source
        }
    }
}

New-Item -ItemType Directory -Force -Path $nativeOut | Out-Null
New-Item -ItemType Directory -Force -Path $runOut | Out-Null

$clangArgs = @(
    "-O2",
    "-std=c++20",
    "-D_CRT_SECURE_NO_WARNINGS",
    "-I", $nativeRoot,
    (Join-Path $nativeRoot "cuda_visual_bridge.cpp"),
    "-o", $exePath
)

& $Clang @clangArgs
if ($LASTEXITCODE -ne 0) {
    throw "cuda visual verifier compilation failed with exit code $LASTEXITCODE"
}

Write-Host "[PASS] CUDA visual verifier: $exePath"
