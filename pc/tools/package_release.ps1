# package_release.ps1 - stage and zip a release build.
# Usage (PowerShell, after ./build_pc.sh):
#   powershell -File pc/tools/package_release.ps1 -Version v1.0
# Produces AnimalCrossing-VR-<Version>-win32.zip next to the repo root.
#
# Includes everything a player needs (and the license notices required for
# redistribution). Never place a ROM in the staging tree.

param(
    [Parameter(Mandatory = $true)][string]$Version
)

$ErrorActionPreference = "Stop"
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$bin = Join-Path $repo "pc\build32\bin"
$stage = Join-Path $env:TEMP "acvr-stage\AnimalCrossing-VR"

if (!(Test-Path (Join-Path $bin "AnimalCrossing.exe"))) {
    throw "Build first: pc/build32/bin/AnimalCrossing.exe not found"
}

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

Copy-Item (Join-Path $bin "AnimalCrossing.exe") $stage
Copy-Item (Join-Path $bin "SDL2.dll") $stage
Copy-Item (Join-Path $bin "openvr_api.dll") $stage
Copy-Item -Recurse (Join-Path $bin "shaders") $stage
Copy-Item -Recurse (Join-Path $bin "vr_actions") $stage
New-Item -ItemType Directory -Force (Join-Path $stage "rom"), (Join-Path $stage "save"), (Join-Path $stage "texture_pack") | Out-Null

# Docs + license notices (MIT/CC0 chain + OpenVR BSD-3 for openvr_api.dll)
Copy-Item (Join-Path $repo "README.md") $stage
Copy-Item (Join-Path $repo "VR_README.md") $stage
Copy-Item (Join-Path $repo "VR_PLAYTEST.md") $stage
Copy-Item (Join-Path $repo "LICENSE") $stage
New-Item -ItemType Directory -Force (Join-Path $stage "licenses") | Out-Null
Copy-Item (Join-Path $repo "pc\lib\openvr\LICENSE") (Join-Path $stage "licenses\LICENSE-OpenVR.txt")

$zip = Join-Path $repo ("AnimalCrossing-VR-" + $Version + "-win32.zip")
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host ("Packaged: " + $zip)
