# package_release.ps1 - stage and zip the VR program overlay.
# Usage (PowerShell, after ./build_pc.sh):
#   powershell -File pc/tools/package_release.ps1 -Version v1.0
# Output and a unique staging directory are kept in ignored pc/build32/releases/.
# The archive contains program files, documentation and license notices only.
# It never copies ROMs, saves, settings, texture packs or arbitrary bin contents.

param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$')]
    [string]$Version
)

$ErrorActionPreference = "Stop"
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$bin = Join-Path $repo "pc\build32\bin"
$releaseDir = Join-Path $repo "pc\build32\releases"
$zip = Join-Path $releaseDir ("AnimalCrossing-VR-" + $Version + "-win32.zip")

if (Test-Path -LiteralPath $zip) {
    throw "Output already exists; choose another version or archive it first: $zip"
}

# Explicit runtime allowlist: old test executables, logs and custom bindings in
# bin/ must not become part of a public release.
$payload = [ordered]@{}
foreach ($relative in @(
    "AnimalCrossing.exe",
    "SDL2.dll",
    "openvr_api.dll",
    "shaders/default.vert",
    "shaders/default.frag",
    "vr_actions/actionmanifest.json",
    "vr_actions/bindings_oculus_touch.json",
    "vr_actions/bindings_knuckles.json"
)) {
    $payload[$relative] = Join-Path $bin $relative
}

foreach ($relative in @(
    "README.md", "VR_README.md", "VR_PLAYTEST.md", "FAQ.md", "LICENSE",
    "pc/VR_ARCHITECTURE.md", "pc/DOCUMENTATION.md",
    "quest/README.md", "quest/INSTALL.md", "quest/MANUAL.md"
)) {
    $payload[$relative] = Join-Path $repo $relative
}
foreach ($document in Get-ChildItem -LiteralPath (Join-Path $repo "docs") -Filter "*.md" -File | Sort-Object Name) {
    $payload["docs/" + $document.Name] = $document.FullName
}
$payload["licenses/LICENSE-OpenVR.txt"] = Join-Path $repo "pc\lib\openvr\LICENSE"
$payload["licenses/LICENSE-SDL2.txt"] = Join-Path $repo "pc\licenses\LICENSE-SDL2.txt"

# Fail before staging if any required component is missing.
foreach ($entry in $payload.GetEnumerator()) {
    if (!(Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "Required release file is missing (build first): $($entry.Value)"
    }
}
$commit = & git -C $repo rev-parse --verify HEAD
if ($LASTEXITCODE -ne 0) { throw "Cannot identify the packaging checkout's Git commit" }
$status = & git -C $repo status --porcelain
if ($LASTEXITCODE -ne 0) { throw "Cannot inspect the packaging checkout's Git status" }
$workingTreeDirty = @($status).Count -ne 0

# Fresh staging avoids recursive deletion and cross-contamination from a prior
# package. Preserve it alongside the zip so its manifest can be inspected.
New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
$stageParent = Join-Path $releaseDir ("stage-" + [Guid]::NewGuid().ToString("N"))
$stage = Join-Path $stageParent "AnimalCrossing-VR"
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($entry in $payload.GetEnumerator()) {
    $destination = Join-Path $stage $entry.Key
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath $entry.Value -Destination $destination
}

$exeHash = (Get-FileHash -LiteralPath (Join-Path $stage "AnimalCrossing.exe") -Algorithm SHA256).Hash
$utf8 = New-Object System.Text.UTF8Encoding($false)
$buildInfo = @(
    "Version: $Version",
    "Git commit (packaging checkout): $commit",
    "Working tree dirty: $workingTreeDirty",
    "AnimalCrossing.exe SHA256: $exeHash"
)
[IO.File]::WriteAllLines((Join-Path $stage "BUILD_INFO.txt"), [string[]]$buildInfo, $utf8)

# SHA256SUMS covers every staged file except itself; paths are relative to the
# AnimalCrossing-VR folder and use forward slashes for common checksum tools.
$sumPaths = @($payload.Keys) + "BUILD_INFO.txt"
$sums = foreach ($relative in $sumPaths | Sort-Object) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $stage $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $relative"
}
[IO.File]::WriteAllLines((Join-Path $stage "SHA256SUMS.txt"), [string[]]$sums, $utf8)

# No -Force: a concurrent/package-existing output must never be overwritten.
Compress-Archive -LiteralPath $stage -DestinationPath $zip
Write-Host ("Packaged: " + $zip)
Write-Host ("Staging: " + $stage)
