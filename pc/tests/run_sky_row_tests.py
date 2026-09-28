"""Compare the actual sky shader with/without conservative cloud-row rejection.

The baseline is reconstructed from the current production translation unit by
removing only the tagged bound block and renaming its exported functions. Every
other shader expression and renderer operation is shared. No ROM/headset needed.
Use --benchmark for optional native GPU timer-query comparisons; default is the
1,176-view byte-equality regression and GL/draw failure checks only.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--benchmark', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = root / 'pc/build32/sky-row-tests'
out.mkdir(parents=True, exist_ok=True)
source_path = root / 'pc/src/pc_sky.cpp'
source = source_path.read_text()
bound = re.compile(
    r'^            // Cloud row bounds:[^\n]*\n'
    r'            //[^\n]*\n'
    r'            float low = [^\n]*\n'
    r'            float high = [^\n]*\n'
    r'            float extent = [^\n]*\n'
    r'            if \([^\n]*\) continue;\n', re.MULTILINE)
baseline, replacements = bound.subn('', source)
if replacements != 1:
    raise RuntimeError('Expected exactly one tagged cloud-row bound block')
baseline = re.sub(r'\bpc_sky_(set_environment|begin_pass|set_view|draw|shutdown)\b',
                  r'baseline_pc_sky_\1', baseline)
(out / 'sky-baseline.cpp').write_text(baseline)
(out / 'source-sha256.txt').write_text(
    hashlib.sha256(source_path.read_bytes()).hexdigest() + ' pc/src/pc_sky.cpp\n')

toolchain = Path('C:/msys64/mingw32/bin')
env = dict(os.environ, PATH=str(toolchain) + os.pathsep + os.environ['PATH'])
command = [
    str(toolchain/'g++.exe'), '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ipc/include', '-Ipc/lib/glad/include', '-IC:/msys64/mingw32/include/SDL2',
    'pc/tests/sky_row_render.cpp', 'pc/src/pc_sky.cpp', str(out/'sky-baseline.cpp'),
    'pc/build32/libglad.a', '-LC:/msys64/mingw32/lib',
    '-lmingw32', '-lSDL2main', '-lSDL2', '-lopengl32',
    '-o', str(out/'sky-row-render.exe')
]
compiled = subprocess.run(command, cwd=root, env=env, capture_output=True,
                          text=True, timeout=60)
(out/'compile.txt').write_text(compiled.stdout+compiled.stderr)
if compiled.returncode:
    print(compiled.stdout+compiled.stderr, end='')
compiled.check_returncode()
run = [str(out/'sky-row-render.exe')]
if args.benchmark:
    run.append('--benchmark')
result = subprocess.run(run, cwd=out, env=env, capture_output=True,
                        text=True, timeout=60)
(out/'results.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr, end='')
raise SystemExit(result.returncode)
