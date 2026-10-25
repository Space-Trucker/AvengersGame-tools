# Packs the built files (..\out, from scripts\build.bat) into the release archives in ..\dist.
# Each archive extracts straight into the game folder. The versions come from ..\versions.txt; the .pdb files in out\
# are never packaged. Quick Start ships no QuickStart.ini: it writes one at its first start, so an update keeps the
# player's settings.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out  = Join-Path $root 'out'
$dist = Join-Path $root 'dist'
$versions = @{}
foreach ($line in Get-Content (Join-Path $root 'versions.txt')) {
    if ($line -notmatch '^(\w+)=(\d+\.\d+)$') { throw "versions.txt: '$line' is not Tool=major.minor" }
    $versions[$Matches[1]] = $Matches[2]
}

foreach ($f in 'version.dll', 'SkillUnlock.asi', 'CollectionUnlock.asi', 'QuickStart.asi', 'AvengersSaveTool.exe') {
    if (-not (Test-Path (Join-Path $out $f))) { throw "Missing out\$f - run scripts\build.bat first." }
}
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory $dist | Out-Null

Add-Type -AssemblyName System.IO.Compression

# Entry names use '/', as zip files must: Windows PowerShell 5.1's Compress-Archive writes '\', which unzip tools on
# other systems (Steam Deck) take as part of the file name. ZipArchive is in System.IO.Compression in both Windows
# PowerShell and PowerShell 7.
function New-Package($name, [hashtable]$files) {
    $stream = [System.IO.File]::Open((Join-Path $dist "$name.zip"), [System.IO.FileMode]::CreateNew)
    $zip = New-Object System.IO.Compression.ZipArchive($stream, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($dest in ($files.Keys | Sort-Object)) {
            $entry = $zip.CreateEntry($dest.Replace('\', '/'), [System.IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = (Get-Item $files[$dest]).LastWriteTime
            $in = [System.IO.File]::OpenRead($files[$dest])
            $out = $entry.Open()
            try { $in.CopyTo($out) } finally { $out.Dispose(); $in.Dispose() }
        }
    } finally {
        $zip.Dispose()
        $stream.Dispose()
    }
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
New-Package "CollectionUnlock-$($versions.CollectionUnlock)" @{
    'mods\CollectionUnlock.asi'   = Join-Path $out 'CollectionUnlock.asi'
    'CollectionUnlock_README.txt' = Join-Path $docs 'CollectionUnlock.md'
    'LICENSE.txt'               = $lic }
New-Package "QuickStart-$($versions.QuickStart)" @{
    'mods\QuickStart.asi'    = Join-Path $out 'QuickStart.asi'
    'QuickStart_README.txt'  = Join-Path $docs 'QuickStart.md'
    'LICENSE.txt'            = $lic }
New-Package "AvengersSaveTool-$($versions.SaveTool)" @{
    'AvengersSaveTool.exe'        = Join-Path $out 'AvengersSaveTool.exe'
    'AvengersSaveTool_README.txt' = Join-Path $docs 'SaveTool.md'
    'LICENSE.txt'                 = $lic }
