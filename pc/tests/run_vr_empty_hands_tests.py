"""Exercise production empty-hand gates and stereo routing without a headset.

Helpers are extracted unchanged from their C/C++ source. Only the OpenVR action
snapshot, GL state, and final hand renderer are simulated; player structs and
enums come from the game's headers. No ROM or user save is needed.
"""
import os
from pathlib import Path
import subprocess

from run_vr_tool_tests import function

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'pc/build32/vr-empty-hands-tests'


def extract(path, names, destination):
    source = (ROOT / path).read_text(encoding='utf-8')
    chunks = []
    for name in names:
        body = function(source, name)
        if body is None:
            raise RuntimeError(f'Missing production function: {path}:{name}')
        chunks.append(body)
    (OUT / destination).write_text('\n\n'.join(chunks), encoding='utf-8')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    extract('pc/src/pc_vr.cpp', [
        'm34_identity', 'm34_mul', 'm34_from_hmd', 'pc_vr_tool_input_allowed',
        'pc_vr_set_empty_hands_available', 'pcvr_empty_hands_visible',
        'pcvr_poll_empty_hands', 'pcvr_draw_empty_hands'], 'runtime_source.inc')
    cc_dir = Path('C:/msys64/mingw32/bin')
    env = dict(os.environ, PATH=str(cc_dir) + os.pathsep + os.environ['PATH'])

    def compile_test(args, name):
        result = subprocess.run(args, cwd=ROOT, env=env, capture_output=True, text=True)
        (OUT / (name + '-compile.txt')).write_text(result.stdout + result.stderr)
        if result.returncode:
            print(result.stdout + result.stderr, end='')
            result.check_returncode()

    exe = OUT / 'empty-hands-runtime.exe'
    compile_test([str(cc_dir / 'g++.exe'), '-std=c++11', '-O2', '-Wall', '-Wextra',
                    # Avoid this C header's obsolete MinGW `typedef char bool`.
                    # Keep _WIN32 defined, so its real __stdcall ABI is tested.
                    '-Wno-unused-variable', '-U__WIN32',
                    '-Ipc/include', '-Ipc/lib/openvr', '-I' + str(OUT),
                    'pc/tests/vr_empty_hands.cpp', '-o', str(exe)],
                   'runtime')
    result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
    (OUT / 'runtime-results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    extract('src/game/m_player_item.c_inc', [
        'Player_actor_empty_hands_free_state', 'Player_actor_empty_hands_available'],
        'player_source.inc')
    player_exe = OUT / 'empty-hands-player.exe'
    compile_test([str(cc_dir / 'gcc.exe'), '-std=gnu11', '-O2', '-Wall',
                    '-Wno-unused-function', '-fno-strict-aliasing', '-fwrapv',
                    '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                    '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(OUT),
                    'pc/tests/vr_empty_hands_player.c', '-o', str(player_exe)],
                   'player')
    player_result = subprocess.run([str(player_exe)], cwd=ROOT, env=env,
                                   capture_output=True, text=True)
    (OUT / 'player-results.txt').write_text(player_result.stdout + player_result.stderr)
    print(player_result.stdout + player_result.stderr, end='')
    return result.returncode or player_result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
