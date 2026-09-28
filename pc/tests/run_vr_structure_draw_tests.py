"""Exercise production structure draw routing and nested temporary state.

Model lookup and GPU submission are seams; separate disc-backed geometry tests
check the returned lists. This verifies that known repaired structures never
replay the old mirrored facade, including while their assets are unavailable.
"""
import os
from pathlib import Path
import subprocess

from run_vr_tool_tests import ROOT, function


def main():
    out = ROOT / 'pc/build32/vr-structure-draw-tests'
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'src/c_keyframe.c').read_text()
    names = ['cKF_shell_wanted', 'cKF_Si3_draw_SV_R_child', 'cKF_Si3_draw_R_SV_solid']
    parts = [function(source, name) for name in names]
    if not all(parts):
        raise RuntimeError('Production draw functions could not be extracted')
    (out / 'structure_draw_source.inc').write_text('\n\n'.join(parts))
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'structure-draw.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                    'pc/tests/vr_structure_draw.c', 'pc/src/pc_gbi_runtime.c', '-o', str(exe)],
                   cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
