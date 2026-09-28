"""Check the production fountain closures using the user's original local disc."""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
args = parser.parse_args()
out = ROOT / 'pc/build32/fountain-tests'
out.mkdir(parents=True, exist_ok=True)
assets = (ROOT / 'pc/src/pc_assets.c').read_text()
source = (ROOT / 'src/data/model/obj_s_shrine.c').read_text()
declarations, fixtures = [], []
for season in 'sw':
    for part in ('trunk', 'figure'):
        name = f'obj_{season}_shrine_{part}'
        body = re.search(r'Gfx ' + name + r'_model\[\] = \{(.*?)\n\};', source, re.S)[1]
        slots, faces, remaining = {}, [], 0
        for command in re.finditer(r'gs(\w+)\(([^)]+)\)', body):
            op, values = command.groups()
            if op == 'SPVertex':
                base, count, start = map(int, re.search(r'\[(\d+)\],\s*(\d+),\s*(\d+)', values).groups())
                slots.update({start + i: base + i for i in range(count)})
            elif op in ('SPNTrianglesInit_5b', 'SPNTriangles_5b'):
                values = list(map(int, values.split(',')))
                if op == 'SPNTrianglesInit_5b':
                    assert remaining == 0
                    remaining = values.pop(0)
                used = min(remaining, len(values) // 3)
                faces.extend(slots[i] for i in values[:used * 3])
                remaining -= used
        assert remaining == 0
        def offset(symbol):
            return re.search(r'\{"assets/' + symbol + r'.bin",[^,]+, 0x\w+, (0x\w+)', assets)[1]
        vertex = offset(f'obj_{season}_shrine_v')
        texture = offset(f'obj_{season}_shrine_t{2 if part == "figure" else 3}_tex_txt')
        declarations.append('static const u16 ' + name + '_faces[]={' + ','.join(map(str, faces)) + '};')
        fixtures.append(f'{{"{name}",obj_{season}_shrine_v,{name}_model,{name}_faces,{len(faces)//3},{vertex},{texture},obj_{season}_shrine_pal,{int(part=="figure")}}}')
(out / 'shrine_fixtures.inc').write_text('\n'.join(declarations) + '\nstatic Fixture fixtures[]={\n' + ',\n'.join(fixtures) + '\n};\n')
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'shrine-backs.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out), 'pc/tests/vr_shrine_backs.c',
                'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c', '-o', str(exe)], cwd=ROOT, env=env, check=True)
run = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
(out / 'results.txt').write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)
