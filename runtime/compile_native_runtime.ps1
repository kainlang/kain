param(
    [switch]$Release,
    [switch]$ScriptVerbose,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "KAIN native runtime compilation wrapper"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File runtime\compile_native_runtime.ps1"
    Write-Host "  powershell -ExecutionPolicy Bypass -File runtime\compile_native_runtime.ps1 -Release -ScriptVerbose"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Release            Forward --release to compile_native_runtime.sh"
    Write-Host "  -ScriptVerbose      Forward --verbose to compile_native_runtime.sh"
    exit 0
}

. (Join-Path $PSScriptRoot "scripts\runtime_windows_shell_helpers.ps1")

$scriptArgs = @()
if ($Release) {
    $scriptArgs += "--release"
}
if ($ScriptVerbose) {
    $scriptArgs += "--verbose"
}

$exitCode = Invoke-KainBashScript -ScriptPath (Join-Path $PSScriptRoot "compile_native_runtime.sh") -ScriptArguments $scriptArgs
exit $exitCode
