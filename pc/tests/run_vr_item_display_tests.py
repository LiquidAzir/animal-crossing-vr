"""Run real catch/find draw paths against instrumented matrix/graphics services.

The extracted fossil block is unchanged code from Player_actor_draw_Normal,
wrapped with its two original local aliases to avoid unrelated body animation.
Other production functions are extracted verbatim. No ROM or user save needed.
"""
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import function

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'pc/build32/vr-item-display-tests'


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    chunks = []
    for path, names in [
        ('src/game/m_player_draw.c_inc', ['Player_actor_vr_find_position']),
        ('src/actor/ac_insect_draw.c_inc', ['aINS_vr_catch_matrix', 'aINS_actor_draw_sub']),
        ('src/actor/ac_gyoei_draw.c_inc', ['aGYO_vr_catch_matrix', 'aGYO_draw_ticks',
          'aGYO_current_anime_frame', 'aGYO_anime_frame', 'aGYO_anime_frame_ticks', 'aGYO_actor_draw_fish']),
        ('src/effect/ef_yajirushi.c', ['eYajirushi_vr_presentation_matrix', 'eYajirushi_dw']),
    ]:
        source = (ROOT / path).read_text(encoding='utf-8')
        if 'gyoei' in path:
            chunks.append(source[:source.index('static int aGYO_current_anime_frame')])
        for name in names:
            body = function(source, name)
            if body is None:
                raise RuntimeError(f'Missing production function {path}:{name}')
            chunks.append(body)
    source = (ROOT / 'src/game/m_player_draw.c_inc').read_text(encoding='utf-8')
    start = source.index('    {\n        mActor_name_t item = EMPTY_NO;')
    end = source.index('    if (moving_in_boat)', start)
    chunks.append('static void test_fossil_draw(ACTOR* actorx, GAME* game) {\n'
                  '    PLAYER_ACTOR* player = (PLAYER_ACTOR*)actorx;\n'
                  '    GAME_PLAY* play = (GAME_PLAY*)game;\n' + source[start:end] + '}')
    (OUT / 'item_display_source.inc').write_text('\n\n'.join(chunks), encoding='utf-8')
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = OUT / 'item-display.exe'
    compile_result = subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing', '-fwrapv',
        '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
        '-Iinclude', '-Isrc', '-Ipc/include', '-I.', '-I' + str(OUT),
        'pc/tests/vr_item_display.c', 'pc/src/pc_gbi_runtime.c', '-o', str(exe)],
        cwd=ROOT, env=env, capture_output=True, text=True)
    (OUT / 'compile.txt').write_text(compile_result.stdout + compile_result.stderr)
    if compile_result.returncode:
        print(compile_result.stdout + compile_result.stderr)
        return compile_result.returncode
    result = subprocess.run([str(exe)], cwd=ROOT, env=env, capture_output=True, text=True)
    (OUT / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
