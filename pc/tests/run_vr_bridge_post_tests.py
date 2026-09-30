"""Disc-backed bridge-post contour, material, and cache regression checks."""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent/'AnimalCrossing-VR')
    args = parser.parse_args()
    out = ROOT/'pc/build32/bridge-post-tests'
    out.mkdir(parents=True, exist_ok=True)
    table = (ROOT/'pc/src/pc_assets.c').read_text()

    def asset(name):
        match = re.search(r'\{"assets/' + re.escape(name) + r'\.bin",[^,]+, (0x\w+), (0x\w+)', table)
        if not match:
            raise ValueError(name)
        return tuple(int(v, 16) for v in match.groups())

    assets, fixtures = [], []
    for name, palette in (('obj_s_bridgeA', 'obj_s_bridgeA_pal'),
                          ('obj_w_bridgeA', 'obj_w_bridgeA_pal')):
        for suffix, vertices in (('_v', 1), ('_t2_tex_txt', 0), ('_pal', 0)):
            size, offset = asset(name + suffix)
            assets.append(f'{{{name + suffix},{size},{offset},{vertices}}}')
        fixtures.append(f'{{"{name}",{asset(name + "_v")[0]//16},{asset(palette)[1]}}}')
    (out/'bridge_post_fixtures.inc').write_text('static Asset assets[]={' + ','.join(assets) + '};\n'
                                              'static Fixture fixtures[]={' + ','.join(fixtures) + '};\n')
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out/'bridge-post-backs.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                    'pc/tests/vr_bridge_post_backs.c', 'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c',
                    'src/data/model/obj_s_bridgeA.c',
                    '-o', str(exe)], cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
    (out/'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
