"""Check diagnostic camera fitting with the native fixed-point matrix code.

--unfitted reproduces the old direct packing path as a failing control. No game,
ROM, graphics context, or headset is required.
"""
import argparse
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import ROOT, function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--unfitted', action='store_true')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/model-viewer-matrix-tests' / ('unfitted' if args.unfitted else 'fixed')
    out.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'pc/src/pc_model_viewer.c').read_text()
    helper = function(source, 'mv_fit_view_matrix')
    if not helper:
        raise RuntimeError('Cannot extract the production model-viewer matrix helper')
    if args.unfitted:
        helper = 'static f32 mv_fit_view_matrix(Mtx* packed, f32 view[4][4]) { guMtxF2L(view, packed); return 1.0f; }'
    (out / 'model_viewer_matrix_source.inc').write_text(helper)
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'model-viewer-matrix.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
        '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
        '-Iinclude', '-Isrc', '-Ipc/include', '-Ipc/lib/glad/include',
        '-IC:/msys64/mingw32/include/SDL2', '-I' + str(out),
        'pc/tests/model_viewer_matrix.c', 'pc/src/pc_mtx.c', '-o', str(exe)],
        cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
