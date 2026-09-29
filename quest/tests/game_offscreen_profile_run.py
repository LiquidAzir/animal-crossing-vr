"""Bounded shell-only title profiler; no Activity or app-private data."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
WORKSPACE=ROOT.parent
stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
OUT=WORKSPACE/'research'/('game-offscreen-'+stamp)
OUT.mkdir(parents=True)
paths=json.loads((WORKSPACE/'toolchain/paths.json').read_text())
REMOTE='/data/local/tmp/acquest-offscreen/'+stamp
build=WORKSPACE/'build/game-arm32'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--libmain',type=Path,default=build/'game/libmain.so',
                    help='Built ARM32 game library to profile; no game rebuild is performed')
parser.add_argument('--stock-world',action='store_true',help='Disable the default Quest full-world rendering')
parser.add_argument('--draw-radius',type=int,default=0,help='Terrain acre radius for comparison,0=whole town,1..10=window')
parser.add_argument('--stereo',action='store_true',help='Replay both eyes with production render helpers and fixed test poses (no XR)')
parser.add_argument('--yaw',type=int,default=0,help='Synthetic headset yaw in degrees,-180..180; stereo only')
parser.add_argument('--eye-size',type=int,default=640,help='Square target pixels per synthetic eye,64..2048(default640)')
parser.add_argument('--save-copy',type=Path,help='Read-only source GCI file to copy into disposable test SlotA')
parser.add_argument('--pad-script',type=Path,help='JSON with stop_frame,events,and captures counted after the first world draw')
parser.add_argument('--no-profile',action='store_true',help='Disable per-draw profiler; measure90world frames after30warmup frames')
parser.add_argument('--warmup-frames',type=int,default=30,help='World frames to exclude from timing,1..600(default30)')
parser.add_argument('--sample-frames',type=int,default=90,help='World frames to time without per-draw profiling,1..1800(default90)')
parser.add_argument('--compile-only',action='store_true',help='Build the test executable without accessing a device')
args=parser.parse_args()
if not 0<=args.draw_radius<=10:parser.error('--draw-radius must be0..10')
if not -180<=args.yaw<=180:parser.error('--yaw must be-180..180')
if args.yaw and not args.stereo:parser.error('--yaw requires --stereo')
if not 64<=args.eye_size<=2048:parser.error('--eye-size must be64..2048')
if not 1<=args.warmup_frames<=600:parser.error('--warmup-frames must be1..600')
if not 1<=args.sample_frames<=1800:parser.error('--sample-frames must be1..1800')
if args.save_copy and (not args.save_copy.is_file() or args.save_copy.suffix.lower()!='.gci'):
    parser.error('--save-copy must identify an existing GCI file')
script=None
script_file=None
if args.pad_script:
    script=json.loads(args.pad_script.read_text(encoding='utf-8'))
    def frame(value):
        if type(value) is not int or not 1<=value<=36000:raise ValueError('Script frames must be integers1..36000')
        return value
    stop=frame(script['stop_frame'])
    events=script.get('events',[])
    captures=script.get('captures',[])
    if len(events)>128 or len(captures)>128:raise ValueError('At most128 events and captures')
    buttons={'LEFT':1,'RIGHT':2,'DOWN':4,'UP':8,'Z':16,'R':32,'L':64,
             'A':256,'B':512,'X':1024,'Y':2048,'START':4096}
    lines=[f'STOP {stop}']
    for event in events:
        at=frame(event['frame']);duration=frame(event.get('duration',2))
        mask=0
        for button in event.get('buttons',[]):mask|=buttons[button]
        x=event.get('stick_x',0);y=event.get('stick_y',0)
        if type(x) is not int or type(y) is not int or not (-100<=x<=100 and -100<=y<=100):
            raise ValueError('Script sticks must be integers-100..100')
        lines.append(f'PAD {at} {duration} {mask} {x} {y}')
    lines.extend(f'CAPTURE {frame(at)}' for at in captures)
    script_file=OUT/'input.txt';script_file.write_text('\n'.join(lines)+'\n')
exe=OUT/'game-offscreen-profile'
command=[paths['clang_armv7_api24'],'-std=c11','-O2','-fPIE','-pie','-DTARGET_PC',
         '-Wl,--export-dynamic','-I'+str(WORKSPACE/'third_party/SDL/include'),
         '-I'+str(ROOT/'include'),'-I'+str(ROOT/'quest/tests'),
         str(ROOT/'quest/tests/game_offscreen_profile.c'),'-L'+str(build/'sdl'),
         '-lSDL2','-lGLESv3','-lEGL','-ldl','-o',str(exe)]
result=subprocess.run(command,text=True,capture_output=True)
(OUT/'compile.log').write_text(result.stdout+result.stderr)
if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
if args.stereo:
    from game_offscreen_stereo_extract import generate
    generate(ROOT,OUT/'game_offscreen_stereo_generated.c_inc')
    includes=['-I'+str(WORKSPACE/'third_party/SDL/include'),'-I'+str(ROOT/'include'),
              '-I'+str(ROOT/'quest/tests'),'-I'+str(ROOT/'pc/include'),'-I'+str(OUT),
              '-I'+str(WORKSPACE/'third_party/OpenXR-SDK/include')]
    c_object=OUT/'game_offscreen_profile.o'
    commands=[
        [paths['clang_armv7_api24'],'-std=c11','-O2','-fPIE','-DTARGET_PC','-DOFFSCREEN_STEREO',
         *includes,'-c',str(ROOT/'quest/tests/game_offscreen_profile.c'),'-o',str(c_object)],
        [paths['clangxx_armv7_api24'],'-std=c++17','-O2','-fPIE','-pie','-DTARGET_PC',
         '-nostdlib++','-fno-exceptions','-fno-rtti','-fno-threadsafe-statics',
         '-Wl,--export-dynamic',*includes,str(ROOT/'quest/tests/game_offscreen_stereo.cpp'),
         str(c_object),'-L'+str(build/'sdl'),'-lSDL2','-lGLESv3','-lEGL','-ldl','-o',str(exe)]
    ]
    for command in commands:
        result=subprocess.run(command,text=True,capture_output=True)
        with (OUT/'compile.log').open('a') as log:log.write(result.stdout+result.stderr)
        if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
if args.compile_only:
    print('Compiled test only:',exe)
    raise SystemExit(0)
settings=OUT/'settings.ini'
settings.write_text(f'[Graphics]\nwindow_width=640\nwindow_height=480\nfullscreen=0\nvsync=0\nmax_fps=60\nmsaa=0\n[Enhancements]\npreload_textures=0\n[FirstPerson]\nfp_mode=0\nvr_draw_radius={args.draw_radius}\n[VR]\nvr_mode=0\n')
adb=paths['adb']
def run(args,check=True,timeout=60):
    result=subprocess.run([adb,*args],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=timeout)
    if check:result.check_returncode()
    return result
live_app=run(['shell','pidof com.liquidazir.animalcrossingquest'],check=False)
if live_app.stdout.strip():
    raise SystemExit('Installed Quest game is running; skipped isolated GPU test to avoid contention.')
run(['shell',f'mkdir -p {REMOTE}/rom {REMOTE}/shaders {REMOTE}/save/card_a {REMOTE}/save/card_b && chmod 700 {REMOTE}'])
files={exe:'game-offscreen-profile',settings:'settings.ini',args.libmain:'libmain.so',
       build/'sdl/libSDL2.so':'libSDL2.so',build/'openxr/src/loader/libopenxr_loader.so':'libopenxr_loader.so',
       WORKSPACE/'data/imports/rom/AnimalCrossing.ciso':'rom/AnimalCrossing.ciso',
       ROOT/'pc/shaders/default.vert':'shaders/default.vert',ROOT/'pc/shaders/default.frag':'shaders/default.frag'}
save_hash=None
if args.save_copy:
    save_hash=hashlib.sha256(args.save_copy.read_bytes()).hexdigest()
    files[args.save_copy]='save/card_a/'+args.save_copy.name
if script_file:files[script_file]='input.txt'
manifest={'mode':'single flat offscreen title; no XR, Activity or physical input',
          'profiling':not args.no_profile,
          'full_world':not args.stock_world,
          'terrain_draw_radius':args.draw_radius,
          'warmup_world_frames':args.warmup_frames,
          'requested_sample_frames':args.sample_frames,
          'audio':'actual sample generator on native pthread, device playback discarded',
          'data':'private temporary ROM copy and initially empty save folders; no app data',
          'remote':REMOTE,'files':[]}
if args.save_copy:
    manifest['data']='private temporary ROM and GCI copies; original source and app data never written'
    manifest['source_save_sha256_before']=save_hash
if script:
    manifest['input_script']=script
    manifest['navigation_only']=True
if args.stereo:
    manifest['mode']='two fixed test eyes, production render helpers; no XR runtime or tracked input'
    manifest['eye_target']=[args.eye_size,args.eye_size]
    manifest['test_pose']={'fov_degrees':90,'ipd_mm':64,'pitch_degrees':-20,'yaw_degrees':args.yaw}
    manifest['pacing']='uncapped; no xrWaitFrame/compositor pacing'
if script:manifest['pacing']='navigation minimum60Hz pacing; not a performance benchmark'
for path,name in files.items():
    run(['push',str(path),REMOTE+'/'+name])
    manifest['files'].append({'name':name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
run(['shell',f'chmod 500 {REMOTE}/game-offscreen-profile && chmod 400 {REMOTE}/rom/AnimalCrossing.ciso'])
test_arguments=' --stock-world' if args.stock_world else ''
result=run(['shell',f'cd {REMOTE} && LD_LIBRARY_PATH={REMOTE} ACQUEST_TEST_WARMUP={args.warmup_frames} ACQUEST_TEST_SAMPLES={args.sample_frames} ACQUEST_TEST_YAW={args.yaw} ACQUEST_TEST_EYE_SIZE={args.eye_size} ACQUEST_TEST_PROFILE={int(not args.no_profile)} ./game-offscreen-profile{test_arguments}'],check=False)
(OUT/'results.log').write_text(result.stdout+result.stderr,encoding='utf-8')
manifest['exit_code']=result.returncode
if args.save_copy:
    manifest['source_save_sha256_after']=hashlib.sha256(args.save_copy.read_bytes()).hexdigest()
    manifest['source_save_unchanged']=manifest['source_save_sha256_after']==save_hash
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print((result.stdout+result.stderr).encode('ascii',errors='replace').decode('ascii'),end='')
if result.returncode==0:run(['pull',REMOTE+'/title.bmp',str(OUT/'title.bmp')])
if result.returncode==0 and args.stereo:
    for eye in ['left','right']:run(['pull',REMOTE+'/'+eye+'.bmp',str(OUT/(eye+'.bmp'))])
if script:
    for at in script.get('captures',[]):
        name=f'capture-{at:06d}.bmp'
        run(['pull',REMOTE+'/'+name,str(OUT/name)],check=False)
print('Receipt:',OUT)
raise SystemExit(result.returncode)
