$msvcLib = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64"
$currentLib = [Environment]::GetEnvironmentVariable("LIB", "Process")
$newLib = "$msvcLib;$currentLib"
[Environment]::SetEnvironmentVariable("LIB", $newLib, "Process")
Write-Host "LIB set to: $([Environment]::GetEnvironmentVariable('LIB','Process'))"
