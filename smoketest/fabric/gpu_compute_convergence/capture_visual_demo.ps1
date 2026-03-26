param(
    [int]$DelaySeconds = 4
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$SmokeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$GeneratedRoot = Join-Path $SmokeRoot "generated"
$NativeAppRoot = Join-Path $SmokeRoot "visual-native-app"
$ScreenshotPath = Join-Path $GeneratedRoot "fabric_gpu_visual_showcase.png"

New-Item -ItemType Directory -Force -Path $GeneratedRoot | Out-Null

$ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter *.exe -File | Select-Object -First 1
if ($null -eq $ExecutablePath) {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $SmokeRoot "build_visual_exe.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build the Fabric visual executable before capture."
    }
    $ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter *.exe -File | Select-Object -First 1
}

if ($null -eq $ExecutablePath) {
    throw "No visual executable exists to capture."
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$Process = Start-Process -FilePath $ExecutablePath.FullName -PassThru
try {
    Start-Sleep -Seconds $DelaySeconds
    $Bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $Bitmap = New-Object System.Drawing.Bitmap $Bounds.Width, $Bounds.Height
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    $Graphics.CopyFromScreen($Bounds.Location, [System.Drawing.Point]::Empty, $Bounds.Size)
    $Bitmap.Save($ScreenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $Graphics.Dispose()
    $Bitmap.Dispose()
    Write-Host "Screenshot captured to $ScreenshotPath"
}
finally {
    if ($null -ne $Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
    }
}
