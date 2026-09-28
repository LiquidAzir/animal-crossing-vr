"""Compile the real mitten renderer and exercise it in a hidden native GL context.

No ROM, headset, installed game, or user save is used. The BMP preview contains
the actual production mesh and shader at synthetic controller grip poses.
"""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'pc/build32/vr-hands-tests'
out.mkdir(parents=True, exist_ok=True)
toolchain = Path('C:/msys64/mingw32/bin')
env = dict(os.environ, PATH=str(toolchain) + os.pathsep + os.environ['PATH'])
subprocess.run([
    str(toolchain/'g++.exe'), '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ipc/include', '-Ipc/lib/glad/include', '-IC:/msys64/mingw32/include/SDL2',
    'pc/tests/vr_hands_render.cpp', 'pc/src/pc_vr_hands.cpp', 'pc/build32/libglad.a',
    '-LC:/msys64/mingw32/lib', '-lmingw32', '-lSDL2main', '-lSDL2', '-lopengl32',
    '-o', str(out/'vr-hands-render.exe')
], cwd=root, env=env, check=True)
result = subprocess.run([str(out/'vr-hands-render.exe')], cwd=out, env=env,
                        capture_output=True, text=True, timeout=60)
(out/'results.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr, end='')
raise SystemExit(result.returncode)
