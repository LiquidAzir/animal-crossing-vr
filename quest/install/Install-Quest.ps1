# Public Quest installer. Windows PowerShell 5.1; no Python or Android SDK needed
# beyond an existing adb executable. Never launches/stops the game or replaces data.
[CmdletBinding()]
param(
    [string]$AdbPath,
    [string]$RomPath,
    [switch]$NonInteractive
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$QuestPackage = 'com.liquidazir.animalcrossingquest'
$QuestSerial = $null
$AppMarker = "__ACQUEST_INSTALL_OK__`n"

function Quote-ProcessArgument([string]$Value) {
    # CommandLineToArgvW / C runtime rules, including trailing backslashes.
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}

function Invoke-Adb([string[]]$AdbArguments, [string]$InputFile = '') {
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $script:AdbPath
    $start.Arguments = (($AdbArguments | ForEach-Object { Quote-ProcessArgument $_ }) -join ' ')
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    $outputBytes = New-Object System.IO.MemoryStream
    $sourceStream = $null
    $started = $false
    try {
        if (!$process.Start()) { throw 'Could not start ADB.' }
        $started = $true
        $readTask = $process.StandardOutput.BaseStream.CopyToAsync($outputBytes)
        $errorTask = $process.StandardError.ReadToEndAsync()
        if ($InputFile) {
            $sourceStream = [System.IO.File]::OpenRead($InputFile)
            # Use a pipe and binary streams: text redirection truncates/corrupts
            # disc images (notably Ctrl-Z bytes on Windows).
            $writeTask = $sourceStream.CopyToAsync($process.StandardInput.BaseStream)
            if (!$writeTask.Wait(600000)) { throw 'ADB data transfer timed out.' }
        }
        $process.StandardInput.Close()
        if (!$process.WaitForExit(600000)) { throw 'ADB operation timed out.' }
        $null = $readTask.GetAwaiter().GetResult()
        $errorText = $errorTask.GetAwaiter().GetResult()
        return [pscustomobject]@{
            Code = $process.ExitCode
            Text = [System.Text.Encoding]::UTF8.GetString($outputBytes.ToArray())
            ErrorText = $errorText
        }
    } finally {
        if ($sourceStream) { $sourceStream.Dispose() }
        if ($started -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
        $outputBytes.Dispose()
    }
}

function Invoke-Quest([string[]]$AdbArguments, [string]$InputFile = '') {
    return Invoke-Adb (@('-s', $script:QuestSerial) + $AdbArguments) $InputFile
}

function Invoke-App([string]$Command) {
    # exec-out can hide a remote error code. Require an end marker from a
    # set -e script; do not trust the adb process exit code alone.
    $scriptText = "set -eu`n" + $Command + "`nprintf '__ACQUEST_INSTALL_OK__\n'"
    $result = Invoke-Quest @('exec-out', 'run-as', $QuestPackage, 'sh', '-c', $scriptText)
    if ($result.Code -ne 0 -or !$result.Text.EndsWith($AppMarker)) {
        throw ('Quest app-storage command failed. ' + $result.Text + $result.ErrorText).Trim()
    }
    return $result.Text.Substring(0, $result.Text.Length - $AppMarker.Length)
}

function Get-AdbPath {
    if ($script:AdbPath) {
        if (!(Test-Path -LiteralPath $script:AdbPath -PathType Leaf)) { throw 'The specified ADB executable does not exist.' }
        return (Resolve-Path -LiteralPath $script:AdbPath).Path
    }
    $candidates = @(
        (Join-Path $PSScriptRoot 'adb.exe'),
        (Join-Path $PSScriptRoot 'platform-tools/adb.exe')
    )
    $found = Get-Command adb.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($found) { $candidates += $found.Source }
    if ($env:LOCALAPPDATA) {
        $candidates += @(
            (Join-Path $env:LOCALAPPDATA 'Android/Sdk/platform-tools/adb.exe'),
            (Join-Path $env:LOCALAPPDATA 'SideQuest/platform-tools/adb.exe'),
            (Join-Path $env:LOCALAPPDATA 'SideQuest/adb/adb.exe'),
            (Join-Path $env:LOCALAPPDATA 'Programs/SideQuest/resources/app.asar.unpacked/build/platform-tools/adb.exe')
        )
    }
    if ($env:APPDATA) {
        $candidates += (Join-Path $env:APPDATA 'SideQuest/platform-tools/adb.exe')
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return (Resolve-Path -LiteralPath $candidate).Path }
    }
    throw 'ADB was not found. Extract Android SDK Platform-Tools beside this installer (platform-tools\adb.exe), put adb.exe on PATH, or use -AdbPath. See README.md.'
}

function Get-QuestSerial {
    $result = Invoke-Adb @('devices', '-l')
    if ($result.Code -ne 0) { throw ('ADB could not list devices. ' + $result.ErrorText) }
    $quests = @()
    foreach ($line in ($result.Text -split '\r?\n')) {
        if ($line -notmatch '^(\S+)\s+(device|unauthorized|offline)(?:\s|$)') { continue }
        $serial = $Matches[1]
        $state = $Matches[2]
        # An unauthorized/offline device cannot be identified safely.
        if ($state -ne 'device') {
            throw "Device $serial is $state. Wake the Quest and authorize USB debugging in the headset; disconnect other unauthorized/offline devices, then retry."
        }
        $model = Invoke-Adb @('-s', $serial, 'shell', 'getprop', 'ro.product.model')
        if ($model.Code -ne 0) { throw "Could not identify connected device $serial." }
        if ($model.Text.Trim() -eq 'Quest 3') { $quests += $serial }
    }
    if ($quests.Count -eq 0) { throw 'No authorized Quest 3 found. Connect it by USB, enable developer mode, and allow USB debugging in the headset.' }
    if ($quests.Count -ne 1) { throw 'More than one Quest 3 is connected. Disconnect the others (including duplicate wireless connections), then retry.' }
    return $quests[0]
}

function Assert-GameStopped {
    $result = Invoke-Quest @('shell', 'pidof', $QuestPackage)
    if ($result.Text.Trim()) {
        throw 'Animal Crossing is running on the Quest. Save and quit, then close the app before retrying. The installer will not force-stop your game.'
    }
    if ($result.Code -gt 1 -or $result.ErrorText.Trim()) { throw ('Could not check whether Animal Crossing is stopped. ' + $result.ErrorText) }
}

function Get-RemoteInfo([string]$Path) {
    # All paths passed here are generated locally, not arbitrary filenames.
    $text = Invoke-App ("sha256sum $Path`nwc -c < $Path")
    $lines = @($text.Trim() -split '\r?\n')
    if ($lines.Count -ne 2 -or $lines[0] -notmatch '^([0-9a-fA-F]{64})\s' -or $lines[1] -notmatch '^\s*\d+\s*$') {
        throw 'Quest returned an invalid file verification response.'
    }
    return [pscustomobject]@{ Hash = $lines[0].Substring(0, 64).ToLowerInvariant(); Size = [long]$lines[1].Trim() }
}

function Test-RemotePath([string]$Path) {
    $answer = Invoke-App "if [ -e $Path ] || [ -L $Path ]; then echo yes; else echo no; fi"
    if ($answer -eq "yes`n") { return $true }
    if ($answer -eq "no`n") { return $false }
    throw 'Quest returned an invalid file-existence response.'
}

function Get-LocalInfo([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $hasher.ComputeHash($stream)
        return [pscustomobject]@{
            Hash = [BitConverter]::ToString($digest).Replace('-', '').ToLowerInvariant()
            Size = $stream.Length
        }
    } finally { $stream.Dispose(); $hasher.Dispose() }
}

function Assert-CompatibleRom([string]$Path) {
    $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
    if ($extension -notin @('.iso', '.gcm', '.ciso')) { throw 'Choose your own USA Rev 0 Animal Crossing .iso, .gcm, or .ciso dump; RVZ/ZIP files are not supported.' }
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $header = New-Object byte[] 32
        if ($stream.Read($header, 0, 32) -ne 32) { throw 'The disc image is incomplete.' }
        if ([System.Text.Encoding]::ASCII.GetString($header, 0, 4) -eq 'CISO') {
            $blockSize = [BitConverter]::ToUInt32($header, 4)
            if ($blockSize -lt 32 -or $header[8] -eq 0) { throw 'Invalid CISO disc header.' }
            $null = $stream.Seek(32768, [System.IO.SeekOrigin]::Begin)
            if ($stream.Read($header, 0, 32) -ne 32) { throw 'The CISO disc image is incomplete.' }
        }
        if ([System.Text.Encoding]::ASCII.GetString($header, 0, 6) -ne 'GAFE01' -or $header[6] -ne 0 -or $header[7] -ne 0 -or
            $header[28] -ne 0xC2 -or $header[29] -ne 0x33 -or $header[30] -ne 0x9F -or $header[31] -ne 0x3D) {
            throw 'This build requires Animal Crossing USA, revision 0 (GAFE01_00). The selected file has a different or invalid disc header.'
        }
    } finally { $stream.Dispose() }
    return $extension
}

try {
    $apk = Join-Path $PSScriptRoot 'AnimalCrossing-Quest.apk'
    $checksums = Join-Path $PSScriptRoot 'SHA256SUMS.txt'
    if (!(Test-Path -LiteralPath $apk -PathType Leaf) -or !(Test-Path -LiteralPath $checksums -PathType Leaf)) {
        throw 'Extract the complete release ZIP first. AnimalCrossing-Quest.apk and SHA256SUMS.txt must be beside this installer.'
    }
    $expected = @()
    foreach ($line in (Get-Content -LiteralPath $checksums)) {
        if ($line -match '^([0-9a-fA-F]{64})\s+\*?AnimalCrossing-Quest\.apk\s*$') { $expected += $Matches[1].ToLowerInvariant() }
    }
    if ($expected.Count -ne 1 -or (Get-LocalInfo $apk).Hash -ne $expected[0]) {
        throw 'The APK does not match SHA256SUMS.txt. Download and extract a fresh complete release ZIP.'
    }
    $script:AdbPath = Get-AdbPath
    $script:QuestSerial = Get-QuestSerial
    Assert-GameStopped
    Write-Host 'Installing Animal Crossing on Quest 3 (existing app data is preserved)...'
    $install = Invoke-Quest @('install', '-r', $apk)
    if ($install.Code -ne 0 -or $install.Text -notmatch '(?m)^Success\s*$') {
        throw ('APK installation failed. Do not uninstall to work around an update/signing error; export saves first. ' + $install.Text + $install.ErrorText).Trim()
    }
    Assert-GameStopped
    $location = (Invoke-App 'pwd').Trim()
    if (!$location.EndsWith('/' + $QuestPackage)) { throw 'Unexpected private app-storage directory; no game data was changed.' }
    $listing = Invoke-App 'if [ -d files/rom ]; then find files/rom -maxdepth 1 -type f -print; fi'
    $existing = @($listing -split '\r?\n' | Where-Object { $_ -match '\.(iso|gcm|ciso)$' })
    if ($existing.Count -gt 0) {
        Write-Host 'Your existing Quest ROM, saves, and settings have been kept. No game data was imported.'
    } else {
        if (!$RomPath -and !$NonInteractive) {
            Add-Type -AssemblyName System.Windows.Forms
            $dialog = New-Object System.Windows.Forms.OpenFileDialog
            try {
                $dialog.Title = 'Select your own Animal Crossing USA Rev 0 (GAFE01_00) disc image'
                $dialog.Filter = 'GameCube disc images (*.iso;*.gcm;*.ciso)|*.iso;*.gcm;*.ciso'
                $dialog.CheckFileExists = $true
                $dialog.Multiselect = $false
                if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $RomPath = $dialog.FileName }
            } finally { $dialog.Dispose() }
        }
        if (!$RomPath) {
            if ($NonInteractive) { throw 'The app is installed but has no ROM. Supply -RomPath or run the installer interactively to choose your own disc image.' }
            Write-Host 'The app is installed, but needs your ROM before it can run. Run this installer again when your dump is ready.'
            exit 0
        }
        if (!(Test-Path -LiteralPath $RomPath -PathType Leaf)) { throw 'The selected ROM file does not exist.' }
        $RomPath = (Resolve-Path -LiteralPath $RomPath).Path
        $extension = Assert-CompatibleRom $RomPath
        $local = Get-LocalInfo $RomPath
        $localSize = $local.Size
        $localHash = $local.Hash
        $destination = 'files/rom/AnimalCrossing' + $extension
        $temporary = 'files/rom/.acquest-import-' + [Guid]::NewGuid().ToString('N') + '.tmp'
        Assert-GameStopped
        $null = Invoke-App 'mkdir -p files/rom'
        if (Test-RemotePath $destination) { throw 'A Quest ROM appeared during setup; it has been kept. Close the game and run the installer again.' }
        Write-Host 'Copying and verifying your disc image. This can take several minutes...'
        try {
            $upload = Invoke-Quest @('exec-in', 'run-as', $QuestPackage, 'sh', '-c', "umask 077; cat > $temporary") $RomPath
            if ($upload.Code -ne 0) { throw ('ROM transfer failed. ' + $upload.Text + $upload.ErrorText) }
            $remote = Get-RemoteInfo $temporary
            if ($remote.Hash -ne $localHash -or $remote.Size -ne $localSize) { throw 'ROM transfer verification failed; the incomplete copy will be removed.' }
            Assert-GameStopped
            $null = Invoke-App "mv -n $temporary $destination"
            if (Test-RemotePath $temporary) { throw 'A Quest ROM appeared during the transfer; the existing file was kept.' }
            $remote = Get-RemoteInfo $destination
            if ($remote.Hash -ne $localHash -or $remote.Size -ne $localSize) { throw 'Final ROM verification failed.' }
        } finally { $null = Invoke-App "rm -f $temporary" }
        Write-Host 'ROM import verified. Your PC files and existing Quest saves/settings were not changed.'
    }
    Write-Host 'Ready. In your Quest library, open Unknown Sources, then Animal Crossing.'
    Write-Host 'For future updates, run this installer again. Do not uninstall or clear app data.'
    exit 0
} catch {
    Write-Host ('Setup stopped: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
