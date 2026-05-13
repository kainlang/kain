param(
    [string]$Backend = "all",
    [string]$Mode = "full",
    [string]$Category,
    [switch]$ScriptVerbose,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "Native runtime conformance wrapper"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File runtime\conformance\run_all.ps1"
    Write-Host "  powershell -ExecutionPolicy Bypass -File runtime\conformance\run_all.ps1 -Backend llvm -ScriptVerbose"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Backend <name>      Forward --backend to run_all.sh"
    Write-Host "  -Mode <mode>         Forward --mode to run_all.sh"
    Write-Host "  -Category <name>     Forward --category to run_all.sh"
    Write-Host "  -ScriptVerbose       Forward --verbose to run_all.sh"
    exit 0
}

. (Join-Path $PSScriptRoot "..\scripts\runtime_windows_shell_helpers.ps1")

$scriptArgs = @("--backend", $Backend, "--mode", $Mode)
if ($Category) {
    $scriptArgs += @("--category", $Category)
}
if ($ScriptVerbose) {
    $scriptArgs += "--verbose"
}

$exitCode = Invoke-KainBashScript -ScriptPath (Join-Path $PSScriptRoot "run_all.sh") -ScriptArguments $scriptArgs
exit $exitCode
