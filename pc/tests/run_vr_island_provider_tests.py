"""Exercise the real seasonal replacement scheduler and null-safe draw dispatch."""
import os
from pathlib import Path
import re
import subprocess
from run_vr_tool_tests import function, ROOT

out = ROOT / 'pc/build32/island-provider-tests'
out.mkdir(parents=True, exist_ok=True)
chunks = []
for path, name in [('src/actor/npc/ac_npc_think.c_inc', 'aNPC_chk_pitfall'),
                   ('src/actor/npc/ac_npc_think_pitfall.c_inc', 'aNPC_think_pitfall_main_proc')]:
    chunks.append(function((ROOT / path).read_text(), name))
(out / 'island_npc_source.inc').write_text('\n\n'.join(chunks))
# Every exterior/event actor uses the checked dispatch: a particular resident
# town may encounter a different building first during the same provider gap.
converted = 0
for path in (ROOT / 'src/actor').rglob('*'):
    if path.suffix not in ('.c', '.c_inc'):
        continue
    text = path.read_text(encoding='utf-8')
    assert '->draw_shadow_proc' not in text, f'Unguarded shadow dispatch: {path}'
    converted += text.count('bIT_draw_shadow_safe(')
assert converted == 36, converted
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'island-provider.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                'pc/tests/vr_island_provider.c', '-o', str(exe)], cwd=ROOT, env=env, check=True)
run = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
report = run.stdout + run.stderr + f'Exterior shadow callers checked: {converted}\n'
(out / 'results.txt').write_text(report)
print(report, end='')
raise SystemExit(run.returncode)
