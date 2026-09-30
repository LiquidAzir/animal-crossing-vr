"""Compile the actual dock/residency/allocation functions against field seams."""
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import function, ROOT

out = ROOT / 'pc/build32/vr-polish-tests'
out.mkdir(parents=True, exist_ok=True)
specs = {
    'src/actor/ac_boat_move.c_inc': ['aBT_check_alive', 'aBT_demo_ctrl_birth_wait'],
    'src/actor/ac_birth_control.c': ['aBC_chk_near_boat_block', 'aBC_deleteActor_part'],
    'src/actor/ac_boat_demo_move.c_inc': ['aBTD_sendo_birth_wait'],
    'src/actor/ac_boat_demo.c': ['aBTD_actor_ct', 'aBTD_actor_dt'],
    'src/game/m_actor.c': ['Actor_malloc_actor_class', 'Actor_info_delete'],
}
boat_source = (ROOT / 'src/actor/ac_boat.c').read_text()
chunks = [boat_source[boat_source.index('enum {'):boat_source.index('static void aBT_actor_ct')]]
for path, names in specs.items():
    source = (ROOT / path).read_text()
    for name in names:
        body = function(source, name)
        if body is None:
            raise RuntimeError(name)
        chunks.append(body)
(out / 'dock_source.inc').write_text('\n\n'.join(chunks))
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'dock.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                'pc/tests/vr_dock.c', '-o', str(exe)], cwd=ROOT, env=env, check=True)
run = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
(out / 'dock-results.txt').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)
