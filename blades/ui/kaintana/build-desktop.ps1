param(
    [string]$Clang = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$nativeRoot = Join-Path $bladeRoot "native"
$nativeOut = Join-Path $bladeRoot ".kain\native"

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

$outputObject = Join-Path $nativeOut "kaintana_desktop_bridge.obj"
$compileArgs = @(
    "-c",
    "-O2",
    "-D_CRT_SECURE_NO_WARNINGS",
    "-I", $nativeRoot,
    (Join-Path $nativeRoot "kaintana_desktop_bridge.c"),
    "-o", $outputObject
)

& $Clang @compileArgs
if ($LASTEXITCODE -ne 0) {
    throw "kaintana desktop bridge compilation failed with exit code $LASTEXITCODE"
}

Write-Host "[PASS] Kaintana desktop bridge object: $outputObject"
