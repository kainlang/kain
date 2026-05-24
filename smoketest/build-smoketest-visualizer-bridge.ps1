param(
    [string]$Clang = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$smoketestRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $smoketestRoot "..")
$nativeRoot = Join-Path $smoketestRoot "native"
$nativeOut = Join-Path $smoketestRoot ".kain\native"
$objectPath = Join-Path $nativeOut "smoketest_visualizer_bridge.obj"

if (!$Clang) {
    $bundled = Join-Path $repoRoot "toolchain\llvm\bin\clang.exe"
    if (Test-Path $bundled) {
        $Clang = $bundled
    } else {
        $clangCommand = Get-Command clang -ErrorAction SilentlyContinue
        if (!$clangCommand) {
            throw "clang was not found."
        }
        $Clang = $clangCommand.Source
    }
}

New-Item -ItemType Directory -Force -Path $nativeOut | Out-Null

$clangArgs = @(
    "-c",
    "-O2",
    "-D_CRT_SECURE_NO_WARNINGS",
    "-I", $nativeRoot,
    (Join-Path $nativeRoot "smoketest_visualizer_bridge.c"),
    "-o", $objectPath
)

& $Clang @clangArgs
if ($LASTEXITCODE -ne 0) {
    throw "smoketest visualizer bridge object compilation failed with exit code $LASTEXITCODE"
}

Write-Host "[PASS] Smoketest visualizer bridge object: $objectPath"
