"""Verify police/tailor caps with original disc geometry and live palettes."""
import argparse
import os
from pathlib import Path
import re
import subprocess
from run_vr_tool_tests import ROOT


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/structure-followup/tests'
    out.mkdir(parents=True, exist_ok=True)
    table = (ROOT / 'pc/src/pc_assets.c').read_text()

    def asset(name):
        match = re.search(r'\{"assets/' + re.escape(name) + r'\.bin",[^,]+, (0x\w+), (0x\w+)', table)
        if not match:
            raise ValueError(name)
        return tuple(int(s, 16) for s in match.groups())

    assets, fixtures = [], []
    for index, name in enumerate(('obj_s_kouban', 'obj_w_kouban', 'obj_s_tailor', 'obj_w_tailor')):
        size, offset = asset(name + '_v')
        assets.append(f'{{{name}_v,{size},{offset},1}}')
        tex = name + ('_t1_tex_txt' if index < 2 else '_2_tex_txt')
        ts, to = asset(tex)
        assets.append(f'{{{tex},{ts},{to},0}}')
        pal = ('obj_police_box_pal', 'obj_police_box_winter_pal', 'obj_s_tailor_pal', 'obj_w_tailor_pal')[index]
        fixtures.append(f'{{"{name}",{size//16},{asset(pal)[1]}}}')
    (out / 'structure_fixtures.inc').write_text('static Asset assets[]={' + ','.join(assets) + '};\n'
            + 'static Fixture fixtures[]={' + ','.join(fixtures) + '};\n')
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'structure-backs.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
        '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
        '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
        'pc/tests/vr_structure_backs.c', 'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c',
        'src/data/model/obj_s_kouban.c', 'src/data/model/obj_s_tailor.c', '-o', str(exe)],
        cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
