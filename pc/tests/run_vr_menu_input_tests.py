"""Exercise actual desktop/Quest PAD merge functions with sequenced VR input.

No runtime, ROM, or headset is used. The menu host is a consuming callback seam;
its navigation/apply behavior has its own tests.
"""
from pathlib import Path
import os
import subprocess
from run_vr_tool_tests import ROOT, function

out = ROOT / 'pc/build32/vr-menu-input-tests'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = r'C:\msys64\mingw32\bin;' + env.get('PATH', '')
paths = [('desktop', ROOT / 'pc/src/pc_vr.cpp')]
if (ROOT / 'quest/src/quest_vr.cpp').exists():
    paths.append(('quest', ROOT / 'quest/src/quest_vr.cpp'))
for name, path in paths:
    source = path.read_text(encoding='utf-8')
    body = '\n'.join(function(source, f) for f in ['pcvr_menu_input_available', 'pc_vr_merge_pad'])
    (out / 'menu_input_actual.inc').write_text(body, encoding='utf-8')
    exe = out / (name + '.exe')
    cmd = [r'C:\msys64\mingw32\bin\gcc.exe', '-std=gnu11', '-O2', '-Wall', '-Wextra',
           '-I' + str(ROOT / 'pc/include'), '-I' + str(out),
           str(ROOT / 'pc/tests/vr_menu_input.c'), '-lm', '-o', str(exe)]
    result = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True)
    (out / f'{name}-compile.txt').write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout + result.stderr)
        result.check_returncode()
    result = subprocess.run([exe], cwd=ROOT, env=env, capture_output=True, text=True)
    (out / f'{name}-results.txt').write_text(result.stdout + result.stderr)
    print(name + ': ' + result.stdout, end='')
    result.check_returncode()
