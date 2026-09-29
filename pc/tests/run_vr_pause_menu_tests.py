"""Exercise the production pause host without starting a game or loading saves."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'pc/build32/vr-pause-menu-tests'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'pc/src/pc_pause_menu.c').read_text()
(out / 'pause_source.inc').write_text(source[source.index('int g_pc_paused ='):source.index('static MenuAction translate_key')])
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'vr-pause-menu.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-Wall', '-Wextra', '-I' + str(out),
                'pc/tests/vr_pause_menu.c', '-o', str(exe)], cwd=root, env=env, check=True)
result = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
(out / 'results.txt').write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr, end='')
raise SystemExit(result.returncode)
