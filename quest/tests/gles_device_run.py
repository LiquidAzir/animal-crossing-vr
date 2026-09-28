"""Build/run isolated ARM32 EGL pbuffer renderer checks via adb shell.

No game app is installed, started or modified. Production renderer sources are
compiled unchanged; only GLES-inapplicable desktop test setup/readback differs.
Outputs and receipts stay in ../research/gles-device-<timestamp> and a unique
child of /data/local/tmp/acquest-gles. Requires one authorized connected device.
"""
import datetime
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--only', choices=['tev','depth','sky','nes','srgb','sky_perf','stream','persistent'], help='Repeat one check or run an optional timing diagnostic')
args=parser.parse_args()
paths = json.loads((WORKSPACE/'toolchain/paths.json').read_text())
stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
OUT = WORKSPACE/'research'/('gles-device-'+stamp)
OUT.mkdir(parents=True)
REMOTE = '/data/local/tmp/acquest-gles/'+stamp
adb = paths['adb']
bin_dir = Path(paths['clang_bin'])
includes = [ROOT/'pc/include',ROOT/'include',WORKSPACE/'third_party/SDL/include',ROOT/'quest/tests',OUT]
common = ['--target=armv7a-linux-androideabi29','-DTARGET_PC','-O2','-Wno-macro-redefined']
common += ['-I'+str(p) for p in includes]

def run(command, log, timeout=90, check=True, quiet=False):
    result = subprocess.run([str(x) for x in command], cwd=ROOT, capture_output=True,
                            text=True, timeout=timeout)
    (OUT/log).write_text(result.stdout+result.stderr)
    if not quiet: print(result.stdout+result.stderr, end='', flush=True)
    if check: result.check_returncode()
    return result

def extract_function(source, signature):
    start = source.index(signature)
    opening = source.index('{',start)
    # Named NES routines contain no brace-bearing string/comment literals.
    depth=1; end=opening+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]+'\n'

nes = (ROOT/'pc/src/pc_nes_fixnes.c').read_text()
(OUT/'gles_device_nes_functions.inc').write_text('\n'.join(extract_function(nes,name) for name in [
    'static GLuint fixnes_compile_shader(', 'static void fixnes_init_gl(', 'void pc_fixnes_render_frame(']))
tests = {
    'tev': ['quest/tests/gles_device_tev.c'],
    'depth': ['quest/tests/gles_device_depth.cpp','pc/src/pc_vr_hands.cpp'],
    'sky': ['quest/tests/gles_device_sky.cpp','pc/src/pc_sky.cpp'],
    'nes': ['quest/tests/gles_device_nes.c'],
    'srgb': ['quest/tests/gles_device_srgb.c'],
}
if args.only == 'sky_perf':
    tests={'sky_perf':['quest/tests/gles_device_sky_perf.cpp','pc/src/pc_sky.cpp']}
elif args.only in ('stream','persistent'):
    gx=(ROOT/'pc/src/pc_gx.c').read_text()
    start=gx.index('/* BEGIN_ANDROID_VERTEX_STREAM:')
    end=gx.index('/* END_ANDROID_VERTEX_STREAM */',start)
    (OUT/'gles_device_stream_functions.inc').write_text(gx[start:end])
    tests={args.only:['quest/tests/gles_device_'+args.only+'.c']}
elif args.only:
    tests={args.only:tests[args.only]}
for name, sources in tests.items():
    cpp = sources[0].endswith('.cpp')
    compiler = bin_dir/('clang++.exe' if cpp else 'clang.exe')
    run([compiler,*common,*(['-std=c++11','-static-libstdc++'] if cpp else ['-std=c11']),
         *sources,'-lEGL','-lGLESv3','-lm','-o',OUT/name], name+'-compile.log')

run([adb,'get-state'],'device-state.log')
properties={}
for prop in ['ro.product.model','ro.product.device','ro.build.version.release','ro.build.version.sdk','ro.build.display.id']:
    result=run([adb,'shell','getprop',prop],prop+'.log',quiet=True)
    properties[prop]=result.stdout.strip()
print('Device:',properties,flush=True)
run([adb,'shell','mkdir','-p',REMOTE+'/shaders'],'device-mkdir.log')
for name in tests:
    run([adb,'push',OUT/name,REMOTE+'/'+name],name+'-push.log')
for shader in ['default.vert','default.frag']:
    run([adb,'push',ROOT/'pc/shaders'/shader,REMOTE+'/shaders/'+shader],shader+'-push.log')
run([adb,'shell','chmod','700',*(REMOTE+'/'+n for n in tests)],'device-chmod.log')
results={}
for name in tests:
    # REMOTE and names are generated from a digit timestamp/constant allowlist.
    arguments = ' shaders/default.vert shaders/default.frag' if name=='depth' else ''
    result=run([adb,'shell','cd '+REMOTE+' && ./'+name+arguments],name+'-results.log',check=False)
    results[name]=result.returncode
run([adb,'pull',REMOTE,OUT/'captures'],'device-pull.log')
sources = [ROOT/p for p in ['pc/include/pc_gl.h','pc/src/pc_gx_tev.c','pc/include/pc_shader_seed.h',
           'pc/src/pc_gx.c','pc/src/pc_vr_hands.cpp','pc/src/pc_sky.cpp','pc/src/pc_nes_fixnes.c',
           'pc/shaders/default.vert','pc/shaders/default.frag']]
receipt = {'remote':REMOTE,'device':properties,'results':results,'sources':{str(p.relative_to(ROOT)):
            hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
(OUT/'receipt.json').write_text(json.dumps(receipt,indent=2))
print('Results:',results,'receipt:',OUT/'receipt.json')
raise SystemExit(any(results.values()))
