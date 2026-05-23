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
$objectPath = Join-Path $nativeOut "neural_lattice_bridge.obj"

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
New-Item -ItemType Directory -Force -Path $runOut | Out-Null

$clangArgs = @(
    "-c",
    "-O2",
    "-D_CRT_SECURE_NO_WARNINGS",
    "-I", $nativeRoot,
    (Join-Path $nativeRoot "neural_lattice_bridge_impl.c"),
    "-o", $objectPath
)

& $Clang @clangArgs
if ($LASTEXITCODE -ne 0) {
    throw "neural lattice bridge object compilation failed with exit code $LASTEXITCODE"
}

Write-Host "[PASS] Neural lattice bridge object: $objectPath"
