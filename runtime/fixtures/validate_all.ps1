param(
    [switch]$Help
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "Native runtime fixture validation wrapper"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File runtime\fixtures\validate_all.ps1"
    Write-Host ""
    Write-Host "This wrapper delegates to runtime\fixtures\validate_all.sh, which is the"
    Write-Host "canonical fixture validation lane for native LLVM/direct-C runtime proofs."
    exit 0
}

. (Join-Path $PSScriptRoot "..\scripts\runtime_windows_shell_helpers.ps1")

$exitCode = Invoke-KainBashScript -ScriptPath (Join-Path $PSScriptRoot "validate_all.sh")
exit $exitCode
