"""Safety regressions for verified transfers; no device or live save access."""
from pathlib import Path
import hashlib
import importlib.util
import shlex
import subprocess
import tempfile
import unittest

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


if __name__ == '__main__':
    unittest.main()
