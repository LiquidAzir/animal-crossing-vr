"""Test isolated clip helper; does not change or launch the game. Requires a quiet GPU slot."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import subprocess

SOURCE = Path(__file__).resolve().parents[2]
WORKSPACE = SOURCE.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--serial', required=True)
parser.add_argument('--compile-only', action='store_true')
args = parser.parse_args()
paths = json.loads((WORKSPACE/'toolchain/paths.json').read_text(encoding='utf-8-sig'))
stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
out = WORKSPACE/'research'/('batch-clip-'+stamp)
out.mkdir()
env = dict(os.environ)
env['PATH'] = r'C:\msys64\mingw32\bin;' + env.get('PATH','')

def run(command, log, check=True):
    result = subprocess.run([str(x) for x in command], capture_output=True, env=env, timeout=60)
    (out/log).write_bytes(result.stdout+result.stderr)
    print((result.stdout+result.stderr).decode(errors='replace'),end='',flush=True)
    if check: result.check_returncode()
    return result

common = ['-std=c11','-O2','-Wall','-Wextra','-I'+str(SOURCE/'quest/include')]
host = out/'math.exe'
run([r'C:\msys64\mingw32\bin\gcc.exe',*common,SOURCE/'quest/tests/batch_clip_math.c','-lm','-o',host],'host-compile.log')
run([host],'host-results.log')
gx_source=(SOURCE/'pc/src/pc_gx.c').read_text()
def function_text(name):
    start=gx_source.index('static ',gx_source.index(name)-40)
    opening=gx_source.index('{',gx_source.index(name))
    depth=1
    for end in range(opening+1,len(gx_source)):
        depth += (gx_source[end]=='{')-(gx_source[end]=='}')
        if depth==0:return gx_source[start:end+1]
    raise RuntimeError('Unclosed production function '+name)
prefix=gx_source[gx_source.index('void pc_gx_flush_vertices(void) {'):]
prefix=prefix[:prefix.index('    if (shader && shader != g_gx.current_shader) {')]
(out/'batch_clip_actual.inc').write_text(function_text('pc_gx_current_modelview')+'\n'+
    function_text('pc_gx_reject_offscreen_batch')+'\n'+prefix+'\n++accepted;\n}\n')
dispatch=out/'dispatch.exe'
run([r'C:\msys64\mingw32\bin\gcc.exe',*common,'-D__ANDROID__','-DPC_GX_ANDROID_BATCH_CLIP=1',
     '-I'+str(out),SOURCE/'quest/tests/batch_clip_dispatch.c','-lm','-o',dispatch],'dispatch-compile.log')
run([dispatch],'dispatch-results.log')
binary=out/'batch-clip'
run([paths['clang_armv7_api24'],*common,'-I'+str(SOURCE/'pc/include'),'-Wno-unused-function',
     SOURCE/'quest/tests/batch_clip_gles.c','-lEGL','-lGLESv3','-lm','-o',binary],'arm32-compile.log')
receipt={'sources':{str(p.relative_to(SOURCE)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [
    SOURCE/'quest/include/quest_batch_clip.h', SOURCE/'quest/tests/batch_clip_math.c',
    SOURCE/'quest/tests/batch_clip_gles.c', SOURCE/'quest/tests/batch_clip_dispatch.c',
    SOURCE/'pc/src/pc_gx.c', SOURCE/'pc/shaders/default.vert']},'serial':args.serial}
if not args.compile_only:
    adb=[paths['adb'],'-s',args.serial]
    remote='/data/local/tmp/acquest-batch-clip/'+stamp
    run([*adb,'shell','mkdir','-p',remote],'mkdir.log')
    run([*adb,'push',binary,remote+'/test'],'push.log')
    run([*adb,'push',SOURCE/'pc/shaders/default.vert',remote+'/default.vert'],'shader-push.log')
    run([*adb,'shell','chmod','700',remote+'/test'],'chmod.log')
    result=run([*adb,'shell',remote+'/test',remote+'/default.vert'],'device-results.log',check=False)
    receipt.update(remote=remote,device_exit=result.returncode)
(out/'receipt.json').write_text(json.dumps(receipt,indent=2))
print('Receipt:',out/'receipt.json')
raise SystemExit(receipt.get('device_exit',0))
