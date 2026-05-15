param(
    [Parameter(Mandatory = $true)]
    [string]$Kind,
    [string]$AppRoot,
    [string]$PayloadJson = "{}"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($AppRoot)) {
    $AppRoot = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) ".."
}

$AppRoot = (Resolve-Path $AppRoot).Path
$StateRoot = Join-Path $AppRoot "state"
$QueuePath = Join-Path $StateRoot "command_queue.jsonl"
New-Item -ItemType Directory -Force -Path $StateRoot | Out-Null

$Payload = $PayloadJson | ConvertFrom-Json
$Command = [ordered]@{
    id = [guid]::NewGuid().ToString()
    kind = $Kind
    issued_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    payload = $Payload
}

$CommandJson = $Command | ConvertTo-Json -Depth 8 -Compress
Add-Content -Path $QueuePath -Value $CommandJson
Write-Host "Queued $Kind into $QueuePath"
