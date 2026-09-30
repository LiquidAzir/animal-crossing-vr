"""Native integration of production floating hands with actual world shaders.

The world is drawn at physical near/far positions, using the same GX projection
and default.vert/default.frag as gameplay. This catches a depth convention bug
that a synthetic depth-clear occlusion test can accidentally reproduce itself.
Optional --renderer-source permits running the same assertions against a saved
older renderer for a negative regression demonstration, without modifying it.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--renderer-source', default='pc/src/pc_vr_hands.cpp')
parser.add_argument('--label', default='current')
args = parser.parse_args()
if not args.label.replace('-', '').replace('_', '').isalnum():
    raise ValueError('Label must contain only letters, digits, hyphens or underscores')
root = Path(__file__).resolve().parents[2]
out = root / 'pc/build32/vr-hand-world-depth-tests' / args.label
out.mkdir(parents=True, exist_ok=True)
renderer = (root / args.renderer_source).resolve()
toolchain = Path('C:/msys64/mingw32/bin')
env = dict(os.environ, PATH=str(toolchain) + os.pathsep + os.environ['PATH'])
command = [
    str(toolchain/'g++.exe'), '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ipc/include', '-Ipc/lib/glad/include', '-IC:/msys64/mingw32/include/SDL2',
    'pc/tests/vr_hand_world_depth.cpp', str(renderer), 'pc/build32/libglad.a',
    '-LC:/msys64/mingw32/lib', '-lmingw32', '-lSDL2main', '-lSDL2', '-lopengl32',
    '-o', str(out/'vr-hand-world-depth.exe')
]
compiled = subprocess.run(command, cwd=root, env=env, capture_output=True,
                          text=True, timeout=60)
(out/'compile.txt').write_text(compiled.stdout+compiled.stderr)
if compiled.returncode:
    print(compiled.stdout+compiled.stderr, end='')
compiled.check_returncode()
sources = [renderer, root/'pc/shaders/default.vert', root/'pc/shaders/default.frag']
(out/'source-sha256.txt').write_text('\n'.join(
    hashlib.sha256(path.read_bytes()).hexdigest()+' '+str(path) for path in sources)+'\n')
result = subprocess.run([str(out/'vr-hand-world-depth.exe'), str(sources[1]), str(sources[2])],
                        cwd=out, env=env, capture_output=True, text=True, timeout=60)
(out/'results.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr, end='')
raise SystemExit(result.returncode)
