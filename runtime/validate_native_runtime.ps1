param(
    [switch]$Release,
    [switch]$SkipCliBuild,
    [switch]$SkipRuntimeBuild,
    [switch]$SkipFixtures,
    [switch]$SkipConformance,
    [switch]$ScriptVerbose,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "Aggregate native runtime validation wrapper"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File runtime\validate_native_runtime.ps1"
    Write-Host "  powershell -ExecutionPolicy Bypass -File runtime\validate_native_runtime.ps1 -Release -ScriptVerbose"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Release             Forward release mode to runtime compilation"
    Write-Host "  -SkipCliBuild        Skip cargo build -p cli"
    Write-Host "  -SkipRuntimeBuild    Skip runtime\compile_native_runtime.ps1"
    Write-Host "  -SkipFixtures        Skip runtime\fixtures\validate_all.ps1"
    Write-Host "  -SkipConformance     Skip runtime\conformance\run_all.ps1"
    Write-Host "  -ScriptVerbose       Forward verbose output to wrapper scripts"
    exit 0
}

function Invoke-NativeStep {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Action
    )

    Write-Host "==> $Label" -ForegroundColor Cyan
    & $Action
    Write-Host ""
}

$runtimeRoot = $PSScriptRoot
$compileWrapperPath = Join-Path $runtimeRoot "compile_native_runtime.ps1"
$fixturesWrapperPath = Join-Path $runtimeRoot "fixtures\validate_all.ps1"
$conformanceWrapperPath = Join-Path $runtimeRoot "conformance\run_all.ps1"

if (-not $SkipCliBuild) {
    Invoke-NativeStep -Label "Building CLI compiler host" -Action {
        & cargo build -p cli
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (-not $SkipRuntimeBuild) {
    Invoke-NativeStep -Label "Compiling manifest-driven native runtime bundle" -Action {
        $compileArgs = @()
        if ($Release) {
            $compileArgs += "-Release"
        }
        if ($ScriptVerbose) {
            $compileArgs += "-ScriptVerbose"
        }
        & $compileWrapperPath @compileArgs
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (-not $SkipFixtures) {
    Invoke-NativeStep -Label "Running native fixture suite" -Action {
        & $fixturesWrapperPath
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if (-not $SkipConformance) {
    Invoke-NativeStep -Label "Running native conformance suite" -Action {
        $conformanceArgs = @()
        if ($ScriptVerbose) {
            $conformanceArgs += "-ScriptVerbose"
        }
        & $conformanceWrapperPath @conformanceArgs
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

if ($SkipCliBuild -and $SkipRuntimeBuild -and $SkipFixtures -and $SkipConformance) {
    Write-Host "No validation steps selected."
    exit 0
}

Write-Host "Native runtime validation completed successfully." -ForegroundColor Green
