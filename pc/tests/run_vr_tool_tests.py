"""Compile actual tool-request functions against a small simulated game/runtime.

The generated fragment is extracted from the source under test, never maintained
as a second implementation. World queries are seams; the rod's actual destination
calculation and the real request ordering/facing writes are executed. No ROM needed.
Run with the same i686 GCC used by the game. --revision HEAD tests the old behavior.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    # Match a definition, not a prototype; ignore braces in comments and strings.
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                    lambda m: ' ' * len(m[0]), source, flags=re.S)
    match = re.search(r'\b' + re.escape(name) + r'\s*\([^;{}]*\)\s*\{', masked)
    if not match:
        return None
    start = source.rfind('\n', 0, match.start()) + 1
    pos = masked.index('{', match.start())
    depth = 1
    end = pos + 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[start:end].replace('extern "C" ', '')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--revision')
    parser.add_argument('--cc', default='C:/msys64/mingw32/bin/gcc.exe')
    args = parser.parse_args()
    out = ROOT / 'pc/build32/vr-tool-tests' / ('baseline' if args.revision else 'fixed')
    out.mkdir(parents=True, exist_ok=True)

    def read(path):
        if args.revision:
            return subprocess.check_output(
                ['git', '-c', f'safe.directory={ROOT.as_posix()}', 'show',
                 f'{args.revision}:{path}'], cwd=ROOT, text=True)
        return (ROOT / path).read_text(encoding='utf-8')

    specs = [
        ('pc/src/pc_vr.cpp', ['pc_vr_tool_input_allowed', 'pc_vr_head_yaw_offset_bang']),
        ('pc/src/pc_fp_camera.c', ['pc_fp_camera_yaw']),
        ('src/game/m_player_controller.c_inc', ['Player_actor_CheckController_forAxe',
         'Player_actor_CheckController_forScoop', 'Player_actor_CheckController_forNet',
         'Player_actor_CheckController_forRod']),
        ('src/game/m_camera2.c', ['getCamera2AngleY']),
        ('src/game/m_player_controller.c_inc', ['Player_actor_GetController_move_angle']),
        ('src/game/m_player_common.c_inc', ['Player_actor_Get_ControllerAngle',
         'Player_actor_Get_DiffWorldAngleToControllerAngle', 'Player_actor_Check_is_demo_mode',
         'Player_actor_face_vr_tool_target', 'Player_actor_CheckAndRequest_main_axe_all',
         'Player_actor_CheckAndRequest_main_scoop_all', 'Player_actor_Get_player_move_position',
         'Player_actor_SetPosition_OBJtoLine_forItem']),
        ('src/game/m_player_main_ready_net.c_inc', ['Player_actor_request_main_ready_net']),
        ('src/game/m_player_main_swing_net.c_inc', ['Player_actor_request_main_swing_net']),
        ('src/game/m_player_main_ready_rod.c_inc', ['Player_actor_request_main_ready_rod',
         'Player_actor_request_proc_index_fromReady_rod']),
        ('pc/src/pc_vr.cpp', ['pc_vr_merge_pad']),
    ]
    chunks = []
    for path, names in specs:
        source = read(path)
        for name in names:
            body = function(source, name)
            if body is None and name in ('pc_vr_tool_input_allowed', 'Player_actor_face_vr_tool_target'):
                continue  # These helpers do not exist in the regression baseline.
            if body is None:
                raise RuntimeError(f'Missing production function: {path}:{name}')
            chunks.append(body)
    (out / 'tool_source.inc').write_text('\n\n'.join(chunks), encoding='utf-8')
    cc = str(Path(args.cc).resolve())
    env = dict(os.environ)
    env['PATH'] = str(Path(cc).parent) + os.pathsep + env['PATH']
    common = [cc, '-std=gnu11', '-O2', '-fno-strict-aliasing', '-fwrapv',
              '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
              '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(out)]
    if not args.revision:
        common.append('-DTEST_FIXED')
    exe = out / 'targeting.exe'
    subprocess.run(common + ['pc/tests/vr_tool_targeting.c', '-o', str(exe)],
                   cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=ROOT, env=env, text=True, capture_output=True)
    (out / 'targeting-results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout, end='')
    if result.stderr:
        print(result.stderr, end='')
    if not args.revision and (ROOT / 'pc/include/pc_vr_swing.h').exists():
        gesture = out / 'gesture.exe'
        subprocess.run(common + ['pc/tests/vr_swing.c', '-o', str(gesture)],
                       cwd=ROOT, env=env, check=True)
        motion = subprocess.run([str(gesture)], cwd=ROOT, env=env, text=True, capture_output=True)
        (out / 'gesture-results.txt').write_text(motion.stdout + motion.stderr)
        print(motion.stdout, end='')
        if motion.stderr:
            print(motion.stderr, end='')
        result.returncode |= motion.returncode
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
