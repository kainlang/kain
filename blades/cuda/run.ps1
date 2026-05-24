param(
    [ValidateSet("dev", "release")]
    [string]$BazelConfig = "dev"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$bladeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $bladeRoot "..\..")
$compileScript = Join-Path $repoRoot ".agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1"
$prebuildScript = Join-Path $bladeRoot "build-cuda-bridge.ps1"
$entry = Join-Path $bladeRoot "src\main.kn"
$exePath = Join-Path $bladeRoot "cuda.exe"
$runRoot = Join-Path $bladeRoot ".kain\run"
$reportPath = Join-Path $runRoot "cuda_report.txt"
$gpuBmpPath = Join-Path $runRoot "cuda_gpu.bmp"
$cpuBmpPath = Join-Path $runRoot "cuda_cpu.bmp"
$diffBmpPath = Join-Path $runRoot "cuda_diff.bmp"

New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
foreach ($artifact in @($reportPath, $gpuBmpPath, $cpuBmpPath, $diffBmpPath)) {
    if (Test-Path $artifact) {
        Remove-Item -LiteralPath $artifact -Force
    }
}

& $prebuildScript
& $compileScript -Entry $entry -OutputName "cuda.exe" -BazelConfig $BazelConfig -VerifyLlvm

if (!(Test-Path $exePath)) {
    throw "Expected executable was not created: $exePath"
}

Push-Location $bladeRoot
try {
    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "cuda.exe exited with code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

foreach ($artifact in @($reportPath, $gpuBmpPath, $cpuBmpPath, $diffBmpPath)) {
    if (!(Test-Path $artifact)) {
        throw "Expected CUDA artifact was not created: $artifact"
    }
}

foreach ($bitmap in @($gpuBmpPath, $cpuBmpPath, $diffBmpPath)) {
    if ((Get-Item $bitmap).Length -le 1024) {
        throw "Bitmap artifact was too small: $bitmap"
    }
}

Write-Host "[PASS] CUDA blade built and executed: $exePath"
Write-Host "[PASS] report: $reportPath"
Write-Host "[PASS] gpu bmp: $gpuBmpPath"
Write-Host "[PASS] cpu bmp: $cpuBmpPath"
Write-Host "[PASS] diff bmp: $diffBmpPath"
