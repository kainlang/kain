$hFiles = Get-ChildItem "X:\llvm-kain\_llvm_bak\llvm\include\llvm\IR\*.h"
$pattern = '\.inc"'
$results = @{}
foreach ($f in $hFiles) {
    $content = Get-Content $f.FullName -Raw
    $matches = [regex]::Matches($content, 'include\s+"[^"]+\.inc"')
    foreach ($m in $matches) {
        $ref = $m.Value -replace '.*"([^"]+)"', '$1'
        $results[$ref] = $f.Name
    }
}
$results.Keys | Sort-Object | ForEach-Object { Write-Host "$_ <- $($results[$_])" }
