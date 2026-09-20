"""Verify all seasonal rock display lists against original topology and ROM vertices."""
import argparse
import os
from pathlib import Path
import re
import subprocess
from run_vr_tool_tests import ROOT

parser = argparse.ArgumentParser()
parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
args = parser.parse_args()
out = ROOT / 'pc/build32/vr-polish-tests'
out.mkdir(parents=True, exist_ok=True)
asset_table = (ROOT / 'pc/src/pc_assets.c').read_text()
declarations, fixtures, sources = [], [], []
for season in 'sw':
    for kind in 'ABCDE':
        name = f'obj_{season}_stone{kind}'
        src_path = f'src/data/model/{name}.c'
        src = (ROOT / src_path).read_text()
        match = re.search(r'\{"assets/' + name + r'_v.bin",[^,]+, (0x\w+), (0x\w+)', asset_table)
        count = int(match[1], 16) // 16
        triangles, remaining = [], 0
        for command in re.finditer(r'gsSPNTriangles(Init)?_5b\(([^)]+)', src):
            values = [int(v.strip()) for v in command[2].split(',')]
            if command[1]:
                if remaining:
                    raise RuntimeError('unfinished triangle batch')
                remaining = values.pop(0)
            used = min(len(values) // 3, remaining)
            triangles.extend(values[:used * 3])
            remaining -= used
        if remaining:
            raise RuntimeError('unfinished triangle batch')
        declarations.append('static const u8 ' + name + '_faces[]={' + ','.join(map(str, triangles)) + '};')
        fixtures.append(f'{{"{name}",{name}_v,{name}_gfx_model,{match[2]},{count},{len(triangles)//3},{name}_faces}}')
        sources.append(src_path)
(out / 'rock_fixtures.inc').write_text('\n'.join(declarations) + '\nstatic Fixture fixtures[]={\n' + ',\n'.join(fixtures) + '\n};\n')
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'rock-backs.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                'pc/tests/vr_rock_backs.c', 'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c',
                *sources, '-o', str(exe)], cwd=ROOT, env=env, check=True)
run = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
(out / 'rock-results.txt').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)
