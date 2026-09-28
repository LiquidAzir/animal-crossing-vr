"""Host regression for the exact pure Quest swapchain size selector; no device."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT.parent / 'build/eye-size-tests'
OUT.mkdir(parents=True, exist_ok=True)
CC = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(CC.parent)+os.pathsep+os.environ['PATH'])
exe = OUT/'eye-size-test.exe'
subprocess.run([str(CC), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I'+str(ROOT/'quest/include'), str(ROOT/'quest/tests/eye_size_test.c'),
                '-o', str(exe)], env=env, check=True)
result = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
(OUT/'results.txt').write_text(result.stdout+result.stderr, encoding='utf-8')
print(result.stdout+result.stderr, end='')
raise SystemExit(result.returncode)
