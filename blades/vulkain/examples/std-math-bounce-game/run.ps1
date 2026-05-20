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
$rootExe = Join-Path $exampleRoot "vulkain-math-bounce.exe"
$runtimeCacheRoot = Join-Path $exampleRoot ".kain\native_runtime\cache"
$shaderEntry = Join-Path $exampleRoot "src\bounce_game_mesh.frag.kn"
$shaderOutDir = Join-Path $exampleRoot ".kain\gpu\std_math_bounce_game"
$fragmentSpv = Join-Path $shaderOutDir "bounce_game_mesh.frag.spv"

function Resolve-KainBinary {
    param([string]$Requested)

    if ($Requested -and (Test-Path $Requested)) {
        return (Resolve-Path $Requested).Path
    }

    $bazelCommand = Get-Command bazel -ErrorAction SilentlyContinue
    if ($bazelCommand) {
        Push-Location $repoRoot
        try {
            & $bazelCommand.Source build "//:kain" "--config=$BazelConfig"
            if ($LASTEXITCODE -eq 0) {
                $bazelBinText = & $bazelCommand.Source info bazel-bin "--config=$BazelConfig"
                if ($LASTEXITCODE -ne 0) {
                    throw "bazel info bazel-bin --config=$BazelConfig failed with exit code $LASTEXITCODE"
                }

                $ansiEscape = [char]27
                $bazelBin = $bazelBinText |
                    ForEach-Object { ($_ -replace "$ansiEscape\[[0-9;]*m", "").Trim() } |
                    Where-Object { $_ -and ($_ -match "[:/\\]") } |
                    Select-Object -Last 1

                if ($bazelBin) {
                    $bazelKain = Join-Path $bazelBin "crates\cli\kain.exe"
                    if (Test-Path $bazelKain) {
                        return (Resolve-Path $bazelKain).Path
                    }

                    $aliasKain = Join-Path $bazelBin "kain.exe"
                    if (Test-Path $aliasKain) {
                        return (Resolve-Path $aliasKain).Path
                    }
                }
            }
        } finally {
            Pop-Location
        }
    }

    $candidates = @(
        (Join-Path $repoRoot "target\debug\kain.exe"),
        (Join-Path $repoRoot "target\release\kain.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    Push-Location $repoRoot
    try {
        & cargo build -p cli --bin kain --bin kn
        if ($LASTEXITCODE -ne 0) {
            throw "cargo build -p cli failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }

    $built = Join-Path $repoRoot "target\debug\kain.exe"
    if (!(Test-Path $built)) {
        throw "Expected built Kain binary was not found: $built"
    }

    return (Resolve-Path $built).Path
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
New-Item -ItemType Directory -Force -Path $shaderOutDir | Out-Null
$entry = Join-Path $exampleRoot "src\main.kn"
$resolvedKain = Resolve-KainBinary -Requested $KainBin
$spirvVal = $env:KAIN_PLATFORM_VULKAN_SPIRV_VAL

if ($SkipShaderCompile) {
    if (!(Test-Path $fragmentSpv)) {
        throw "SkipShaderCompile requested, but the Kain fragment SPIR-V artifact was not found: $fragmentSpv"
    }
} else {
    Push-Location $exampleRoot
    try {
        & $resolvedKain check $shaderEntry --target spirv
        if ($LASTEXITCODE -ne 0) {
            throw "kain check for the bounce-game fragment shader failed with exit code $LASTEXITCODE"
        }

        & $resolvedKain $shaderEntry -t spirv -o $fragmentSpv
        if ($LASTEXITCODE -ne 0) {
            throw "Kain fragment SPIR-V compilation failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
}

if (!(Test-Path $spirvVal)) {
    throw "spirv-val was not found at $spirvVal"
}

& $spirvVal --target-env vulkan1.3 $fragmentSpv
if ($LASTEXITCODE -ne 0) {
    throw "Kain fragment SPIR-V validation failed with exit code $LASTEXITCODE"
}

if ($KainBin) {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain-math-bounce.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -KainBin $resolvedKain `
        -CompilerBuild auto
} else {
    & $compileScript `
        -Entry $entry `
        -OutputName "vulkain-math-bounce.exe" `
        -BazelConfig $BazelConfig `
        -VerifyLlvm `
        -KainBin $resolvedKain `
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
