"""Verify residence rear caps against original disc geometry and every wall palette.

The expected contours describe the holes in the original models independently of
the implementation's triangulation. This runs the real helper and display lists;
it does not need a headset or modify the user's disc, saves, or settings.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]

# Original rear contours (vertex indices), measured in the authored models.
BOUNDARIES = {
    's_house1': [[41, 42, 44, 48, 50, 54, 57]],
    'w_house1': [[77, 78, 80, 84, 86, 90, 93]],
    's_house2': [[21, 22, 27, 25, 26, 15, 12]],
    'w_house2': [[21, 22, 27, 25, 26, 15, 12]],
    's_house3': [[22, 18, 19, 16, 14, 11, 23]],
    'w_house3': [[22, 18, 19, 16, 14, 11, 23]],
    's_house4': [[25, 19, 20, 26, 13, 14, 16]],
    'w_house4': [[25, 19, 20, 27, 13, 14, 16]],
    # The overhanging rear eave is a separate downward-facing surface, with
    # split vertices at its hard normal seam to the wall below.
    's_house5': [[62, 63, 68, 69], [63, 68, 83, 121], [112, 109, 88, 85]],
    'w_house5': [[117, 118, 123, 124], [118, 123, 74, 109], [100, 97, 79, 76]],
    's_myhome1': [[115, 112, 110, 108, 104, 107]],
    'w_myhome1': [[115, 112, 110, 108, 104, 107]],
    's_myhome2': [[106, 105, 103, 98, 101, 108]],
    'w_myhome2': [[106, 105, 103, 98, 101, 108]],
    's_myhome3': [[109, 106, 89, 88, 90, 93, 102, 105]],
    'w_myhome3': [[109, 106, 89, 88, 90, 93, 102, 105]],
    's_myhome4': [[121, 117, 125, 128]],
    'w_myhome4': [[121, 117, 125, 128]],
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
    parser.add_argument('--cc', default='C:/msys64/mingw32/bin/gcc.exe')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/vr-house-tests'
    out.mkdir(parents=True, exist_ok=True)
    table = (ROOT / 'pc/src/pc_assets.c').read_text()

    def asset(name):
        match = re.search(r'\{"assets/' + re.escape(name) + r'\.bin",[^,]+, (0x\w+), (0x\w+)', table)
        if not match:
            raise ValueError('Missing original asset metadata: ' + name)
        return tuple(int(v, 16) for v in match.groups())

    declarations, assets, fixtures = [], [], []
    for suffix, loops in BOUNDARIES.items():
        name = 'obj_' + suffix
        size, offset = asset(name + '_v')
        declarations.append(f'extern Vtx {name}_v[];')
        declarations.append(f'extern cKF_Skeleton_R_c cKF_bs_r_{name};')
        assets.append(f'{{{name}_v,{size},{offset},1}}')
        for tex in ('t1', 't2', 't3'):
            texname = name + '_' + tex + '_tex_txt'
            nbytes, address = asset(texname)
            declarations.append(f'extern u8 {texname}[];')
            assets.append(f'{{{texname},{nbytes},{address},0}}')
        season, kind = suffix.split('_', 1)
        prefix = f'obj_{season}_myhome_' if 'myhome' in kind else name + '_'
        palnames = re.findall(r'\{"assets/(' + prefix + r'[a-l]_pal)\.bin"', table)
        palettes = ','.join(str(asset(pal)[1]) for pal in palnames)
        contour = ','.join('{' + ','.join(map(str, loop)) + '}' for loop in loops)
        counts = ','.join(str(len(loop)) for loop in loops)
        fixtures.append(f'{{"{name}",&cKF_bs_r_{name},{name}_v,{size // 16},'
                        f'{len(loops)},{{{counts}}},{{{contour}}},{len(palnames)},{{{palettes}}}' + '}')
    (out / 'house_fixtures.inc').write_text('\n'.join(declarations) +
        '\nstatic Asset assets[]={\n' + ',\n'.join(assets) + '\n};\n' +
        'static Fixture fixtures[]={\n' + ',\n'.join(fixtures) + '\n};\n')
    cc = Path(args.cc)
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'house-backs.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                    'pc/tests/vr_house_backs.c', 'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c',
                    'src/data/model/obj_s_house1.c', 'src/data/model/obj_s_myhome1.c',
                    '-o', str(exe)], cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
