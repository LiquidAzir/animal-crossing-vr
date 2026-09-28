"""Exercise actual GX/emu64 culling and rug display-list scope, without a ROM.

Optional --render-rel runs a hidden SDL/OpenGL reproduction using the original
disc vertices/palette/texture at pc_assets.c offsets. Assets remain in ignored
build output. --revision HEAD demonstrates the original routing regression.
"""
import argparse
import os
from pathlib import Path
import re
import struct
import subprocess
from run_vr_tool_tests import ROOT, function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision')
    parser.add_argument('--render-rel', type=Path)
    args = parser.parse_args()
    out = ROOT/'pc/build32/vr-rug-tests'/('baseline' if args.revision else 'fixed')
    out.mkdir(parents=True, exist_ok=True)

    def read(path):
        if args.revision:
            return subprocess.check_output(['git', 'show', f'{args.revision}:{path}'],
                                           cwd=ROOT, text=True, encoding='utf-8')
        return (ROOT/path).read_text(encoding='utf-8')

    gx = read('pc/src/pc_gx.c')
    emu = read('src/static/libforest/emu64/emu64.c')
    chunks = []
    for source, names in [(gx, ['pc_gx_apply_cull_mode', 'GXSetCullMode',
                               'pc_gx_set_authored_cull_mode']),
                          (emu, ['cullmode', 'dl_G_GEOMETRYMODE', 'emu64_taskstart_r'])]:
        for name in names:
            body = function(source, name)
            if body:
                chunks.append(body)
            elif name not in ('pc_gx_apply_cull_mode', 'pc_gx_set_authored_cull_mode'):
                raise ValueError(f'Missing production function: {name}')
    if not function(gx, 'pc_gx_set_authored_cull_mode'):
        chunks.insert(1, 'void pc_gx_set_authored_cull_mode(unsigned int m) { GXSetCullMode(m); }')
    (out/'rug_culling_source.inc').write_text('\n\n'.join(chunks), encoding='utf-8')
    (out/'rug_model.c').write_text(read('src/data/model/obj_shop_carpet.c'), encoding='utf-8')
    toolchain = Path('C:/msys64/mingw32/bin')
    env = dict(os.environ, PATH=str(toolchain)+os.pathsep+os.environ['PATH'])
    common = ['-O2', '-fno-strict-aliasing', '-fwrapv', '-DTARGET_PC',
              '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0', '-Iinclude',
              '-Isrc', '-Ipc/include', '-I.', '-I'+str(out)]
    objects = []
    for source in [out/'rug_model.c', ROOT/'pc/src/pc_gbi_runtime.c']:
        obj = out/(source.stem+'.o')
        subprocess.run([str(toolchain/'gcc.exe'), '-std=gnu11', '-w']+common+
                       ['-c', str(source), '-o', str(obj)], cwd=ROOT, env=env, check=True)
        objects.append(str(obj))
    exe = out/'rug-culling.exe'
    subprocess.run([str(toolchain/'g++.exe'), '-std=c++11', '-w']+common+
                   ['pc/tests/vr_rug_culling.cpp']+objects+['-o', str(exe)],
                   cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
    (out/'results.txt').write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr, end='')

    if args.render_rel:
        blob = args.render_rel.read_bytes()
        table = (ROOT/'pc/src/pc_assets.c').read_text()
        def asset(name):
            match = re.search(r'\{"assets/'+name+r'\.bin",[^,]+, (0x\w+), (0x\w+)', table)
            size, offset = (int(v, 16) for v in match.groups())
            data = blob[offset:offset+size]
            if len(data) != size:
                raise ValueError('REL does not contain '+name)
            return data
        vertices = list(struct.iter_unpack('>hhhHhh4B', asset('obj_shop_carpet_v')))
        pal = []
        for (value,) in struct.iter_unpack('>H', asset('obj_shop_carpet_pal')):
            pal.append(tuple(round(v*255/div) for v, div in (
                [((value>>10)&31,31), ((value>>5)&31,31), (value&31,31), (1,1)]
                if value&0x8000 else [((value>>8)&15,15), ((value>>4)&15,15),
                                     (value&15,15), ((value>>12)&7,7)])))
        tex = asset('obj_shop_carpet_tex')
        rgba = bytearray()
        for y in range(32):
            for x in range(32):
                n = ((y//8)*4+x//8)*64+(y%8)*8+x%8
                rgba.extend(pal[(tex[n//2] >> (0 if n%2 else 4))&15])
        (out/'rug-texture.rgba').write_bytes(rgba)
        tris, base, remaining = [], 0, 0
        for name, params in re.findall(r'(gsSPVertex|gsSPNTrianglesInit_5b|gsSPNTriangles_5b)\(([^)]*)\)',
                                       read('src/data/model/obj_shop_carpet.c')):
            if name == 'gsSPVertex':
                match = re.search(r'\[(\d+)\]', params)
                base = int(match[1]) if match else 0
                continue
            values = [int(n.strip()) for n in params.split(',')]
            if name == 'gsSPNTrianglesInit_5b':
                remaining = values.pop(0)
            for i in range(min(remaining, len(values)//3)):
                tris.append(tuple(base+v for v in values[i*3:i*3+3]))
                remaining -= 1
        pairs = [(a, b) for a in range(len(tris)) for b in range(a+1, len(tris))
                 if sorted(vertices[i][:3] for i in tris[a]) ==
                    sorted(vertices[i][:3] for i in tris[b])]
        if len(vertices) != 47 or len(tris) != 48 or len(pairs) != 20:
            raise ValueError('Original rug topology changed; review the scoped exception')
        packed = bytearray()
        for tri in tris:
            for index in tri:
                v = vertices[index]
                # Simple diagnostic vertex lighting, intentionally independent
                # of TEV. Geometry, UVs and texture are the original asset.
                ny = v[7] if v[7] < 128 else v[7]-256
                shade = .65+.35*max(0, ny/127)
                packed.extend(struct.pack('<6f', *v[:3], v[4]/1024, v[5]/1024, shade))
        (out/'rug-vertices.bin').write_bytes(packed)
        preview = out/'rug-render.exe'
        subprocess.run([str(toolchain/'g++.exe'), '-std=c++11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-Ipc/lib/glad/include', '-IC:/msys64/mingw32/include/SDL2',
                        'pc/tests/vr_rug_render.cpp', 'pc/build32/libglad.a',
                        '-LC:/msys64/mingw32/lib', '-lmingw32', '-lSDL2main', '-lSDL2', '-lopengl32',
                        '-o', str(preview)], cwd=ROOT, env=env, check=True)
        rendered = subprocess.run([str(preview)], cwd=out, env=env, capture_output=True,
                                  text=True, timeout=60)
        (out/'render-results.txt').write_text(rendered.stdout+rendered.stderr)
        print(rendered.stdout+rendered.stderr, end='')
        result.returncode |= rendered.returncode
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
