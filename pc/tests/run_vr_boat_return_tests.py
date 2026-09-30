"""Exercise boat return/respawn lifecycle from production C, without game data."""
import argparse
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import ROOT, function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', help='Optional baseline git revision')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/vr-boat-return-tests' / ('baseline' if args.revision else 'fixed')
    out.mkdir(parents=True, exist_ok=True)

    def read(path):
        if args.revision:
            return subprocess.check_output(['git', 'show', f'{args.revision}:{path}'],
                                           cwd=ROOT, text=True)
        return (ROOT / path).read_text(encoding='utf-8')

    boat = read('src/actor/ac_boat.c')
    collision = read('src/game/m_collision_bg_move.c_inc')
    type_end = collision.index('} mCoBG_boat_collision_c;')
    chunks = [boat[boat.index('enum {'):boat.index('static void aBT_actor_ct')],
              boat[boat.index('s16 aBT_init_angleY'):boat.index('static void aBT_setupAction')],
              collision[collision.rfind('typedef struct {', 0, type_end):
                        collision.index('static mCoBG_bg_size_c l_mCoBG_boat_size')]]
    specs = {
        'src/game/m_actor.c': ['Actor_info_fgName_search_sub'],
        'src/actor/ac_boat_move.c_inc': ['aBT_check_other_boat', 'aBT_check_alive', 'aBT_anchor'],
        'src/actor/ac_boat.c': ['aBT_actor_ct', 'aBT_actor_dt'],
        'src/game/m_collision_bg_move.c_inc': ['mCoBG_DeleteBoatCollision'],
        'src/actor/ac_boat_demo_move.c_inc': ['aBTD_check_sendo_and_boat', 'aBTD_anchor'],
        'src/actor/ac_boat_demo.c': ['aBTD_actor_ct'],
    }
    for path, names in specs.items():
        source = read(path)
        for name in names:
            code = function(source, name)
            assert code, name
            chunks.append(code)
    (out / 'boat_return_source.inc').write_text('\n\n'.join(chunks), encoding='utf-8', newline='\n')
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'boat-return.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out),
                    'pc/tests/vr_boat_return.c', '-o', str(exe)], cwd=ROOT, env=env, check=True)
    run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(run.stdout + run.stderr, encoding='utf-8', newline='\n')
    print(run.stdout + run.stderr, end='')
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
