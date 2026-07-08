$srcDir = "X:\llvm-kain\clang\src"
$diags = @{}

Get-ChildItem $srcDir -Recurse -Filter *.cpp | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $matches = [regex]::Matches($content, 'diag::([a-zA-Z_][a-zA-Z0-9_]*)')
    foreach ($m in $matches) {
        $name = $m.Groups[1].Value
        $skipList = @('Severity','Flavor','Group','kind','CustomDiagInfo','getCustomDiagID','getDiagInfo',
                      'getDiagIDForStableID','getNumberOfCategories','getCategoryNameFromID',
                      'getCategoryIDFromName','getWarningOptionForDiag','getWarningOptionForGroup',
                      'getGroupForWarningOption','getDiagnosticFlags','getNearestOption',
                      'isUnrecoverable','IgnoreAll','isARCDiagnostic',
                      'NUM_BUILTIN_COMMON_DIAGNOSTICS','NUM_BUILTIN_DRIVER_DIAGNOSTICS',
                      'NUM_BUILTIN_FRONTEND_DIAGNOSTICS','NUM_BUILTIN_SERIALIZATION_DIAGNOSTICS',
                      'NUM_BUILTIN_LEX_DIAGNOSTICS','NUM_BUILTIN_PARSE_DIAGNOSTICS','NUM_BUILTIN_AST_DIAGNOSTICS',
                      'NUM_BUILTIN_COMMENT_DIAGNOSTICS','NUM_BUILTIN_CROSSTU_DIAGNOSTICS',
                      'NUM_BUILTIN_SEMA_DIAGNOSTICS','NUM_BUILTIN_ANALYSIS_DIAGNOSTICS',
                      'NUM_BUILTIN_REFACTORING_DIAGNOSTICS','NUM_BUILTIN_INSTALLAPI_DIAGNOSTICS',
                      'NUM_BUILTIN_TRAP_DIAGNOSTICS')
        if ($skipList -contains $name) { return }
        $diags[$name] = $true
    }
}

Get-ChildItem "X:\llvm-kain\clang\include\basic" -Filter *.h | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $matches = [regex]::Matches($content, 'diag::([a-zA-Z_][a-zA-Z0-9_]*)')
    foreach ($m in $matches) {
        $name = $m.Groups[1].Value
        if ($name -notin @('Severity','Flavor','Group','kind','CustomDiagInfo')) {
            $diags[$name] = $true
        }
    }
}

# Remove DIAG_UPPER_LIMIT which is already defined
$diags.Remove('DIAG_UPPER_LIMIT')

$sorted = $diags.Keys | Sort-Object
$outLines = @()
$outLines += "  // Auto-generated diagnostic IDs ($($sorted.Count) total)"
$outLines += "  enum {"
$i = 200
foreach ($name in $sorted) {
    $outLines += "    $name = DIAG_START_COMMON + $i,"
    $i++
}
$outLines += "  };"

# Now insert into DiagnosticIDs.h
$headerPath = "X:\llvm-kain\clang\include\basic\DiagnosticIDs.h"
$content = Get-Content $headerPath

# Find the insertion point: after "// Auto-generated diagnostic IDs" comment
$insertLine = -1
for ($j = 0; $j -lt $content.Count; $j++) {
    if ($content[$j] -match 'Auto-generated diagnostic IDs') {
        $insertLine = $j + 1
        break
    }
}

if ($insertLine -ge 0) {
    # Insert the enum lines after the comment line
    $newContent = @()
    for ($j = 0; $j -lt $content.Count; $j++) {
        $newContent += $content[$j]
        if ($j -eq $insertLine - 1) {
            # Insert the enum block
            foreach ($line in $outLines[1..($outLines.Count-1)]) {
                $newContent += $line
            }
        }
    }
    [System.IO.File]::WriteAllLines($headerPath, $newContent)
    Write-Host "Inserted $($sorted.Count) diagnostic entries into DiagnosticIDs.h"
} else {
    Write-Host "ERROR: Could not find insertion point"
}
