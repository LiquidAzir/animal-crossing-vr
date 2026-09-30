"""Import/export debug Quest app data through run-as, with byte/hash verification.

Uses adb exec-in for binary input: ordinary adb shell stdin can truncate at Ctrl-Z
on Windows. Existing destination files are never overwritten. Stop the game first.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import subprocess
import uuid

PACKAGE = 'com.liquidazir.animalcrossingquest'
TOOL_DIR = Path(__file__).resolve().parent
# A release places this helper next to the APK, rather than quest/tools/.
WORKSPACE = (TOOL_DIR.parents[2] if TOOL_DIR.name == 'tools' and
             TOOL_DIR.parent.name == 'quest' else TOOL_DIR)
MARKER = b'__ACQUEST_DATA_OK__\n'


def local_info(path):
    digest = hashlib.sha256()
    size = 0
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
            size += len(block)
    return size, digest.hexdigest()


def safe_basename(name):
    if not name or name in ('.', '..') or any(c in name for c in '/\\\r\n\x00'):
        raise ValueError(f'Invalid file name: {name!r}')
    return name


class Device:
    def __init__(self, adb, serial):
        self.prefix = [str(adb), '-s', serial]

    def execute(self, mode, script, **kwargs):
        # Pass one script argument; adb exec quotes its arguments for the remote
        # command. Literal extra surrounding quotes would become part of sh -c.
        return subprocess.run(self.prefix + [mode, 'run-as', PACKAGE, 'sh', '-c', script],
                              stderr=subprocess.PIPE, timeout=60, **kwargs)

    def shell(self, script):
        command = 'set -eu\n' + script + "\nprintf '__ACQUEST_DATA_OK__\\n'"
        result = self.execute('exec-out', command, stdout=subprocess.PIPE)
        # exec-out does not reliably return the remote process exit code.
        if result.returncode or not result.stdout.endswith(MARKER):
            raise RuntimeError('App data command failed: ' +
                               (result.stdout + result.stderr).decode(errors='replace').strip())
        return result.stdout[:-len(MARKER)]

    def upload(self, source, destination):
        command = self.prefix + ['exec-in', 'run-as', PACKAGE, 'sh', '-c',
                                 f'umask 077; cat > {shlex.quote(destination)}']
        # An actual pipe is essential on Windows; inheriting a disk-file handle
        # as adb's stdin can still stop at Ctrl-Z, even with exec-in.
        with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE) as process:
            try:
                with source.open('rb') as stream:
                    shutil.copyfileobj(stream, process.stdin, 1024 * 1024)
                process.stdin.close()
                process.stdin = None
                output, error = process.communicate(timeout=60)
            except BaseException:
                process.kill()
                process.wait()
                raise
            if process.returncode:
                raise RuntimeError('Upload failed: ' + (output + error).decode(errors='replace'))

    def ensure_stopped(self):
        result = subprocess.run(self.prefix + ['shell', 'pidof', PACKAGE],
                                capture_output=True, timeout=15)
        if result.stdout.strip():
            raise RuntimeError('Stop Animal Crossing on Quest before importing or exporting data.')
        location = self.shell('pwd').decode().strip()
        if not location.endswith('/' + PACKAGE):
            raise RuntimeError(f'Unexpected run-as working directory: {location!r}')

    def exists(self, path):
        quoted = shlex.quote(path)
        result = self.shell(f'if [ -e {quoted} ] || [ -L {quoted} ]; then echo yes; else echo no; fi')
        if result not in (b'yes\n', b'no\n'):
            raise RuntimeError(f'Unexpected existence result for {path}: {result!r}')
        return result == b'yes\n'

    def info(self, path):
        quoted = shlex.quote(path)
        result = self.shell(f'sha256sum {quoted}\nwc -c < {quoted}').decode().splitlines()
        if len(result) != 2 or not re.fullmatch(r'[0-9a-fA-F]{64}', result[0].split()[0]):
            raise RuntimeError(f'Unexpected hash result for {path}: {result!r}')
        return int(result[1]), result[0].split()[0].lower()

    def import_file(self, source, destination):
        expected = local_info(source)
        if self.exists(destination):
            if self.info(destination) == expected:
                return dict(action='already present', source=str(source), destination=destination,
                            bytes=expected[0], sha256=expected[1])
            raise FileExistsError(f'Refusing to overwrite existing Quest file: {destination}')
        parent = str(PurePosixPath(destination).parent)
        temporary = parent + '/.acquest-import-' + uuid.uuid4().hex + '.tmp'
        self.shell(f'mkdir -p {shlex.quote(parent)}')
        try:
            self.upload(source, temporary)
            if self.info(temporary) != expected:
                raise RuntimeError(f'Upload byte count or SHA-256 mismatch: {source}')
            # -n prevents overwrite even if a destination appears after our check.
            self.shell(f'mv -n {shlex.quote(temporary)} {shlex.quote(destination)}')
            if self.exists(temporary):
                raise FileExistsError(f'Destination appeared during upload; kept existing {destination}')
            if self.info(destination) != expected:
                raise RuntimeError(f'Final file byte count or SHA-256 mismatch: {destination}')
        finally:
            self.shell(f'rm -f {shlex.quote(temporary)}')
        return dict(action='imported', source=str(source), destination=destination,
                    bytes=expected[0], sha256=expected[1])

    def export_saves(self, destination):
        if not self.exists('files/save'):
            return []
        listing = self.shell("find files/save -type f -name '*.gci' -print").decode().splitlines()
        results = []
        for remote in sorted(listing):
            parts = PurePosixPath(remote).parts
            if len(parts) != 4 or parts[:2] != ('files', 'save') or parts[2] not in ('card_a', 'card_b'):
                raise RuntimeError(f'Unexpected save path: {remote!r}')
            safe_basename(parts[3])
            target = destination / parts[2] / parts[3]
            expected = self.info(remote)
            if target.exists():
                if local_info(target) == expected:
                    results.append(dict(action='already exported', destination=str(target),
                                        bytes=expected[0], sha256=expected[1]))
                    continue
                raise FileExistsError(f'Refusing to overwrite local export: {target}')
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.parent / ('.acquest-export-' + uuid.uuid4().hex + '.tmp')
            try:
                with temporary.open('xb') as stream:
                    result = self.execute('exec-out', 'cat ' + shlex.quote(remote), stdout=subprocess.PIPE)
                    stream.write(result.stdout)
                if result.returncode or local_info(temporary) != expected or self.info(remote) != expected:
                    raise RuntimeError(f'Export failed or source changed: {remote}')
                # Hard linking publishes the verified file atomically without replacing any existing file.
                os.link(temporary, target)
            finally:
                temporary.unlink(missing_ok=True)
            results.append(dict(action='exported', source=remote, destination=str(target),
                                bytes=expected[0], sha256=expected[1]))
        return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='ADB serial of the intended Quest')
    parser.add_argument('--adb', type=Path, help='Override local toolchain paths.json adb executable')
    parser.add_argument('--rom', type=Path, help='Owned .ciso, .iso, or .gcm; imports as AnimalCrossing.ext')
    parser.add_argument('--save', type=Path, action='append', default=[], help='Staged .gci to import into card_a')
    parser.add_argument('--export-save', type=Path, help='Export card_a/card_b saves into this directory')
    args = parser.parse_args()
    if not args.rom and not args.save and not args.export_save:
        parser.error('Specify --rom, --save, or --export-save')
    imports = []
    if args.rom:
        if args.rom.suffix.lower() not in ('.ciso', '.iso', '.gcm'):
            parser.error('--rom must be a .ciso, .iso, or .gcm file')
        imports.append((args.rom, 'files/rom/AnimalCrossing' + args.rom.suffix.lower()))
    for save in args.save:
        if save.suffix.lower() != '.gci':
            parser.error('--save must be a .gci file')
        imports.append((save, 'files/save/card_a/' + safe_basename(save.name)))
    for source, _ in imports:
        if not source.is_file() or source.stat().st_size == 0:
            parser.error(f'Input file is missing or empty: {source}')
    if not args.adb:
        paths_file = WORKSPACE / 'toolchain/paths.json'
        if paths_file.is_file():
            paths = json.loads(paths_file.read_text(encoding='utf-8-sig'))
            args.adb = Path(paths['adb'])
        else:
            found = shutil.which('adb')
            if not found:
                parser.error('ADB was not found. Install Android Platform Tools and pass --adb PATH_TO_ADB.')
            args.adb = Path(found)
    device = Device(args.adb, args.serial)
    device.ensure_stopped()
    results = [device.import_file(source, destination) for source, destination in imports]
    if args.export_save:
        results.extend(device.export_saves(args.export_save))
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
