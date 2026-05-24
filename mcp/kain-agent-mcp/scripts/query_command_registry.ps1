param(
    [Parameter(Mandatory = $true)]
    [string]$KainBin,

    [string]$BinName = "kain",

    [ValidateSet("list", "describe")]
    [string]$Mode = "list",

    [string]$Query = "",

    [string]$Pack = "",

    [int]$Limit = 20,

    [switch]$Runtime
)

$ErrorActionPreference = "Stop"

function Get-CommandLines {
    $args = @("commands", "list", "--bin", $BinName)
    if ($Runtime) {
        $args += "--runtime"
    }

    $output = & $KainBin @args
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to read the Kain command registry."
    }

    return ($output -split "`r?`n") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
}

function Get-CommandPath {
    param([Parameter(Mandatory = $true)][string]$Line)

    $columns = $Line -split "  +" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    if ($columns.Count -eq 0) {
        return ""
    }
    return $columns[0].Trim()
}

$queryLower = $Query.Trim().ToLowerInvariant()
$packLower = $Pack.Trim().ToLowerInvariant()
$matches = New-Object System.Collections.Generic.List[string]

foreach ($line in (Get-CommandLines)) {
    $trimmed = $line.Trim()
    if ($packLower.Length -gt 0 -and ($trimmed.ToLowerInvariant().Contains("pack=$packLower") -eq $false)) {
        continue
    }

    if ($Mode -eq "describe") {
        $commandPath = (Get-CommandPath -Line $trimmed).ToLowerInvariant()
        if ($commandPath -eq $queryLower) {
            Write-Output $trimmed
            exit 0
        }
        if ($queryLower.Length -gt 0 -and $trimmed.ToLowerInvariant().Contains($queryLower) -and $matches.Count -eq 0) {
            $matches.Add($trimmed)
        }
        continue
    }

    if ($queryLower.Length -gt 0 -and ($trimmed.ToLowerInvariant().Contains($queryLower) -eq $false)) {
        continue
    }

    $matches.Add($trimmed)
    if ($matches.Count -ge $Limit) {
        break
    }
}

if ($Mode -eq "describe") {
    if ($matches.Count -gt 0) {
        Write-Output $matches[0]
        exit 0
    }
    exit 3
}

if ($matches.Count -gt 0) {
    $matches
}
