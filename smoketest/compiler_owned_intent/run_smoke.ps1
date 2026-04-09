param()

$ErrorActionPreference = "Stop"

$SmokeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $SmokeDir "..\..")).Path
$SourcePath = Join-Path $SmokeDir "compiler_owned_intent.kn"
$OutputDir = Join-Path $SmokeDir "output"
$LlvmOutputPath = Join-Path $OutputDir "compiler_owned_intent.ll"
$RuntimeContractPath = Join-Path $OutputDir "compiler_owned_intent.runtime_contract.json"
$RealtimeBundlePath = Join-Path $OutputDir "compiler_owned_intent.realtime_app.json"
$SummaryPath = Join-Path $OutputDir "summary.txt"

function Resolve-KainBinary {
    $candidates = @(
        (Join-Path $RepoRoot "target\debug\kain.exe"),
        (Join-Path $RepoRoot "target\release\kain.exe"),
        (Join-Path $RepoRoot "target\debug\kain"),
        (Join-Path $RepoRoot "target\release\kain")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    $fromPath = Get-Command kain -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    throw "Unable to resolve the kain binary."
}

function Assert-Contains {
    param(
        [string[]]$Values,
        [string]$ExpectedValue,
        [string]$Label
    )

    if ($Values -notcontains $ExpectedValue) {
        throw "Missing expected $Label '$ExpectedValue'."
    }
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$kain = Resolve-KainBinary

$runOutput = & $kain run $SourcePath 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "kain run failed: $($runOutput | Out-String)"
}
$runText = $runOutput | Out-String
if ($runText -notmatch "(^|\s)48(\s|$)") {
    throw "Expected kain run to return 48. Output: $runText"
}

$buildOutput = & $kain $SourcePath -t llvm -o $LlvmOutputPath 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "LLVM bundle staging failed: $($buildOutput | Out-String)"
}

if (-not (Test-Path $RuntimeContractPath)) {
    throw "Missing runtime contract artifact: $RuntimeContractPath"
}
if (-not (Test-Path $RealtimeBundlePath)) {
    throw "Missing realtime bundle artifact: $RealtimeBundlePath"
}

$runtimeContract = Get-Content -Raw -Path $RuntimeContractPath | ConvertFrom-Json
$realtimeBundle = Get-Content -Raw -Path $RealtimeBundlePath | ConvertFrom-Json

if (@($runtimeContract.patches).Count -lt 1) { throw "Runtime contract is missing patches[] output." }
if (@($runtimeContract.converges).Count -lt 1) { throw "Runtime contract is missing converges[] output." }
if (@($runtimeContract.worlds).Count -lt 1) { throw "Runtime contract is missing worlds[] output." }
if (@($runtimeContract.orchestrations).Count -lt 1) { throw "Runtime contract is missing orchestrations[] output." }
if (@($runtimeContract.worlds[0].surfaces).Count -ne 4) {
    throw "Runtime contract world surface projection count was not 4."
}

$runtimeCapabilities = @($runtimeContract.required_capabilities | ForEach-Object { $_.key })
foreach ($requiredCapability in @(
    "patch.transactions",
    "converge.dispatch",
    "world.native-ui",
    "world.viewport3d",
    "world.web",
    "world.ue5",
    "orchestrate.pipeline"
)) {
    Assert-Contains -Values $runtimeCapabilities -ExpectedValue $requiredCapability -Label "runtime capability"
}

if (@($realtimeBundle.patches).Count -lt 1) { throw "Realtime bundle is missing patches[] output." }
if (@($realtimeBundle.converges).Count -lt 1) { throw "Realtime bundle is missing converges[] output." }
if (@($realtimeBundle.worlds).Count -lt 1) { throw "Realtime bundle is missing worlds[] output." }
if (@($realtimeBundle.orchestrations).Count -lt 1) { throw "Realtime bundle is missing orchestrations[] output." }
if (@($realtimeBundle.worlds[0].surfaces).Count -ne 4) {
    throw "Realtime bundle world surface projection count was not 4."
}

$toolCaps = @($realtimeBundle.tool_caps)
foreach ($requiredToolCap in @(
    "patch.transactions",
    "converge.dispatch",
    "world.native-ui",
    "world.viewport3d",
    "world.web",
    "world.ue5",
    "orchestrate.pipeline"
)) {
    Assert-Contains -Values $toolCaps -ExpectedValue $requiredToolCap -Label "tool capability"
}

$summaryLines = @(
    "kain: $kain",
    "run_result: 48",
    "llvm_output: $LlvmOutputPath",
    "runtime_contract: $RuntimeContractPath",
    "realtime_bundle: $RealtimeBundlePath"
)
Set-Content -Path $SummaryPath -Value $summaryLines
