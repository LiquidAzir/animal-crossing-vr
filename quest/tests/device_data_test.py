"""Safety regressions for verified transfers; no device or live save access."""
from pathlib import Path
import hashlib
import importlib.util
import shlex
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch
import sys

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
spec = importlib.util.spec_from_file_location('device_data', TOOLS / 'device_data.py')
data = importlib.util.module_from_spec(spec)
spec.loader.exec_module(data)


class FakeDevice(data.Device):
    def __init__(self):
        self.files = {}
        self.uploads = 0
        self.corrupt = False
        self.race = False

    def exists(self, path):
        return path in self.files or path == 'files/save'

    def info(self, path):
        content = self.files[path]
        return len(content), hashlib.sha256(content).hexdigest()

    def upload(self, source, destination):
        self.uploads += 1
        self.files[destination] = source.read_bytes() + (b'broken' if self.corrupt else b'')

    def execute(self, mode, script, **kwargs):
        assert mode == 'exec-out'
        args = shlex.split(script)
        assert args[0] == 'cat'
        return subprocess.CompletedProcess(args, 0, self.files[args[1]], b'')

    def shell(self, script):
        args = shlex.split(script)
        if args[:2] == ['mkdir', '-p']:
            return b''
        if args[:2] == ['rm', '-f']:
            self.files.pop(args[2], None)
            return b''
        if args[:2] == ['mv', '-n']:
            if self.race:
                self.files[args[3]] = b'newer save'
            if args[3] not in self.files:
                self.files[args[3]] = self.files.pop(args[2])
            return b''
        if args[0] == 'find':
            return ''.join(p + '\n' for p in self.files if p.endswith('.gci')).encode()
        raise AssertionError(script)


class Transfers(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.source = self.folder / 'sample.gci'
        self.payload = bytes(range(256)) * 100
        self.source.write_bytes(self.payload)
        self.remote = 'files/save/card_a/sample.gci'
        self.device = FakeDevice()

    def test_binary_import_and_identical_retry(self):
        self.assertEqual(self.device.import_file(self.source, self.remote)['action'], 'imported')
        self.assertEqual(self.device.files, {self.remote: self.payload})
        self.assertEqual(self.device.import_file(self.source, self.remote)['action'], 'already present')
        self.assertEqual(self.device.uploads, 1)

    def test_existing_save_is_never_overwritten(self):
        self.device.files[self.remote] = b'Quest progress'
        with self.assertRaises(FileExistsError):
            self.device.import_file(self.source, self.remote)
        self.assertEqual(self.device.files, {self.remote: b'Quest progress'})
        self.assertEqual(self.device.uploads, 0)

    def test_corruption_is_rejected_and_temporary_removed(self):
        self.device.corrupt = True
        with self.assertRaises(RuntimeError):
            self.device.import_file(self.source, self.remote)
        self.assertEqual(self.device.files, {})

    def test_racing_save_is_preserved(self):
        self.device.race = True
        with self.assertRaises(FileExistsError):
            self.device.import_file(self.source, self.remote)
        self.assertEqual(self.device.files, {self.remote: b'newer save'})

    def test_binary_export_and_overwrite_protection(self):
        self.device.files[self.remote] = self.payload
        self.assertEqual(self.device.export_saves(self.folder)[0]['action'], 'exported')
        target = self.folder / 'card_a/sample.gci'
        self.assertEqual(target.read_bytes(), self.payload)
        self.assertEqual(self.device.export_saves(self.folder)[0]['action'], 'already exported')
        target.write_bytes(b'local backup')
        with self.assertRaises(FileExistsError):
            self.device.export_saves(self.folder)
        self.assertEqual(target.read_bytes(), b'local backup')


class PortableReleaseTests(unittest.TestCase):
    def test_flat_shallow_path_with_explicit_adb(self):
        # Exercise the flattened release location without creating a file at
        # the drive root or touching a real device.
        shallow = Path(Path.cwd().anchor) / 'AnimalCrossing-Quest/device_data.py'
        namespace = {'__file__': str(shallow), '__name__': 'flat_data_test'}
        exec(compile((TOOLS / 'device_data.py').read_text(), str(shallow), 'exec'), namespace)
        self.assertEqual(namespace['WORKSPACE'], shallow.parent)
        device = Mock()
        device.export_saves.return_value = []
        factory = Mock(return_value=device)
        namespace['Device'] = factory
        with patch.object(sys, 'argv', ['device_data.py', '--serial', 'TEST_QUEST',
                                      '--adb', 'test-adb', '--export-save', 'test-backup']), \
                patch('builtins.print'):
            namespace['main']()
        factory.assert_called_once_with(Path('test-adb'), 'TEST_QUEST')
        device.ensure_stopped.assert_called_once_with()
        device.export_saves.assert_called_once_with(Path('test-backup'))


if __name__ == '__main__':
    unittest.main()
