"""Check actual fountain actor draw ordering against original display lists."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
out = ROOT / 'pc/build32/fountain-draw-tests'
out.mkdir(parents=True, exist_ok=True)
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'shrine-draw.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', 'pc/tests/vr_shrine_draw.c',
    'pc/src/pc_gbi_runtime.c', 'src/data/model/obj_s_shrine.c', '-o', str(exe)],
    cwd=ROOT, env=env, check=True)
result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
(out / 'results.txt').write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr, end='')
raise SystemExit(result.returncode)
