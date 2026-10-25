# Checks that every place a version is written agrees with ..\versions.txt (the one source the build and the
# packages take): the README table, each tool's doc title, and a "### <tool> <version>" heading in CHANGELOG.md.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tools = [ordered]@{
    ModLoader        = @{ Name = 'Avengers Mod Loader'; Doc = 'ModLoader.md' }
    SkillUnlock      = @{ Name = 'Skill Unlock'; Doc = 'SkillUnlock.md' }
    CollectionUnlock = @{ Name = 'Collection Unlock'; Doc = 'CollectionUnlock.md' }
    QuickStart       = @{ Name = 'Quick Start'; Doc = 'QuickStart.md' }
    SaveTool         = @{ Name = 'Avengers Save Tool'; Doc = 'SaveTool.md' }
}
$versions = @{}
foreach ($line in Get-Content (Join-Path $root 'versions.txt')) {
    if ($line -notmatch '^(\w+)=(\d+\.\d+)$') { throw "versions.txt: '$line' is not Tool=major.minor" }
    $versions[$Matches[1]] = $Matches[2]
}
$readme = Get-Content (Join-Path $root 'README.md') -Raw
$changelog = Get-Content (Join-Path $root 'CHANGELOG.md') -Raw
$problems = @()
foreach ($key in $tools.Keys) {
    $t = $tools[$key]; $v = $versions[$key]
    if (-not $v) { $problems += "versions.txt has no $key"; continue }
    $name = [regex]::Escape($t.Name); $ver = [regex]::Escape($v)
    if ($readme -notmatch "\|\s*\*\*$name\*\*\s*\|\s*$ver\s*\|") { $problems += "README.md: the $($t.Name) row is not version $v" }
    $title = (Get-Content (Join-Path $root "docs\$($t.Doc)") -TotalCount 1)
    if ($title -ne "# $($t.Name) $v") { $problems += "docs\$($t.Doc): the title is '$title', not '# $($t.Name) $v'" }
    if ($changelog -notmatch "(?m)^### $name $ver\b") { $problems += "CHANGELOG.md: no '### $($t.Name) $v' heading" }
}
foreach ($key in $versions.Keys) { if (-not $tools.Contains($key)) { $problems += "versions.txt: unknown tool $key" } }
if ($problems) { $problems | ForEach-Object { Write-Host "version check: $_" }; exit 1 }
Write-Host ("versions OK: " + (($tools.Keys | ForEach-Object { "$_ $($versions[$_])" }) -join ', '))
