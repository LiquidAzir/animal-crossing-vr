"""Exercise production camera transitions and dialogue draw placement, without ROMs.

Rendering/runtime services are instrumented by vr_presentation.c. The functions
under test are extracted from the source, not reimplemented in the harness.
--revision HEAD demonstrates the regressions against the previous build.
"""
import argparse
import os
from pathlib import Path
import subprocess

from run_vr_tool_tests import ROOT, function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--revision')
    parser.add_argument('--cc', default='C:/msys64/mingw32/bin/gcc.exe')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/vr-presentation-tests' / ('baseline' if args.revision else 'fixed')
    out.mkdir(parents=True, exist_ok=True)
    chunks = []
    for path, names in [
        ('pc/src/pc_fp_camera.c', ['pc_fp_set_in_talk', 'pc_fp_in_talk',
         'pc_fp_set_active', 'pc_fp_view_is_active', 'pc_fp_hide_player', 'pc_fp_view']),
        ('pc/src/pc_vr.cpp', ['pc_vr_tool_input_allowed']),
        ('src/game/m_camera2.c', ['Camera2_SetView']),
        ('src/game/m_msg.c', ['mMsg_Draw_Window']),
    ]:
        if args.revision:
            source = subprocess.check_output(['git', 'show', f'{args.revision}:{path}'],
                                             cwd=ROOT, text=True)
        else:
            source = (ROOT / path).read_text(encoding='utf-8')
        for name in names:
            body = function(source, name)
            if body is None:
                raise RuntimeError(f'Missing production function: {path}:{name}')
            chunks.append(body)
    (out / 'presentation_source.inc').write_text('\n\n'.join(chunks), encoding='utf-8')
    cc = str(Path(args.cc).resolve())
    env = dict(os.environ, PATH=str(Path(cc).parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'presentation.exe'
    subprocess.run([cc, '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                    'pc/tests/vr_presentation.c', '-o', str(exe)],
                   cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
