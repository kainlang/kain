param(
    [string]$ManifestPath = (Join-Path $PSScriptRoot "agent-tooling.manifest.json"),
    [switch]$DryRun,
    [switch]$SkipScoop,
    [switch]$SkipWinget,
    [switch]$SkipCargo,
    [switch]$SkipPython,
    [switch]$SkipHeavyWindowsKits
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Section {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Title
    )

    Write-Host ""
    Write-Host ("== " + $Title + " ==") -ForegroundColor Cyan
}

function Test-CommandAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList,
        [string]$Description
    )

    $display = $FilePath + " " + ($ArgumentList -join " ")
    if (-not [string]::IsNullOrWhiteSpace($Description)) {
        Write-Host ("-> " + $Description) -ForegroundColor DarkCyan
    }
    Write-Host ("   " + $display) -ForegroundColor DarkGray

    if ($DryRun) {
        return
    }

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw ("Command failed with exit code " + $LASTEXITCODE + ": " + $display)
    }
}

function Get-PythonLauncher {
    if (Test-CommandAvailable -Name "python") {
        return [pscustomobject]@{
            FilePath = "python"
            PrefixArgs = @()
        }
    }

    if (Test-CommandAvailable -Name "py") {
        return [pscustomobject]@{
            FilePath = "py"
            PrefixArgs = @("-3")
        }
    }

    return $null
}

function Get-ScoopInstalledSet {
    $installed = @{}
    if (-not (Test-CommandAvailable -Name "scoop")) {
        return $installed
    }

    $exportJson = & scoop export 2>$null
    if ([string]::IsNullOrWhiteSpace(($exportJson | Out-String))) {
        return $installed
    }

    $export = $exportJson | ConvertFrom-Json
    foreach ($app in $export.apps) {
        if ($null -ne $app.Name) {
            $installed[([string]$app.Name).ToLowerInvariant()] = $true
        }
    }

    return $installed
}

function Get-ScoopBucketSet {
    $buckets = @{}
    if (-not (Test-CommandAvailable -Name "scoop")) {
        return $buckets
    }

    $exportJson = & scoop export 2>$null
    if ([string]::IsNullOrWhiteSpace(($exportJson | Out-String))) {
        return $buckets
    }

    $export = $exportJson | ConvertFrom-Json
    foreach ($bucket in $export.buckets) {
        if ($null -ne $bucket.Name) {
            $buckets[([string]$bucket.Name).ToLowerInvariant()] = $true
        }
    }

    return $buckets
}

function Install-ScoopLayer {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Config
    )

    if (-not (Test-CommandAvailable -Name "scoop")) {
        throw "Scoop is required for the Scoop layer but was not found on PATH."
    }

    Write-Section "Scoop Packages"
    $bucketSet = Get-ScoopBucketSet
    foreach ($bucket in $Config.scoopBuckets) {
        $bucketName = [string]$bucket
        if ($bucketSet.ContainsKey($bucketName.ToLowerInvariant())) {
            Write-Host ("[skip] scoop bucket " + $bucketName + " already present") -ForegroundColor Yellow
            continue
        }

        Invoke-External -FilePath "scoop" -ArgumentList @("bucket", "add", $bucketName) -Description ("Add Scoop bucket " + $bucketName)
    }

    $installedSet = Get-ScoopInstalledSet
    foreach ($package in $Config.scoopPackages) {
        $name = [string]$package.name
        $candidateNames = @($name)
        if ($package.PSObject.Properties.Name -contains "installedNames") {
            foreach ($installedName in $package.installedNames) {
                $candidateNames += [string]$installedName
            }
        }

        $alreadyInstalled = $false
        foreach ($candidateName in $candidateNames) {
            if ($installedSet.ContainsKey($candidateName.ToLowerInvariant())) {
                $alreadyInstalled = $true
                break
            }
        }

        if ($alreadyInstalled) {
            Write-Host ("[skip] scoop package " + $name + " already installed") -ForegroundColor Yellow
            continue
        }

        Invoke-External -FilePath "scoop" -ArgumentList @("install", $name) -Description ([string]$package.reason)
    }
}

function Test-AppVerifierInstalled {
    $searchRoots = @(
        "C:\Program Files (x86)\Windows Kits",
        "C:\Program Files\Windows Kits"
    )

    foreach ($root in $searchRoots) {
        if (-not (Test-Path -LiteralPath $root)) {
            continue
        }

        $match = Get-ChildItem -LiteralPath $root -Filter "appverif.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $match) {
            return $true
        }
    }

    return $false
}

function Test-WptInstalled {
    return (Test-CommandAvailable -Name "wpr") -or (Test-CommandAvailable -Name "wpa")
}

function Invoke-WingetInstall {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageId,
        [Parameter(Mandatory = $true)]
        [string]$DisplayName,
        [Parameter(Mandatory = $true)]
        [string]$Reason
    )

    $args = @(
        "install",
        "--id", $PackageId,
        "--exact",
        "--source", "winget",
        "--accept-package-agreements",
        "--accept-source-agreements",
        "--disable-interactivity"
    )
    Invoke-External -FilePath "winget" -ArgumentList $args -Description ($DisplayName + " - " + $Reason)
}

function Install-WingetLayer {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Config
    )

    if (-not (Test-CommandAvailable -Name "winget")) {
        throw "Winget is required for the WinDbg/SDK layer but was not found on PATH."
    }

    Write-Section "Winget Packages"
    foreach ($package in $Config.wingetPackages) {
        $group = [string]$package.group
        $displayName = [string]$package.displayName
        $reason = [string]$package.reason

        if ($SkipHeavyWindowsKits -and $group -eq "heavy") {
            Write-Host ("[skip] heavy Windows kit " + $displayName) -ForegroundColor Yellow
            continue
        }

        if ($displayName -eq "Windows SDK" -and (Test-AppVerifierInstalled)) {
            Write-Host "[skip] Application Verifier already present through an installed Windows SDK" -ForegroundColor Yellow
            continue
        }

        if ($displayName -eq "Windows ADK" -and (Test-WptInstalled)) {
            Write-Host "[skip] WPR/WPA already present on PATH" -ForegroundColor Yellow
            continue
        }

        $candidateIds = @([string]$package.id)
        if ($package.PSObject.Properties.Name -contains "alternateIds" -and $null -ne $package.alternateIds) {
            foreach ($alternate in $package.alternateIds) {
                $candidateIds += [string]$alternate
            }
        }

        $installed = $false
        foreach ($candidateId in $candidateIds) {
            try {
                Invoke-WingetInstall -PackageId $candidateId -DisplayName $displayName -Reason $reason
                $installed = $true
                break
            }
            catch {
                if ($candidateId -eq $candidateIds[-1]) {
                    throw
                }

                Write-Host ("[warn] winget id failed, trying fallback: " + $candidateId) -ForegroundColor Yellow
            }
        }

        if (-not $installed) {
            throw "Failed to install $displayName via winget."
        }
    }
}

function Get-CargoInstalledSet {
    $installed = @{}
    if (-not (Test-CommandAvailable -Name "cargo")) {
        return $installed
    }

    foreach ($line in (& cargo install --list 2>$null)) {
        if ($line -match '^([A-Za-z0-9._+-]+)\s+v[0-9]') {
            $installed[$matches[1].ToLowerInvariant()] = $true
        }
    }

    return $installed
}

function Install-CargoLayer {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Config
    )

    if (-not (Test-CommandAvailable -Name "cargo")) {
        throw "Cargo is required for the Rust tooling layer but was not found on PATH."
    }

    Write-Section "Cargo Tools"
    $installedSet = Get-CargoInstalledSet
    foreach ($crate in $Config.cargoCrates) {
        $name = [string]$crate.name
        if ($installedSet.ContainsKey($name.ToLowerInvariant())) {
            Write-Host ("[skip] cargo crate " + $name + " already installed") -ForegroundColor Yellow
            continue
        }

        try {
            Invoke-External -FilePath "cargo" -ArgumentList @("install", "--locked", $name) -Description ([string]$crate.reason)
        }
        catch {
            if ($DryRun) {
                throw
            }

            Write-Host ("[warn] retrying cargo install without --locked for " + $name) -ForegroundColor Yellow
            Invoke-External -FilePath "cargo" -ArgumentList @("install", $name) -Description ([string]$crate.reason)
        }
    }
}

function Install-PythonLayer {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Config
    )

    $pythonLauncher = Get-PythonLauncher
    if ($null -eq $pythonLauncher) {
        throw "Python was not found on PATH."
    }

    Write-Section "Python Toolbox"
    $packages = @()
    foreach ($package in $Config.pythonToolbox.packages) {
        $packages += [string]$package.name
    }

    $launcherFile = [string]$pythonLauncher.FilePath
    $launcherArgs = @($pythonLauncher.PrefixArgs)
    Invoke-External -FilePath $launcherFile -ArgumentList ($launcherArgs + @("-m", "pip", "install", "--user", "--upgrade") + $packages) -Description "Install Python testing and profiling toolbox"
}

if (-not (Test-Path -LiteralPath $ManifestPath)) {
    throw "Tooling manifest not found: $ManifestPath"
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json

Write-Section "Environment"
Write-Host ("Dry run: " + [bool]$DryRun)
Write-Host ("Skip heavy Windows kits: " + [bool]$SkipHeavyWindowsKits)
Write-Host ("SCOOP: " + $env:SCOOP)
Write-Host ("SCOOP_GLOBAL: " + $env:SCOOP_GLOBAL)
Write-Host ("CARGO_HOME: " + $env:CARGO_HOME)
$pythonLauncher = Get-PythonLauncher
if ($null -eq $pythonLauncher) {
    Write-Host "Python launcher: <missing>"
}
else {
    $launcherDisplay = @([string]$pythonLauncher.FilePath) + @($pythonLauncher.PrefixArgs)
    Write-Host ("Python launcher: " + ($launcherDisplay -join " "))
}

if (-not $SkipScoop) {
    Install-ScoopLayer -Config $manifest
}

if (-not $SkipWinget) {
    Install-WingetLayer -Config $manifest
}

if (-not $SkipCargo) {
    Install-CargoLayer -Config $manifest
}

if (-not $SkipPython) {
    Install-PythonLayer -Config $manifest
}

Write-Section "Done"
Write-Host "Agent tooling bootstrap finished."
Write-Host ("Manifest: " + $ManifestPath)
Write-Host "No path or tool-root overrides were written by this script."
