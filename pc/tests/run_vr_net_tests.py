"""Check the actual net draw/animation/matrix code with both original ROM meshes.

GPU submission and controller availability are seams. Geometry, skeletal pose,
matrix math, and catch points execute the production sources. No headset needed.
"""
import argparse
import os
from pathlib import Path
import subprocess

from run_vr_tool_tests import ROOT, function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
    parser.add_argument('--revision', help='Test an earlier net draw implementation')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/vr-net-tests' / ('baseline' if args.revision else 'fixed')
    out.mkdir(parents=True, exist_ok=True)

    specs = [
        ('src/system/sys_matrix.c', ['Matrix_copy_MtxF', 'Matrix_push', 'Matrix_pull',
         'Matrix_put', 'Matrix_scale', 'Matrix_RotateY', 'Matrix_RotateZ', 'Matrix_rotateXYZ', 'Matrix_softcv3_mult',
         'Matrix_Position', 'Matrix_Position_VecZ']),
        ('src/game/m_skin_matrix.c', ['Skin_Matrix_SetRotateXyz_s', 'Skin_Matrix_SetScale']),
        ('src/c_keyframe.c', ['cKF_HermitCalc', 'cKF_KeyCalc', 'cKF_SkeletonInfo_R_play']),
        ('src/game/m_player_item_net.c_inc', ['Player_actor_Item_draw_net_After_dummy_net',
         'Player_actor_Item_draw_net_After', 'Player_actor_Item_draw_net']),
        ('src/game/m_player_item.c_inc', ['Player_actor_empty_hands_free_state',
         'Player_actor_empty_hands_available', 'Player_actor_Item_draw']),
    ]
    parts = []
    for path, names in specs:
        source = (ROOT / path).read_text()
        if args.revision and path in ('src/game/m_player_item_net.c_inc', 'src/game/m_player_item.c_inc'):
            source = subprocess.check_output(['git', 'show', f'{args.revision}:{path}'],
                                             cwd=ROOT, text=True)
        for name in names:
            part = function(source, name)
            if not part and args.revision and name.startswith('Player_actor_empty_hands_'):
                continue  # Optional hands did not exist in the older net baseline.
            if not part:
                raise RuntimeError(f'Cannot extract {path}:{name}')
            parts.append(part)
    (out / 'net_source.inc').write_text('\n\n'.join(parts))
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'net-orientation.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                    '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                    'pc/tests/vr_net_orientation.c', 'src/data/model/player_tool.c',
                    'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c', '-o', str(exe)],
                   cwd=ROOT, env=env, check=True)
    run = subprocess.run([str(exe)], cwd=args.rom_dir, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(run.stdout + run.stderr)
    print(run.stdout + run.stderr, end='')
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
