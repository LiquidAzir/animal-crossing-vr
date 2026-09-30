"""Check actual dock/bridge lists and supplemental faces against a local disc.

The expected wooden solids are derived from each original deck/post outline,
independently of the repair metadata. No extracted game assets are committed.
"""
import argparse
from collections import Counter
import json
import math
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/crossing-tests'
    out.mkdir(parents=True, exist_ok=True)
    models = sorted(p for p in (ROOT / 'src/data/field/bg/acre').glob('*/*.c')
                    if not p.stem.startswith('tmp') and
                    re.search(r'bridge_[12]_tex_dummy', p.read_text()))
    metadata = (ROOT / 'src/pc_crossing_back_data.c_inc').read_text()
    repaired = re.findall(r'extern Gfx (\w+)_model\[\];', metadata)
    externs, loads = set(), []
    for p in models:
        externs.update(re.findall(r'extern u8 (\w+)\[\]', p.read_text()))
        name = p.stem
        loads.append(f'_pc_load_src_data_field_bg_acre_{name}_{name}_c();')
        loads.append(f'dump("{name}", {name}_v, ARRAY_COUNT({name}_v), '
                     f'{name}_model, ARRAY_COUNT({name}_model));')
    (out / 'crossing_models.inc').write_text(
        '\n'.join('u8 ' + name + '[8192];' for name in sorted(externs)) + '\n' +
        '\n'.join('#include "' + p.as_posix() + '"' for p in models) + '\n' +
        'static const char* crossing_names[] = {' +
        ','.join('"' + name + '-caps"' for name in repaired) + '};\n')
    (out / 'crossing_load.inc').write_text('\n'.join(loads))
    (out / 'crossing_capacity.inc').write_text('\n'.join(
        'static const unsigned ' + key + '_capacity[] = {' +
        ','.join('ARRAY_COUNT(' + pattern.format(name=name) + ')' for name in repaired) + '};'
        for key, pattern in [('vertex', 'pc_crossing_{name}_v'),
                             ('list', 'pc_crossing_{name}_dl'), ('source', '{name}_v')]))
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'crossing-backs.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Ipc/include', '-I.', '-I' + str(out),
                    'pc/tests/vr_crossing_backs.c', 'pc/src/pc_disc.c',
                    'pc/src/pc_gbi_runtime.c', '-o', str(exe)], cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'Production display-list validation failed ({result.returncode}): {result.stderr}')
    (out / 'geometry.jsonl').write_text(result.stdout)
    meshes = {d['name']: d for d in map(json.loads, result.stdout.splitlines())}
    checks = 0

    def check(ok, message):
        nonlocal checks
        checks += 1
        if not ok:
            raise AssertionError(message)

    def sub(a, b):
        return tuple(x - y for x, y in zip(a, b))

    def cross(a, b):
        return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])

    def dot(a, b):
        return sum(x*y for x, y in zip(a, b))

    def normal(t):
        return cross(sub(t[1], t[0]), sub(t[2], t[0]))

    def area(t):
        return math.sqrt(dot(normal(t), normal(t))) / 2

    def components(triangles):
        parts = []
        for t in triangles:
            points = set(t)
            for part in [p for p in parts if p & points]:
                points |= part
                parts.remove(part)
            parts.append(points)
        return parts

    completed = 0
    for name in [p.stem for p in models]:
        original = meshes[name]
        verts = original['vertices']
        source = [tuple(tuple(verts[i][:3]) for i in t[:3])
                  for t in original['triangles'] if t[4]]
        cap = meshes.get(name + '-caps')
        added = [] if cap is None else [tuple(tuple(cap['vertices'][i][:3]) for i in t[:3])
                                       for t in cap['triangles']]
        material = next(t[4] for t in original['triangles'] if t[4])
        label = name + ': '
        for t in added:
            check(area(t) > 0, label + 'nondegenerate new face')
            check(not any(set(t) == set(old) for old in source), label + 'no duplicate original triangle')
        if cap:
            check(all(t[4] == material and t[3] == 1 for t in cap['triangles']),
                  label + 'original bridge atlas/palette used for every new triangle')
            check(len(added) <= 56, label + 'bounded per-acre geometry')
            for triangle in cap['triangles']:
                points = [cap['vertices'][i] for i in triangle[:3]]
                n = normal([v[:3] for v in points])
                check(all(dot(n, v[5:8]) > 0 for v in points), label + 'lighting normals face outward')
        if material == 2:
            matched = Counter()
            for part in components(source):
                top_y, bottom_y = max(p[1] for p in part), min(p[1] for p in part)
                top = [p for p in part if p[1] == top_y]
                check(len(top) == 4, label + 'original four-corner solid')
                cx, cz = (sum(p[j] for p in top)/4 for j in (0, 2))
                top.sort(key=lambda p: -math.atan2(p[2] - cz, p[0] - cx))
                faces = [top]
                for a, b in zip(top, top[1:] + top[:1]):
                    faces.append([b, a, (a[0], bottom_y, a[2]), (b[0], bottom_y, b[2])])
                if top_y != 720:  # Post bases are intentionally below the waterline.
                    faces.append([(p[0], bottom_y, p[2]) for p in reversed(top)])
                for face in faces:
                    n = normal(face[:3])
                    expected = area(face[:3]) + area([face[0], face[2], face[3]])
                    in_face = []
                    for t in source + added:
                        if all(dot(n, sub(p, face[0])) == 0 for p in t):
                            # Rectangle coordinates, rather than an axis-aligned
                            # bounding box, distinguish neighboring diagonal posts.
                            u, w = sub(face[1], face[0]), sub(face[3], face[0])
                            if all(0 <= dot(sub(p, face[0]), u) <= dot(u, u) and
                                   0 <= dot(sub(p, face[0]), w) <= dot(w, w) for p in t):
                                in_face.append(t)
                    check(abs(sum(area(t) for t in in_face) - expected) < .01,
                          label + 'complete solid face with no excess or missing area')
                    for t in in_face:
                        check(dot(normal(t), n) > 0, label + 'outward winding')
                        if t in added:
                            matched[t] += 1
                    completed += 1
            check(all(matched[t] == 1 for t in added), label + 'every cap belongs to one authored opening')
        else:
            # East/west stone bridges omit the north wall; north/south variants
            # already have both walls and must remain untouched.
            expected = []
            for part in components(source):
                ids = {i for t in original['triangles'] if t[4] for i in t[:3] if tuple(verts[i][:3]) in part}
                front = [i for i in ids if verts[i][4] == 1024]
                rear = [i for i in ids if verts[i][4] == -1024]
                axis = next(j for j in (0, 2) if len({verts[i][j] for i in front}) ==
                            len({verts[i][j] for i in rear}) == 1)
                center_sum = verts[front[0]][axis] + verts[rear[0]][axis]
                local = [t[:3] for t in original['triangles'] if t[4] and t[0] in ids]
                if any(all(verts[i][4] <= -1024 for i in t) and any(verts[i][4] < -1024 for i in t) for t in local):
                    continue
                for t in local:
                    if all(verts[i][4] >= 1024 for i in t) and any(verts[i][4] > 1024 for i in t) and area([verts[i][:3] for i in t]) > 0:
                        points = []
                        for i in reversed(t):
                            p = list(verts[i][:3]); p[axis] = center_sum - p[axis]; points.append(tuple(p))
                        expected.append(tuple(points))
            check(Counter(added) == Counter(expected), label + 'only the missing opposite stone wall')
        print(f'{name}: {len(added)} additional triangles')
    check(len(repaired) == 31 and len(models) == 35, 'all authored dock/bridge variants audited')
    routing = (ROOT / 'src/actor/ac_field_draw.c').read_text()
    check('if (pc_fp_view_is_active() || pc_vr_active())' in routing,
          'supplemental geometry restricted to first-person/VR')
    print(f'Crossing geometry: {checks} checks, {completed} wooden solid faces passed')


if __name__ == '__main__':
    main()
