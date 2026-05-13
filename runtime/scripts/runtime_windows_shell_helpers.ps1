Set-StrictMode -Version Latest

function ConvertTo-KainPosixPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LiteralPath
    )

    $resolvedPath = (Resolve-Path -LiteralPath $LiteralPath).Path
    $normalizedPath = $resolvedPath -replace "\\", "/"
    if ($normalizedPath -match "^([A-Za-z]):/(.*)$") {
        return "/$($matches[1].ToLowerInvariant())/$($matches[2])"
    }

    return $normalizedPath
}

function Resolve-KainBashExecutable {
    $candidatePaths = [System.Collections.Generic.List[string]]::new()

    if ($env:KAIN_BASH_PATH) {
        $candidatePaths.Add($env:KAIN_BASH_PATH)
    }

    foreach ($candidateName in @("bash.exe", "bash")) {
        $commandInfo = Get-Command -Name $candidateName -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -eq $commandInfo) {
            continue
        }

        $resolvedCandidate = $null
        if ($commandInfo.Path) {
            $resolvedCandidate = $commandInfo.Path
        } elseif ($commandInfo.Source) {
            $resolvedCandidate = $commandInfo.Source
        }

        if ($resolvedCandidate) {
            $candidatePaths.Add($resolvedCandidate)
        }
    }

    foreach ($fixedPath in @(
        "C:\Program Files\Git\bin\bash.exe",
        "C:\Program Files\Git\usr\bin\bash.exe",
        "C:\msys64\usr\bin\bash.exe"
    )) {
        $candidatePaths.Add($fixedPath)
    }

    foreach ($candidatePath in $candidatePaths | Select-Object -Unique) {
        if (-not $candidatePath) {
            continue
        }

        if (Test-Path -LiteralPath $candidatePath) {
            return (Resolve-Path -LiteralPath $candidatePath).Path
        }
    }

    throw "Unable to locate bash. Install Git Bash or MSYS2, or set KAIN_BASH_PATH to a bash executable."
}

function Invoke-KainBashScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,

        [string[]]$ScriptArguments = @()
    )

    $bashExecutable = Resolve-KainBashExecutable
    $posixScriptPath = ConvertTo-KainPosixPath -LiteralPath $ScriptPath

    & $bashExecutable $posixScriptPath @ScriptArguments
    return $LASTEXITCODE
}
