"""Run the shared closure builder against original post-office disc assets."""
import argparse
import os
from pathlib import Path
import re
import subprocess
from run_vr_tool_tests import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/post-office-followup/tests'
    out.mkdir(parents=True, exist_ok=True)
    table = (ROOT / 'pc/src/pc_assets.c').read_text()
    civic = (ROOT / 'src/pc_civic_back.c_inc').read_text()
    names = ('pc_post_office_back_lookup', 'pc_post_office_light_for_back')
    extracted = [function(civic, name) for name in names]
    if not all(extracted):
        raise RuntimeError('Production post-office functions could not be extracted')
    (out / 'post_office_source.inc').write_text('\n\n'.join(extracted))

    def asset(name):
        match = re.search(r'\{"assets/' + re.escape(name) + r'\.bin",[^,]+, (0x\w+), (0x\w+)', table)
        if not match:
            raise ValueError(name)
        return tuple(int(s, 16) for s in match.groups())

    assets, fixtures = [], []
    for season in ('s', 'w'):
        name = f'obj_{season}_yubinkyoku'
        size, offset = asset(name + '_v')
        assets.append(f'{{{name}_v,{size},{offset},1}}')
        for suffix in ('_t2_txt',):
            ts, to = asset(name + suffix)
            assets.append(f'{{{name + suffix},{ts},{to},0}}')
        palette = 'obj_s_post_office_pal' if season == 's' else 'obj_s_post_office_winter_pal'
        fixtures.append(f'{{"{name}",{size//16},{asset(palette)[1]}}}')
    (out / 'post_office_fixtures.inc').write_text('static Asset assets[]={' + ','.join(assets) + '};\n'
            + 'static Fixture fixtures[]={' + ','.join(fixtures) + '};\n')
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'post-office-backs.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
        '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
        '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
        'pc/tests/vr_post_office_backs.c', 'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c',
        'src/data/model/obj_s_yubinkyoku.c', '-o', str(exe)],
        cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
