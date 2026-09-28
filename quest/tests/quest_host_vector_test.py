"""Compile the production host allocator against both host and actual Quest ARM32 libc++."""
from pathlib import Path
import json
import os
import subprocess

SOURCE = Path(__file__).resolve().parents[2]
QUEST = SOURCE.parent
OUT = QUEST / 'research' / 'host-vector'
OUT.mkdir(parents=True, exist_ok=True)
paths = json.loads((QUEST / 'toolchain/paths.json').read_text(encoding='utf-8-sig'))
env = dict(os.environ)
env['PATH'] = r'C:\msys64\mingw32\bin;' + env.get('PATH', '')
summary = {}
for target, compiler in [('windows', r'C:\msys64\mingw32\bin\g++.exe'),
                         ('arm32', paths['clangxx_armv7_api24'])]:
    binary = OUT / ('test-' + target + ('.exe' if target == 'windows' else ''))
    command = [compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-static-libstdc++',
               '-I', str(SOURCE / 'quest/include'),
               '-I', str(QUEST / 'third_party/OpenXR-SDK/include'),
               str(Path(__file__).with_suffix('.cpp')), '-o', str(binary)]
    if target == 'windows': command.append('-static-libgcc')
    compile_result = subprocess.run(command, capture_output=True, env=env, timeout=60)
    (OUT / f'compile-{target}.log').write_bytes(compile_result.stdout + compile_result.stderr)
    if compile_result.returncode:
        raise RuntimeError(compile_result.stderr.decode(errors='replace'))
    if target == 'arm32':
        remote = '/data/local/tmp/acquest-host-vector'
        subprocess.run([paths['adb'], 'push', str(binary), remote], check=True,
                       capture_output=True, timeout=30)
        subprocess.run([paths['adb'], 'shell', 'chmod', '700', remote], check=True,
                       capture_output=True, timeout=30)
        command = [paths['adb'], 'shell', remote]
    else:
        command = [str(binary)]
    result = subprocess.run(command, capture_output=True, env=env, timeout=30)
    (OUT / f'test-{target}.log').write_bytes(result.stdout + result.stderr)
    summary[target] = {'exit': result.returncode, 'output': result.stdout.decode().strip()}
(OUT / 'allocator-results.json').write_text(json.dumps(summary, indent=2))
print(json.dumps(summary, indent=2))
raise SystemExit(any(r['exit'] for r in summary.values()))
