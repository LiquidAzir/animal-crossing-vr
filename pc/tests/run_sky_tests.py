"""Build and run the production sky on a hidden native OpenGL 3.3 context."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'pc/build32/sky-tests'
out.mkdir(parents=True, exist_ok=True)
toolchain = Path('C:/msys64/mingw32/bin')
env = dict(os.environ, PATH=str(toolchain) + os.pathsep + os.environ['PATH'])
subprocess.run([str(toolchain/'g++.exe'), '-std=c++11', '-O2', '-Wall', '-Wextra',
                '-Ipc/include', '-Ipc/lib/glad/include', '-IC:/msys64/mingw32/include/SDL2',
                'pc/tests/sky_render.cpp', 'pc/src/pc_sky.cpp', 'pc/build32/libglad.a',
                '-LC:/msys64/mingw32/lib', '-lmingw32', '-lSDL2main', '-lSDL2', '-lopengl32',
                '-o', str(out/'sky-render.exe')], cwd=root, env=env, check=True)
result = subprocess.run([str(out/'sky-render.exe')], cwd=out, env=env, capture_output=True, text=True)
(out/'results.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr, end='')
raise SystemExit(result.returncode)
