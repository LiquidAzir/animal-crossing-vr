"""Check the compiled slope-acre geometry against vertices on a local game disc.

An optional --revision compiles that revision's original models to demonstrate
the holes before the fix. Generated geometry remains in ignored pc/build32.
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
parser.add_argument('--revision')
args = parser.parse_args()
out = ROOT / 'pc/build32/cliff-tests'
out.mkdir(parents=True, exist_ok=True)
models = []
for name in ('grd_s_c4_s_1', 'grd_s_c4_s_2'):
    path = Path('src/data/field/bg/acre') / name / (name + '.c')
    if args.revision:
        text = subprocess.check_output(['git', 'show', f'{args.revision}:{path.as_posix()}'], cwd=ROOT, text=True)
        source = out / (name + '-baseline.c')
        source.write_text(text)
    else:
        source = ROOT / path
    models.append('#include "' + source.as_posix() + '"')
(out / 'cliff_models.inc').write_text('\n'.join(models))
cc = Path('C:/msys64/mingw32/bin/gcc.exe')
env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
exe = out / 'cliff-geometry.exe'
subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                '-Iinclude', '-Ipc/include', '-I' + str(out), 'pc/tests/vr_cliff_geometry.c',
                'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c', '-o', str(exe)], cwd=ROOT, env=env, check=True)
run = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
if run.returncode:
    raise RuntimeError(f'Native display-list decoder failed ({run.returncode}): {run.stderr}')
(out / 'geometry.jsonl').write_text(run.stdout)
checks = 0


def check(condition, message):
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


def key_edges(points):
    return [(points[i], points[(i + 1) % 3]) for i in range(3)]


for line in run.stdout.splitlines():
    data = json.loads(line)
    vertices, triangles = data['vertices'], data['triangles']
    original = [t for t in triangles if not t[3]]
    added = [t for t in triangles if t[3]]
    check(len(added) == (2 if data['name'].endswith('_1') else 1), 'missing ramp-side repair triangles')
    edge_counts, directions = Counter(), Counter()
    for triangle in original:
        points = [tuple(vertices[i][:3]) for i in triangle[:3]]
        for a, b in key_edges(points):
            edge_counts[tuple(sorted((a, b)))] += 1
            directions[a, b] += 1
    for triangle in added:
        check(triangle[4] == 1, 'repair uses the existing cliff texture and palette')
        points = [tuple(vertices[i][:3]) for i in triangle[:3]]
        for a, b in key_edges(points):
            key = tuple(sorted((a, b)))
            check(edge_counts[key] == 1, 'each repaired edge was an actual hole, not an existing surface')
            check(directions[b, a] == 1 and directions[a, b] == 0, 'new winding opposes its exact neighboring edge')
        ab = [points[1][i] - points[0][i] for i in range(3)]
        ac = [points[2][i] - points[0][i] for i in range(3)]
        normal = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
        check(sum(n*n for n in normal) > 0, 'new face has nonzero area')
        check(min(v[1] for v in points) < max(v[1] for v in points), 'repair is a cliff side, not a flat ground patch')
        # The real neighboring cliff vertices must carry identical UVs and
        # normals, so texture/lighting cannot open a seam at the patch boundary.
        for vertex_id in triangle[:3]:
            vertex = vertices[vertex_id]
            neighbors = [vertices[i] for t in original if t[4] for i in t[:3] if vertices[i][:3] == vertex[:3]]
            check(any(other == vertex for other in neighbors), 'repair preserves original cliff UVs and lighting')
    for triangle in added:
        for a, b in key_edges([tuple(vertices[i][:3]) for i in triangle[:3]]):
            edge_counts[tuple(sorted((a, b)))] += 1
            directions[a, b] += 1
    for triangle in added:
        for a, b in key_edges([tuple(vertices[i][:3]) for i in triangle[:3]]):
            check(edge_counts[tuple(sorted((a, b)))] == 2 and directions[a, b] == directions[b, a],
                  'repaired boundary is closed without duplicate or nonmanifold surfaces')
    print(f"{data['name']}: {len(added)} missing ramp-side faces closed")
print(f'Cliff geometry: {checks} checks passed')
