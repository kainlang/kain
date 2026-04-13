Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$patterns = @(
    @{ Pattern = "*.ilk"; Reason = "Linker/intermediate artifacts should never be tracked." },
    @{ Pattern = "runtime/conformance/**/bin/test_*"; Reason = "Conformance binaries are generated outputs." },
    @{ Pattern = ".zig-cache/**"; Reason = "Zig cache contents are local build outputs." },
    @{ Pattern = "*.pyc"; Reason = "Python bytecode should not be tracked." },
    @{ Pattern = "graphics_runtime_smoke_env_bundle.realtime_app.json"; Reason = "Root runtime smoke bundle output is generated." }
)

$stale = @()
foreach ($entry in $patterns) {
    $matches = git ls-files -- $entry.Pattern
    if ($LASTEXITCODE -ne 0) {
        Write-Error "git ls-files failed while checking pattern '$($entry.Pattern)'."
    }

    foreach ($path in $matches) {
        if ([string]::IsNullOrWhiteSpace($path)) {
            continue
        }

        $stale += [pscustomobject]@{
            Path = $path.Trim()
            Pattern = $entry.Pattern
            Reason = $entry.Reason
        }
    }
}

if ($stale.Count -gt 0) {
    Write-Host "Tracked stale artifacts detected:" -ForegroundColor Red
    $stale |
        Sort-Object Path -Unique |
        ForEach-Object {
            Write-Host (" - {0} ({1})" -f $_.Path, $_.Reason) -ForegroundColor Yellow
        }

    Write-Host ""
    Write-Host "Cleanup hint:" -ForegroundColor Cyan
    Write-Host "  git rm --cached <path>  # remove stale tracked artifacts"
    exit 1
}

Write-Host "No tracked stale artifacts found."
