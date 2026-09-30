"""Exercise production first-person/VR acre draw routing without a headset."""
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import ROOT, function

out = ROOT / 'pc/build32/crossing-draw-tests'
out.mkdir(parents=True, exist_ok=True)
source = function((ROOT / 'src/actor/ac_field_draw.c').read_text(), 'aFD_DrawBg')
if source is None:
    raise RuntimeError('Missing production acre draw function')
(out / 'crossing_draw_source.inc').write_text(source)
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'crossing-draw.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-DTARGET_PC', '-D_LANGUAGE_C',
                '-DF3DEX_GBI_2', '-DVERSION=0', '-Iinclude', '-Ipc/include', '-I' + str(out),
                'pc/tests/vr_crossing_draw.c', 'pc/src/pc_gbi_runtime.c', '-o', str(exe)],
               cwd=ROOT, env=env, check=True)
run = subprocess.run([str(exe)], cwd=ROOT, env=env, text=True, capture_output=True)
(out / 'results.txt').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)
