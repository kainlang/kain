$exe = Join-Path $PSScriptRoot "kg.exe"
if (!(Test-Path $exe)) {
    Write-Error "kg.exe not found next to this launcher. Build the `blades/kg` blade first."
    exit 1
}
& $exe @args
exit $LASTEXITCODE
