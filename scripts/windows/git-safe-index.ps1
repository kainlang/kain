#!/usr/bin/env pwsh
# Run an index-mutating git command through a temporary index file, then
# stream-copy the result back into .git/index to avoid Windows rename failures
# like "fatal: unable to write new index file".
# Usage:
#   .\scripts\windows\git-safe-index.ps1 add -A
#   .\scripts\windows\git-safe-index.ps1 rm -r --cached generated

[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $GitArgs
)

$ErrorActionPreference = "Stop"

function Copy-FileBytesInPlace {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourcePath,
        [Parameter(Mandatory = $true)]
        [string] $DestinationPath
    )

    $maxAttempts = 8
    $buffer = New-Object byte[] 1048576

    for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
        try {
            $source = [System.IO.File]::Open(
                $SourcePath,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read,
                [System.IO.FileShare]::ReadWrite
            )
            try {
                $destination = [System.IO.File]::Open(
                    $DestinationPath,
                    [System.IO.FileMode]::Open,
                    [System.IO.FileAccess]::Write,
                    [System.IO.FileShare]::ReadWrite
                )
                try {
                    $destination.SetLength($source.Length)
                    while (($count = $source.Read($buffer, 0, $buffer.Length)) -gt 0) {
                        $destination.Write($buffer, 0, $count)
                    }
                    $destination.Flush()
                    return
                }
                finally {
                    $destination.Dispose()
                }
            }
            finally {
                $source.Dispose()
            }
        }
        catch [System.IO.IOException] {
            if ($attempt -eq $maxAttempts) {
                throw
            }
            Start-Sleep -Milliseconds 250
        }
    }
}

if (-not $GitArgs -or $GitArgs.Count -eq 0) {
    $GitArgs = @("add", "-A")
}

$repoRoot = (& git rev-parse --show-toplevel).Trim()
if (-not $repoRoot) {
    throw "Not inside a git repository."
}

$gitDirRaw = (& git rev-parse --git-dir).Trim()
if (-not $gitDirRaw) {
    throw "Could not resolve .git directory."
}

$gitDir = if ([System.IO.Path]::IsPathRooted($gitDirRaw)) {
    $gitDirRaw
} else {
    Join-Path $repoRoot $gitDirRaw
}

$liveIndexPath = Join-Path $gitDir "index"
$safeIndexPath = Join-Path $gitDir "index.safe-write"

if (-not (Test-Path $liveIndexPath)) {
    throw "Live git index not found at $liveIndexPath"
}

if (Test-Path (Join-Path $gitDir "index.lock")) {
    throw "Refusing to continue while $gitDir\index.lock exists. Clear the stale lock or stop the active git process first."
}

Copy-Item $liveIndexPath $safeIndexPath -Force

$previousIndexEnv = $env:GIT_INDEX_FILE
$env:GIT_INDEX_FILE = $safeIndexPath

try {
    & git @GitArgs
    $gitExitCode = $LASTEXITCODE
    if ($gitExitCode -ne 0) {
        exit $gitExitCode
    }

    Copy-FileBytesInPlace -SourcePath $safeIndexPath -DestinationPath $liveIndexPath
    Write-Host "Synced repaired index back to $liveIndexPath" -ForegroundColor Green
}
finally {
    if ([string]::IsNullOrWhiteSpace($previousIndexEnv)) {
        Remove-Item Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:GIT_INDEX_FILE = $previousIndexEnv
    }
    Remove-Item $safeIndexPath -ErrorAction SilentlyContinue
}
