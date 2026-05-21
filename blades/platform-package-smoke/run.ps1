param(
    [switch]$NoRun,
    [switch]$SkipKainCompile,
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev",
    [string]$KainBin = $env:KAIN_BIN
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$fixtureRoot = Join-Path $repoRoot "fixtures\platform_sdk\tiny_math"
$stageRoot = Join-Path $bladeRoot ".kain\sdk\tiny_math"
$proofRoot = Join-Path $bladeRoot ".kain\proof"
$lockDir = Join-Path $bladeRoot ".kain\platform\tiny_math"
$reportPath = Join-Path $proofRoot "tiny_math.lock.report.json"
$firstLockPath = Join-Path $proofRoot "tiny_math.first.lock"
$firstReportPath = Join-Path $proofRoot "tiny_math.first.report.json"
$entry = Join-Path $bladeRoot "src\main.kn"
$rootExe = Join-Path $bladeRoot "platform-package-smoke.exe"

function Resolve-Clang {
    $candidates = @()
    if ($env:KAIN_CLANG_PATH) {
        $candidates += $env:KAIN_CLANG_PATH
    }
    $candidates += (Join-Path $repoRoot "toolchain\llvm\bin\clang.exe")
    $candidates += (Join-Path $repoRoot "toolchain\llvm\bin\clang")
    $clangCommand = Get-Command clang -ErrorAction SilentlyContinue
    if ($clangCommand) {
        $candidates += $clangCommand.Source
    }
    $candidates += "C:\Program Files\LLVM\bin\clang.exe"
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

function Invoke-Kain {
    param([string[]]$Arguments)
    if ($KainBin -and (Test-Path $KainBin)) {
        & $KainBin @Arguments
    } else {
        Push-Location $repoRoot
        try {
            & cargo run -q -p cli --bin kain -- @Arguments
        } finally {
            Pop-Location
        }
    }
    if ($LASTEXITCODE -ne 0) {
        throw "kain command failed: $($Arguments -join ' ')"
    }
}

function Assert-EqualBytes {
    param(
        [string]$Left,
        [string]$Right,
        [string]$Label
    )
    $leftBytes = [System.IO.File]::ReadAllBytes($Left)
    $rightBytes = [System.IO.File]::ReadAllBytes($Right)
    if ($leftBytes.Length -ne $rightBytes.Length) {
        throw "$Label byte length mismatch: $($leftBytes.Length) != $($rightBytes.Length)"
    }
    for ($i = 0; $i -lt $leftBytes.Length; $i += 1) {
        if ($leftBytes[$i] -ne $rightBytes[$i]) {
            throw "$Label differs at byte $i"
        }
    }
}

New-Item -ItemType Directory -Force -Path $stageRoot, $proofRoot, $lockDir | Out-Null
Copy-Item -LiteralPath (Join-Path $fixtureRoot "include") -Destination $stageRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $fixtureRoot "src") -Destination $stageRoot -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $stageRoot "bin") | Out-Null

$isWindows = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)
$isMacOS = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::OSX)
if ($isWindows) {
    $dynamicName = "tiny_math.dll"
} elseif ($isMacOS) {
    $dynamicName = "libtiny_math.dylib"
} else {
    $dynamicName = "libtiny_math.so"
}
$dynamicPath = Join-Path (Join-Path $stageRoot "bin") $dynamicName
$clang = Resolve-Clang
if ($clang) {
    $sourcePath = Join-Path $stageRoot "src\tiny_math.c"
    if ($isWindows) {
        & $clang -shared -O2 "-I$(Join-Path $stageRoot 'include')" $sourcePath -o $dynamicPath
    } else {
        & $clang -shared -fPIC -O2 "-I$(Join-Path $stageRoot 'include')" $sourcePath -o $dynamicPath
    }
    if ($LASTEXITCODE -ne 0) {
        throw "clang failed to build tiny_math dynamic library"
    }
} else {
    [System.IO.File]::WriteAllText($dynamicPath, "fake tiny_math dynamic library bytes; install clang for a callable local DLL proof")
}

$importArgs = @(
    "import", "platform", $stageRoot,
    "--package-name", "tiny_math",
    "--provider", "fixture",
    "--output", $lockDir,
    "--report-json", $reportPath
)
Invoke-Kain -Arguments $importArgs
Copy-Item -LiteralPath (Join-Path $lockDir "tiny_math.lock") -Destination $firstLockPath -Force
Copy-Item -LiteralPath $reportPath -Destination $firstReportPath -Force
Invoke-Kain -Arguments $importArgs
Assert-EqualBytes -Left $firstLockPath -Right (Join-Path $lockDir "tiny_math.lock") -Label "lock determinism"
Assert-EqualBytes -Left $firstReportPath -Right $reportPath -Label "report determinism"

$lockText = Get-Content -LiteralPath (Join-Path $lockDir "tiny_math.lock") -Raw
$stagePrefix = ((Resolve-Path $stageRoot).Path -replace "\\", "/")
if ($lockText.Contains($stagePrefix)) {
    throw "relocatability failure: lock contains staged absolute SDK path"
}
foreach ($required in @(
    "unsupported_by_value_aggregate",
    "type_only_callback_handle",
    "opaque_struct_metadata_only",
    "header declarations plus generated typed thunks"
)) {
    if (!$lockText.Contains($required)) {
        throw "platform lock missing required proof token: $required"
    }
}
if ($lockText.Contains("call_typed")) {
    throw "v1 platform lock leaked public call_typed"
}

if (!$SkipKainCompile) {
    if ($KainBin) {
        & $compileScript `
            -Entry $entry `
            -OutputName "platform-package-smoke.exe" `
            -BazelConfig $BazelConfig `
            -VerifyLlvm `
            -KainBin $KainBin `
            -CompilerBuild auto
    } else {
        & $compileScript `
            -Entry $entry `
            -OutputName "platform-package-smoke.exe" `
            -BazelConfig $BazelConfig `
            -VerifyLlvm `
            -CompilerBuild auto
    }
}

if (!$NoRun -and (Test-Path $rootExe)) {
    Push-Location $bladeRoot
    try {
        & $rootExe
        if ($LASTEXITCODE -ne 0) {
            throw "platform-package-smoke.exe exited with $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
}
