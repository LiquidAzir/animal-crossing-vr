"""Execute real rod meshes, animation, item dispatch and matrix/tip callbacks."""
import argparse
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import ROOT, function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom-dir', type=Path, default=ROOT.parent / 'AnimalCrossing-VR')
    parser.add_argument('--revision', help='Regression baseline for the rod draw function')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/vr-rod-tests' / ('baseline' if args.revision else 'fixed')
    out.mkdir(parents=True, exist_ok=True)
    specs = [
        ('src/system/sys_matrix.c', ['Matrix_copy_MtxF', 'Matrix_push', 'Matrix_pull',
         'Matrix_put', 'Matrix_scale', 'Matrix_RotateY', 'Matrix_RotateZ', 'Matrix_rotateXYZ',
         'Matrix_softcv3_mult', 'Matrix_Position', 'Matrix_Position_VecX', 'Matrix_Position_VecZ']),
        ('src/game/m_skin_matrix.c', ['Skin_Matrix_SetRotateXyz_s', 'Skin_Matrix_SetScale']),
        ('src/c_keyframe.c', ['cKF_HermitCalc', 'cKF_KeyCalc', 'cKF_SkeletonInfo_R_play']),
        ('src/game/m_player_item_rod.c_inc', ['Player_actor_Item_draw_net_After_main4_sao',
         'Player_actor_Item_draw_rod_After', 'Player_actor_Item_draw_rod']),
        ('src/game/m_player_item.c_inc', ['Player_actor_empty_hands_free_state',
         'Player_actor_empty_hands_available', 'Player_actor_Item_draw']),
    ]
    parts = []
    for path, names in specs:
        source = (ROOT / path).read_text()
        if args.revision and path == 'src/game/m_player_item_rod.c_inc':
            source = subprocess.check_output(['git', 'show', f'{args.revision}:{path}'], cwd=ROOT, text=True)
        for name in names:
            body = function(source, name)
            if body is None:
                raise ValueError(f'Missing production function {name}')
            parts.append(body)
    (out / 'rod_source.inc').write_text('\n\n'.join(parts))
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'rod-orientation.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-DTARGET_PC',
        '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0', '-Iinclude', '-Isrc',
        '-Ipc/include', '-I.', '-I' + str(out), 'pc/tests/vr_rod_orientation.c',
        'src/data/model/player_tool.c', 'pc/src/pc_disc.c', 'pc/src/pc_gbi_runtime.c',
        '-o', str(exe)], cwd=ROOT, env=env, check=True)
    run = subprocess.run([str(exe), str(out / 'rod-poses.csv')], cwd=args.rom_dir,
                         env=env, text=True, capture_output=True)
    (out / 'results.txt').write_text(run.stdout + run.stderr)
    print(run.stdout + run.stderr, end='')
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
