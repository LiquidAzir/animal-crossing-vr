"""Exercise release data exclusion and tamper/overwrite guards with synthetic files."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/package_release.py'
spec = importlib.util.spec_from_file_location('package_release', SCRIPT)
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.workspace = Path(self.temp.name)
        self.source = self.workspace / 'source'
        self.apk = self.workspace / 'build/game/apk/game.apk'
        self.apk.parent.mkdir(parents=True)
        self.receipt = self.apk.parent / 'package-receipt.json'
        self.addCleanup(patch.stopall)
        patch.object(release, 'SOURCE', self.source).start()
        patch.object(release, 'WORKSPACE', self.workspace).start()
        self.git = patch.object(release, 'git_state', return_value=('a' * 40, False)).start()
        names = {'AndroidManifest.xml': b'test manifest', 'resources.arsc': b'resources',
                 'classes.dex': b'dex', 'assets/quest-settings.ini': b'defaults',
                 'assets/shaders/default.vert': b'vertex', 'assets/shaders/default.frag': b'fragment'}
        header = bytearray(20)
        header[:6] = b'\x7fELF\x01\x01'
        header[18] = 40
        self.libs = {name: bytes(header) + name.encode() for name in release.LIBRARIES}
        names.update(('lib/armeabi-v7a/' + name, data) for name, data in self.libs.items())
        with zipfile.ZipFile(self.apk, 'w') as archive:
            for name, data in names.items():
                archive.writestr(name, data)
        self.metadata = {
            'package': release.PACKAGE, 'abi': release.ABI, 'version': 14,
            'verified_manifest': {'package': release.PACKAGE, 'abi': release.ABI,
                                  'version_code': 14, 'label': release.APP_LABEL},
            'libraries': {name: hashlib.sha256(data).hexdigest() for name, data in self.libs.items()},
        }
        self.write_receipt()
        paths = ['quest/install/Install-Quest.cmd', 'quest/install/Install-Quest.ps1',
                 'quest/INSTALL.md', 'quest/MANUAL.md', 'quest/tools/device_data.py', 'LICENSE']
        paths += ['quest/licenses/' + name for name in release.LICENSES]
        for name in paths:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('synthetic fixture: ' + name)
        # These files must never appear in either published artifact.
        for name in ('rom/private.iso', 'save/private.gci', 'settings.ini', 'quest/.local/debug.keystore'):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('private fixture')

    def write_receipt(self):
        self.metadata['sha256'] = release.sha256(self.apk)
        self.receipt.write_text(json.dumps(self.metadata))

    def package(self, check=False):
        return release.package('test-only', self.apk, self.receipt, 14, check)

    def test_check_writes_no_release_files(self):
        self.git.return_value = ('a' * 40, True)
        result = self.package(check=True)
        self.assertTrue(result['working_tree_dirty'])
        self.assertFalse((self.workspace / 'build/releases').exists())

    def test_allowlist_checksum_and_unchanged_apk(self):
        result = self.package()
        self.assertEqual(Path(result['apk']).read_bytes(), self.apk.read_bytes())
        with zipfile.ZipFile(result['zip']) as archive:
            expected = {'AnimalCrossing-Quest/' + name for name in result['payload']}
            expected.update({'AnimalCrossing-Quest/BUILD_INFO.txt', 'AnimalCrossing-Quest/SHA256SUMS.txt'})
            self.assertEqual(set(archive.namelist()), expected)
            sums = archive.read('AnimalCrossing-Quest/SHA256SUMS.txt').decode().splitlines()
            apk_entries = [line for line in sums if line.endswith('  AnimalCrossing-Quest.apk')]
            self.assertEqual(apk_entries, [release.sha256(self.apk) + '  AnimalCrossing-Quest.apk'])
            for line in sums:
                digest, name = line.split('  ', 1)
                self.assertEqual(hashlib.sha256(archive.read('AnimalCrossing-Quest/' + name)).hexdigest(), digest)

    def test_refuses_existing_release(self):
        result = self.package()
        before = Path(result['zip']).read_bytes()
        with self.assertRaises(FileExistsError):
            self.package()
        self.assertEqual(Path(result['zip']).read_bytes(), before)

    def test_dirty_checkout_cannot_package(self):
        self.git.return_value = ('a' * 40, True)
        with self.assertRaisesRegex(ValueError, 'dirty'):
            self.package()
        self.assertFalse((self.workspace / 'build/releases').exists())

    def test_rejects_rom_inside_apk_even_with_matching_receipt_hash(self):
        with zipfile.ZipFile(self.apk, 'a') as archive:
            archive.writestr('assets/rom/private.iso', b'not a real disc')
        self.write_receipt()
        with self.assertRaisesRegex(ValueError, 'Unexpected APK payload'):
            self.package(check=True)

    def test_rejects_apk_hash_mismatch(self):
        self.metadata['sha256'] = '0' * 64
        self.receipt.write_text(json.dumps(self.metadata))
        with self.assertRaisesRegex(ValueError, 'APK hash'):
            self.package(check=True)

    def test_rejects_library_hash_mismatch(self):
        self.metadata['libraries']['libmain.so'] = '0' * 64
        self.write_receipt()
        with self.assertRaisesRegex(ValueError, 'libmain.so differs'):
            self.package(check=True)

    def test_rejects_different_package_or_version(self):
        self.metadata['verified_manifest']['version_code'] = 13
        self.write_receipt()
        with self.assertRaisesRegex(ValueError, 'package/version/ABI/label'):
            self.package(check=True)


if __name__ == '__main__':
    unittest.main()
