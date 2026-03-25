param(
    [string]$ConfigPath = (Join-Path $PSScriptRoot "kain-drive-map.json")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Normalize-DriveLetter {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Letter
    )

    $trimmed = $Letter.Trim().TrimEnd(':', '\')
    if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.Length -ne 1 -or -not [char]::IsLetter($trimmed[0])) {
        throw "Invalid drive letter '$Letter'."
    }

    return ([string]$trimmed[0]).ToUpperInvariant()
}

function Resolve-NormalizedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    $resolved = Resolve-Path -LiteralPath $PathValue -ErrorAction Stop
    return $resolved.ProviderPath.TrimEnd('\')
}

function Get-SubstMappings {
    $mappings = @{}

    foreach ($line in (cmd /c subst)) {
        if ($line -match '^([A-Z]):\\: => (.+)$') {
            $mappings[$matches[1]] = $matches[2].Trim().TrimEnd('\')
            continue
        }

        if ($line -match '^([A-Z]): => (.+)$') {
            $mappings[$matches[1]] = $matches[2].Trim().TrimEnd('\')
        }
    }

    return $mappings
}

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    throw "Drive map config not found: $ConfigPath"
}

$config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
if ($null -eq $config.driveLetter -or $null -eq $config.targetPath) {
    throw "Drive map config must include driveLetter and targetPath."
}

$driveLetter = Normalize-DriveLetter -Letter $config.driveLetter
$targetPath = Resolve-NormalizedPath -PathValue $config.targetPath
$currentMappings = Get-SubstMappings

if ($currentMappings.ContainsKey($driveLetter)) {
    $currentTarget = [string]$currentMappings[$driveLetter]
    if ($currentTarget -ieq $targetPath) {
        Write-Host "$driveLetter`: already mapped to $targetPath"
        exit 0
    }

    & subst "$driveLetter`:" /d | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to remove existing $driveLetter`: mapping."
    }
}

& subst "$driveLetter`:" $targetPath | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to map $driveLetter`: to $targetPath"
}

$updatedMappings = Get-SubstMappings
if (-not $updatedMappings.ContainsKey($driveLetter) -or ([string]$updatedMappings[$driveLetter]) -ine $targetPath) {
    throw "Verification failed for $driveLetter`: mapping."
}

Write-Host "Mapped $driveLetter`: to $targetPath"
