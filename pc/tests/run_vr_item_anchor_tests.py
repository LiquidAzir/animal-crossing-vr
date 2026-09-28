"""Verify the production catch anchor in physical head space, without a headset."""
import os
from pathlib import Path
import subprocess

from run_vr_tool_tests import function

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'pc/build32/vr-item-anchor-tests'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'pc/src/pc_vr.cpp').read_text(encoding='utf-8')
    chunks = []
    for name in ('m34_mul', 'pc_vr_item_presentation_mtx'):
        body = function(source, name)
        if body is None:
            raise RuntimeError('Missing production function: ' + name)
        chunks.append(body)
    (OUT / 'anchor_source.inc').write_text('\n\n'.join(chunks), encoding='utf-8')
    cc_dir = Path('C:/msys64/mingw32/bin')
    env = dict(os.environ, PATH=str(cc_dir) + os.pathsep + os.environ['PATH'])
    exe = OUT / 'item-anchor.exe'
    build = subprocess.run([str(cc_dir / 'g++.exe'), '-std=c++11', '-O2',
                            '-Wall', '-Wextra', '-I' + str(OUT),
                            'pc/tests/vr_item_anchor.cpp', '-o', str(exe)],
                           cwd=ROOT, env=env, text=True, capture_output=True)
    (OUT / 'compile.txt').write_text(build.stdout + build.stderr)
    if build.returncode:
        print(build.stdout + build.stderr, end='')
        return build.returncode
    result = subprocess.run([str(exe)], cwd=ROOT, env=env, text=True, capture_output=True)
    (OUT / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
