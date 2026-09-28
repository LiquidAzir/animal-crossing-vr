"""Check production scenery culling, dialogue masking, and matrix side effects.

Extracts the real culling and matrix functions; only the camera dialogue-area
query is a seam. Builds PC, PC widescreen, and original-target paths. No ROM,
game launch, user save, or settings changes are required.
"""
import os
from pathlib import Path
import subprocess

from run_vr_tool_tests import ROOT, function


def extract(path, names):
    source = (ROOT / path).read_text(encoding='utf-8')
    parts = [function(source, name) for name in names]
    if not all(parts):
        raise RuntimeError(f'Cannot extract production functions from {path}')
    return '\n\n'.join(parts)


def main():
    out = ROOT / 'pc/build32/bg-item-culling-tests'
    out.mkdir(parents=True, exist_ok=True)
    matrices = extract('src/game/m_skin_matrix.c', ['Skin_Matrix_PrjMulVector'])
    matrices += '\n\n' + extract('src/system/sys_matrix.c', [
        'Matrix_copy_MtxF', 'Matrix_put', 'Matrix_Position'])
    (out / 'culling_matrices_source.inc').write_text(matrices, encoding='utf-8')
    culling = extract('src/game/m_actor.c', ['Actor_draw_actor_no_culling_check2'])
    culling += '\n\n' + extract('src/bg_item/bg_item_common.c_inc', [
        'bg_item_common_culling_check', 'bg_item_common_culling_check_talk',
        'bg_item_common_culling_check_loop', 'bg_item_common_culling_check_talk_loop',
        'bg_item_common_check_talk_tree', 'bg_item_common_draw_check'])
    (out / 'bg_culling_source.inc').write_text(culling, encoding='utf-8')

    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    result_code = 0
    for label, definitions in [
        ('pc', ['-DTARGET_PC']),
        ('pc-widescreen', ['-DTARGET_PC', '-DPC_ENHANCEMENTS']),
        # Use host-compatible game types, then undefine TARGET_PC before the
        # extracted functions to exercise their original-target code paths.
        ('original', ['-DTARGET_PC', '-DTEST_ORIGINAL_CULLING']),
    ]:
        exe = out / f'bg-item-culling-{label}.exe'
        subprocess.run([str(cc), '-std=gnu11', '-O2', '-w', '-fno-strict-aliasing',
                        '-fwrapv', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                        *definitions, '-Iinclude', '-Isrc', '-Ipc/include', '-I.',
                        '-I' + str(out), 'pc/tests/bg_item_culling.c', '-o', str(exe)],
                       cwd=ROOT, env=env, check=True)
        result = subprocess.run([str(exe)], cwd=ROOT, env=env,
                                capture_output=True, text=True)
        receipt = f'{label}: {result.stdout}{result.stderr}'
        (out / f'{label}-results.txt').write_text(receipt, encoding='utf-8')
        print(receipt, end='')
        result_code |= result.returncode
    return result_code


if __name__ == '__main__':
    raise SystemExit(main())
