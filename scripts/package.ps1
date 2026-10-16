# Packs the built files (..\out, from scripts\build.bat) into the release archives in ..\dist.
# Each archive extracts straight into the game folder.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out  = Join-Path $root 'out'
$dist = Join-Path $root 'dist'
$versions = @{ ModLoader = '1.1'; SkillUnlock = '1.0'; SaveTool = '1.0' }

foreach ($f in 'version.dll', 'SkillUnlock.asi', 'AvengersSaveTool.exe') {
    if (-not (Test-Path (Join-Path $out $f))) { throw "Missing out\$f - run scripts\build.bat first." }
}
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory $dist | Out-Null

function New-Package($name, [hashtable]$files) {
    $stage = Join-Path $dist "_$name"
    foreach ($dest in $files.Keys) {
        $target = Join-Path $stage $dest
        New-Item -ItemType Directory (Split-Path $target) -Force | Out-Null
        Copy-Item $files[$dest] $target
    }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $dist "$name.zip")
    Remove-Item $stage -Recurse -Force
    Write-Host "$name.zip"
}

$lic = Join-Path $root 'LICENSE'
$docs = Join-Path $root 'docs'
New-Package "AvengersModLoader-$($versions.ModLoader)" @{
    'version.dll'                  = Join-Path $out 'version.dll'
    'mods\PUT_ASI_MODS_HERE.txt'   = Join-Path $root 'scripts\PUT_ASI_MODS_HERE.txt'
    'AvengersModLoader_README.txt' = Join-Path $docs 'ModLoader.md'
    'LICENSE.txt'                  = $lic }
New-Package "SkillUnlock-$($versions.SkillUnlock)" @{
    'mods\SkillUnlock.asi'   = Join-Path $out 'SkillUnlock.asi'
    'SkillUnlock_README.txt' = Join-Path $docs 'SkillUnlock.md'
    'LICENSE.txt'            = $lic }
New-Package "AvengersSaveTool-$($versions.SaveTool)" @{
    'AvengersSaveTool.exe'        = Join-Path $out 'AvengersSaveTool.exe'
    'AvengersSaveTool_README.txt' = Join-Path $docs 'SaveTool.md'
    'LICENSE.txt'                 = $lic }
