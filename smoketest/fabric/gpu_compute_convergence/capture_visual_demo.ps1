param(
    [int]$DelaySeconds = 4,
    [switch]$Release
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$SmokeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$GeneratedRoot = Join-Path $SmokeRoot "generated"
$NativeAppRoot = Join-Path $SmokeRoot "visual-native-app"
$ExpectedExecutableName = "fabric-studio-3d-editor.exe"
$ScreenshotPath = Join-Path $GeneratedRoot "fabric_gpu_visual_showcase.png"

New-Item -ItemType Directory -Force -Path $GeneratedRoot | Out-Null

$ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter $ExpectedExecutableName -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $ExecutablePath) {
    $ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter *.exe -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
}
if ($null -eq $ExecutablePath) {
    $BuildArguments = @(
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        (Join-Path $SmokeRoot "build_visual_exe.ps1")
    )
    if ($Release) {
        $BuildArguments += "-Release"
    }
    & powershell @BuildArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build the Fabric visual executable before capture."
    }
    $ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter $ExpectedExecutableName -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $ExecutablePath) {
        $ExecutablePath = Get-ChildItem -Path $NativeAppRoot -Filter *.exe -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
    }
}

if ($null -eq $ExecutablePath) {
    throw "No visual executable exists to capture."
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class FabricCaptureUser32 {
    [DllImport("user32.dll")]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
"@

$Process = Start-Process -FilePath $ExecutablePath.FullName -PassThru
try {
    Start-Sleep -Seconds $DelaySeconds
    $Process.Refresh()
    $WindowHandle = $Process.MainWindowHandle
    if ($WindowHandle -eq [IntPtr]::Zero) {
        Start-Sleep -Seconds 2
        $Process.Refresh()
        $WindowHandle = $Process.MainWindowHandle
    }
    if ($WindowHandle -eq [IntPtr]::Zero) {
        throw "Native editor window handle was not available for capture."
    }

    [FabricCaptureUser32]::ShowWindowAsync($WindowHandle, 3) | Out-Null
    Start-Sleep -Milliseconds 700

    $Rect = New-Object FabricCaptureUser32+RECT
    if (-not [FabricCaptureUser32]::GetWindowRect($WindowHandle, [ref]$Rect)) {
        throw "Failed to query the native editor window bounds."
    }

    $Width = [Math]::Max(1, $Rect.Right - $Rect.Left)
    $Height = [Math]::Max(1, $Rect.Bottom - $Rect.Top)
    $Bitmap = New-Object System.Drawing.Bitmap $Width, $Height
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    $Graphics.CopyFromScreen(
        (New-Object System.Drawing.Point($Rect.Left, $Rect.Top)),
        [System.Drawing.Point]::Empty,
        (New-Object System.Drawing.Size($Width, $Height))
    )
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
