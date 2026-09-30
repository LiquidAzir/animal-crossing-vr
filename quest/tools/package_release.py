"""Package the verified Quest APK and installation tools without rebuilding it.

Run --check before committing to validate the payload without writing outputs.
Publishing requires a clean checkout. Personal game data, build logs, signing
keys, SDK tools, and arbitrary directory contents are never included.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
import zipfile

SOURCE = Path(__file__).resolve().parents[2]
WORKSPACE = SOURCE.parent
PACKAGE = 'com.liquidazir.animalcrossingquest'
ABI = 'armeabi-v7a'
APP_LABEL = 'Animal Crossing'
LIBRARIES = ('libmain.so', 'libSDL2.so', 'libopenxr_loader.so')
LICENSES = (
    'LICENSE-SDL2.txt', 'LICENSE-OpenXR.txt', 'NOTICE-OpenXR.txt',
    'LICENSE-JsonCpp.txt', 'LICENSE-jnipp.txt',
    'LICENSE-Android-JNI-Wrappers.txt', 'NOTICE-LLVM.txt',
    'NOTICE-Android-cpufeatures.txt',
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def validate_apk(apk, receipt_path, expected_version):
    receipt = json.loads(receipt_path.read_text(encoding='utf-8-sig'))
    expected = {'package': PACKAGE, 'version_code': expected_version,
                'abi': ABI, 'label': APP_LABEL}
    if receipt.get('verified_manifest') != expected:
        raise ValueError('Build receipt does not verify the requested game package/version/ABI/label')
    if (receipt.get('package') != PACKAGE or receipt.get('abi') != ABI or
            receipt.get('version') != expected_version):
        raise ValueError('Build receipt identity is inconsistent')
    apk_hash = sha256(apk)
    if receipt.get('sha256') != apk_hash:
        raise ValueError('APK hash does not match its verified build receipt')
    if set(receipt.get('libraries', {})) != set(LIBRARIES):
        raise ValueError('Build receipt must identify exactly the three game libraries')
    required = {'AndroidManifest.xml', 'resources.arsc', 'classes.dex',
                'assets/quest-settings.ini', 'assets/shaders/default.vert',
                'assets/shaders/default.frag'}
    required.update('lib/' + ABI + '/' + name for name in LIBRARIES)
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError('APK contains duplicate ZIP entries')
        if not required.issubset(names):
            raise ValueError('APK is missing required game files')
        for name in names:
            if name not in required and name != 'META-INF/MANIFEST.MF' and not re.fullmatch(
                    r'META-INF/[A-Za-z0-9_-]+\.(?:SF|RSA|DSA|EC)', name):
                raise ValueError('Unexpected APK payload: ' + name)
        if archive.testzip() is not None:
            raise ValueError('APK ZIP integrity check failed')
        for name in LIBRARIES:
            data = archive.read('lib/' + ABI + '/' + name)
            if (len(data) < 20 or data[:4] != b'\x7fELF' or data[4:6] != b'\x01\x01' or
                    int.from_bytes(data[18:20], 'little') != 40):
                raise ValueError(name + ' is not an ARM32 little-endian library')
            if hashlib.sha256(data).hexdigest() != receipt['libraries'][name]:
                raise ValueError(name + ' differs from the verified build receipt')
    return receipt, apk_hash


def git_state():
    def git(*args):
        return subprocess.check_output(['git', '-C', str(SOURCE), *args], text=True).strip()
    return git('rev-parse', '--verify', 'HEAD'), bool(git('status', '--porcelain'))


def payload_files(apk):
    files = {
        'AnimalCrossing-Quest.apk': apk,
        'Install-Quest.cmd': SOURCE / 'quest/install/Install-Quest.cmd',
        'Install-Quest.ps1': SOURCE / 'quest/install/Install-Quest.ps1',
        'README.md': SOURCE / 'quest/INSTALL.md',
        'MANUAL.md': SOURCE / 'quest/MANUAL.md',
        'device_data.py': SOURCE / 'quest/tools/device_data.py',
        'LICENSE': SOURCE / 'LICENSE',
    }
    files.update(('licenses/' + name, SOURCE / 'quest/licenses' / name) for name in LICENSES)
    for name, path in files.items():
        if not path.is_file() or path.is_symlink() or not path.stat().st_size:
            raise ValueError('Required release file is missing, empty, or a link: ' + name)
    return files


def package(version, apk, receipt_path, expected_version, check_only=False):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,63}', version):
        raise ValueError('Version must be a short filename-safe label')
    receipt, apk_hash = validate_apk(apk, receipt_path, expected_version)
    files = payload_files(apk)
    commit, dirty = git_state()
    result = {'version': version, 'source_commit': commit, 'working_tree_dirty': dirty,
              'apk_version_code': expected_version, 'apk_sha256': apk_hash,
              'payload': sorted(files), 'check_only': check_only}
    if check_only:
        return result
    if dirty:
        raise ValueError('Commit the reviewed release files before packaging; the checkout is dirty')

    output = WORKSPACE / 'build/releases'
    basename = 'AnimalCrossing-Quest-' + version
    zip_path = output / (basename + '.zip')
    apk_path = output / (basename + '.apk')
    zip_sum = output / (basename + '.zip.sha256')
    apk_sum = output / (basename + '.apk.sha256')
    for path in (zip_path, apk_path, zip_sum, apk_sum):
        if path.exists():
            raise FileExistsError('Release output already exists: ' + str(path))
    output.mkdir(parents=True, exist_ok=True)
    stage_parent = Path(tempfile.mkdtemp(prefix='stage-', dir=output))
    stage = stage_parent / 'AnimalCrossing-Quest'
    stage.mkdir()
    for name, source in files.items():
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    if sha256(stage / 'AnimalCrossing-Quest.apk') != apk_hash:
        raise ValueError('APK changed during staging')
    if git_state() != (commit, False):
        raise ValueError('Source checkout changed during staging; retry from a stable clean checkout')
    info = [
        'Version: ' + version,
        'Git commit (packaging checkout): ' + commit,
        'Working tree dirty: False',
        'Application label: ' + APP_LABEL,
        'Android package: ' + PACKAGE,
        'Android ABI: ' + ABI,
        'APK version code: ' + str(expected_version),
        'APK SHA256: ' + apk_hash,
        'APK bytes are unchanged from the verified build; no rebuild or re-signing.',
    ]
    info.extend(name + ' SHA256: ' + receipt['libraries'][name] for name in LIBRARIES)
    (stage / 'BUILD_INFO.txt').write_text('\n'.join(info) + '\n', encoding='utf-8')
    members = sorted([*files, 'BUILD_INFO.txt'])
    sums = [sha256(stage / name) + '  ' + name for name in members]
    (stage / 'SHA256SUMS.txt').write_text('\n'.join(sums) + '\n', encoding='utf-8')
    members.append('SHA256SUMS.txt')
    with zipfile.ZipFile(zip_path, 'x', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(members):
            archive.write(stage / name, 'AnimalCrossing-Quest/' + name)
    with (stage / 'AnimalCrossing-Quest.apk').open('rb') as source, apk_path.open('xb') as target:
        shutil.copyfileobj(source, target, 1024 * 1024)
    with zip_sum.open('x', encoding='utf-8') as target:
        target.write(sha256(zip_path) + '  ' + zip_path.name + '\n')
    with apk_sum.open('x', encoding='utf-8') as target:
        target.write(sha256(apk_path) + '  ' + apk_path.name + '\n')
    result.update(zip=str(zip_path), apk=str(apk_path), staging=str(stage),
                  zip_sha256=sha256(zip_path), checksums=[str(zip_sum), str(apk_sum)])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True, help='Release label, e.g. v0.9.3-vr20')
    parser.add_argument('--apk', type=Path,
                        default=WORKSPACE / 'build/game-arm32/apk/AnimalCrossingQuest-armeabi-v7a.apk')
    parser.add_argument('--receipt', type=Path, help='Verified package-receipt.json (defaults beside APK)')
    parser.add_argument('--expected-version-code', type=int, default=14)
    parser.add_argument('--check', action='store_true', help='Validate only; write no release or staging files')
    args = parser.parse_args()
    if args.expected_version_code < 1:
        parser.error('--expected-version-code must be positive')
    try:
        result = package(args.version, args.apk.resolve(),
                         (args.receipt or args.apk.parent / 'package-receipt.json').resolve(),
                         args.expected_version_code, args.check)
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, 'Cannot package Quest release: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
