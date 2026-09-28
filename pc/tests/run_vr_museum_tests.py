"""Verify fitted museum surfaces with the user's original local disc assets."""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/museum-audit/tests'
    out.mkdir(parents=True, exist_ok=True)
    table = (ROOT / 'pc/src/pc_assets.c').read_text()
    assets, fixtures = [], []

    def asset(name):
        match = re.search(r'\{"assets/' + re.escape(name) + r'\.bin",[^,]+, (0x\w+), (0x\w+)', table)
        if not match:
            raise ValueError(name)
        return tuple(int(s, 16) for s in match.groups())

    for season, palette in [('s', 'obj_summer_museum_pal'), ('w', 'obj_winter_museum_pal')]:
        size, offset = asset(f'obj_{season}_museum_v')
        assets.append(f'{{obj_{season}_museum_v,{size},{offset},1}}')
        ts, to = asset(f'obj_{season}_museum_t1_tex')
        assets.append(f'{{obj_{season}_museum_t1_tex,{ts},{to},0}}')
        ps, po = asset(palette)
        assets.append(f'{{{palette},{ps},{po},2}}')
        fixtures.append(f'{{"museum_{season}",{size//16},{po}}}')
    (out / 'museum_fixtures.inc').write_text('static Asset assets[]={' + ','.join(assets) + '};\n'
        + 'static Fixture fixtures[]={' + ','.join(fixtures) + '};\n')
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'museum-backs.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
        '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
        '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
        'pc/tests/vr_museum_backs.c', 'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c',
        'src/data/model/obj_s_museum.c', '-o', str(exe)], cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
