"""Run the shipped PowerShell installer against a disposable fake adb.exe only.

No headset, installed app, real ADB executable, or personal game data is used.
"""
import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import mock
import zipfile

SOURCE = Path(__file__).resolve().parents[2]
checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


def disc(ciso=False, wrong_region=False, wrong_revision=False):
    # Exercises all byte values repeatedly, including Ctrl-Z, beyond pipe buffers.
    raw = bytearray(bytes(range(256)) * 8192)
    raw[:8] = b'GAFE01\0\0'
    raw[28:32] = bytes.fromhex('c2339f3d')
    if wrong_region:
        raw[3] = ord('P')
    if wrong_revision:
        raw[7] = 1
    if ciso:
        header = bytearray(32768)
        header[:4] = b'CISO'
        header[4:8] = (2097152).to_bytes(4, 'little')
        header[8] = 1
        raw = header + raw
    return bytes(raw)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, help='Also exercise an already extracted public bundle using fake ADB')
    parser.add_argument('--package-roundtrip', action='store_true', help='Also package/extract the verified local APK into a disposable test directory')
    args = parser.parse_args()
    if os.name != 'nt':
        raise SystemExit('These integration tests require Windows PowerShell 5.1.')
    compiler = Path(os.environ['WINDIR']) / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    with tempfile.TemporaryDirectory(prefix='AC Quest installer tests caf\u00e9 ') as temporary:
        root = Path(temporary)
        fake = root / 'fake adb.exe'
        subprocess.run([str(compiler), '/nologo', '/target:exe', '/out:' + str(fake),
                        str(SOURCE / 'quest/tests/installer_fake_adb.cs')], check=True)
        scenarios = [
            ('fresh-iso', 'fresh', '.iso', disc(), 0),
            ('fresh-gcm', 'fresh', '.gcm', disc(), 0),
            ('fresh-ciso', 'fresh', '.ciso', disc(True), 0),
            ('existing', 'fresh', '.iso', disc(), 0),
            ('unauthorized', 'unauthorized', '.iso', disc(), 1),
            ('offline', 'offline', '.iso', disc(), 1),
            ('phone-only', 'phoneonly', '.iso', disc(), 1),
            ('multiple', 'twoquests', '.iso', disc(), 1),
            ('running-game', 'busy', '.iso', disc(), 1),
            ('bad-apk-hash', 'fresh', '.iso', disc(), 1),
            ('wrong-region', 'fresh', '.iso', disc(wrong_region=True), 1),
            ('wrong-revision', 'fresh', '.iso', disc(wrong_revision=True), 1),
            ('bad-transfer', 'hashfail', '.iso', disc(), 1),
            ('remote-error', 'remoteerror', '.iso', disc(), 1),
            ('concurrent-file', 'race', '.iso', disc(), 1),
            ('install-error', 'installfail', '.iso', disc(), 1),
            ('bad-adb', 'fresh', '.iso', disc(), 1),
            ('truncated-disc', 'fresh', '.iso', b'too short', 1),
            ('unsupported-rvz', 'fresh', '.rvz', disc(), 1),
            ('missing-rom', 'fresh', '.iso', disc(), 1),
        ]
        for name, scenario, extension, data, expected_code in scenarios:
            folder = root / name
            folder.mkdir()
            installer = folder / 'Install-Quest.ps1'
            shutil.copyfile(SOURCE / 'quest/install/Install-Quest.ps1', installer)
            apk = folder / 'AnimalCrossing-Quest.apk'
            apk.write_bytes(b'disposable synthetic APK fixture\x1a')
            digest = hashlib.sha256(apk.read_bytes()).hexdigest()
            if name == 'bad-apk-hash':
                digest = '0' * 64
            (folder / 'SHA256SUMS.txt').write_text(digest + '  AnimalCrossing-Quest.apk\n')
            rom = folder / ('My own ROM' + extension)
            rom.write_bytes(data)
            private = folder / 'private/files'
            private.mkdir(parents=True)
            (private / 'settings.ini').write_bytes(b'keep-settings\x1a\0')
            (private / 'save/card_a').mkdir(parents=True)
            (private / 'save/card_a/MyTown.gci').write_bytes(b'keep-save\x1a\0')
            if name == 'existing':
                (private / 'rom').mkdir()
                (private / 'rom/Existing.CISO').write_bytes(b'keep-existing-ROM\x1a\0')
                rom.unlink()  # Existing ROM must skip supplied path validation too.
            env = dict(os.environ, ACQUEST_INSTALL_TEST_ROOT=str(folder),
                       ACQUEST_INSTALL_TEST_SCENARIO=scenario)
            selected_adb = fake
            if name == 'bad-adb':
                selected_adb = folder / 'not-an-executable.exe'
                selected_adb.write_text('not an executable')
            command = [
                'powershell.exe', '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', str(installer), '-AdbPath', str(selected_adb), '-NonInteractive',
            ]
            if name != 'missing-rom':
                command += ['-RomPath', str(rom)]
            result = subprocess.run(command, capture_output=True, text=True, env=env, timeout=90)
            check(result.returncode == expected_code,
                  name + ' exit code: ' + result.stdout + result.stderr)
            calls = (folder / 'calls.txt').read_text() if (folder / 'calls.txt').exists() else ''
            check('force-stop' not in calls and '|start|' not in calls and 'uninstall' not in calls,
                  name + ' must not stop, launch, or uninstall game')
            check((private / 'settings.ini').read_bytes() == b'keep-settings\x1a\0', name + ' settings preserved')
            check((private / 'save/card_a/MyTown.gci').read_bytes() == b'keep-save\x1a\0', name + ' saves preserved')
            check(not list(private.glob('rom/.acquest-import-*')), name + ' temporary upload cleaned up')
            if name.startswith('fresh-'):
                check((private / ('rom/AnimalCrossing' + extension)).read_bytes() == data,
                      name + ' byte-exact binary transfer, including Ctrl-Z')
                check('|PHONE|install|' not in calls and '|PHONE|exec-' not in calls,
                      name + ' connected phone ignored')
                check('Ready.' in result.stdout, name + ' success message')
            elif name == 'existing':
                check((private / 'rom/Existing.CISO').read_bytes() == b'keep-existing-ROM\x1a\0', 'existing ROM unchanged')
                check('exec-in' not in calls, 'existing ROM skips upload')
                check('have been kept' in result.stdout, 'existing data preservation message')
            elif name == 'concurrent-file':
                check((private / 'rom/AnimalCrossing.iso').read_bytes() == b'existing-file-wins', 'atomic no-overwrite rename')
            else:
                check(not list(private.glob('rom/AnimalCrossing.*')), name + ' no finalized ROM')
            if name in ('bad-apk-hash', 'bad-adb', 'unauthorized', 'offline', 'phone-only', 'multiple', 'running-game'):
                check('|install|' not in calls, name + ' refuses before installation')
            if name == 'bad-apk-hash':
                check(not calls, 'checksum validated before any ADB call')
            print(name + ': PASS')
        bundles = []
        if args.bundle:
            bundles.append(args.bundle.resolve())
        if args.package_roundtrip:
            spec = importlib.util.spec_from_file_location('quest_release', SOURCE / 'quest/tools/package_release.py')
            package = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(package)
            apk = SOURCE.parent / 'build/game-arm32/apk/AnimalCrossingQuest-armeabi-v7a.apk'
            # Only the output root and clean-state claim are mocked. APK receipt,
            # payload allowlist, hashes, names, staging and ZIP are production code.
            with mock.patch.object(package, 'WORKSPACE', root / 'package-fixture'), \
                    mock.patch.object(package, 'git_state', return_value=('a' * 40, False)):
                packed = package.package('installer-test-fixture', apk, apk.parent / 'package-receipt.json', 14)
            extracted = root / 'extracted-release'
            with zipfile.ZipFile(packed['zip']) as archive:
                archive.extractall(extracted)
            bundles.append(extracted / 'AnimalCrossing-Quest')
        for index, bundle in enumerate(bundles):
            state = root / f'packaged-case-{index}'
            state.mkdir()
            rom = state / 'My own ROM.iso'
            data = disc()
            rom.write_bytes(data)
            env = dict(os.environ, ACQUEST_INSTALL_TEST_ROOT=str(state), ACQUEST_INSTALL_TEST_SCENARIO='fresh')
            result = subprocess.run([
                'powershell.exe', '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', str(bundle / 'Install-Quest.ps1'), '-AdbPath', str(fake),
                '-RomPath', str(rom), '-NonInteractive',
            ], capture_output=True, text=True, env=env, timeout=90)
            check(result.returncode == 0, 'packaged installer failed: ' + result.stdout + result.stderr)
            check((state / 'private/files/rom/AnimalCrossing.iso').read_bytes() == data,
                  'packaged installer transfers exact fixture')
            check('Ready.' in result.stdout, 'packaged installer reports success')
            print('packaged installer/checksum contract: PASS')
        print(f'Quest public installer: {checks} checks passed across {len(scenarios)} scenarios')


if __name__ == '__main__':
    main()
