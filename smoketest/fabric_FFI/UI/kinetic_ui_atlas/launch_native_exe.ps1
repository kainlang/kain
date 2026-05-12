param()

$ErrorActionPreference = "Stop"

$SmokeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SmokeRoot = (Resolve-Path $SmokeRoot).Path
$ExePath = Join-Path $SmokeRoot "native-app\kinetic-ui-atlas.exe"

if (-not (Test-Path $ExePath)) {
    throw "Expected executable was not found at $ExePath. Build it first with build_native_exe.ps1."
}

Start-Process -FilePath $ExePath -WorkingDirectory (Split-Path -Parent $ExePath)
