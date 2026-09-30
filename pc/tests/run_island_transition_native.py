"""Run actual background-controller transition renders in a hidden fixture.

No game build, production binary replacement, headset access, or user saves.
All runtime files and settings live in a fresh build32 fixture.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'pc/build32'
OUT = BUILD / ('island-transition-native-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
FIXTURE = OUT / 'fixture'
CC = Path('C:/msys64/mingw32/bin')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive',type=Path,default=BUILD/'CMakeFiles/ac_pc.dir/objects.a')
    parser.add_argument('--expect-crash',action='store_true')
    args=parser.parse_args()
    FIXTURE.mkdir(parents=True)
    env = dict(os.environ, PATH=str(CC) + os.pathsep + os.environ['PATH'], SDL_AUDIODRIVER='dummy')
    obj = OUT / 'capture.o'
    exe = FIXTURE / 'AnimalIslandTransition.exe'
    archive = args.archive.resolve()
    original_exe = BUILD / 'bin/AnimalCrossing.exe'
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = {'production_archive_sha256': sha(archive),
                'production_exe_sha256_before': sha(original_exe),
                'mode': 'actual climate/controller lifecycle/house/GX; hidden SDL window, no VR runtime',
                'data': 'private ROM copy, new empty saves, private settings; no user saves',
                'synthetic': 'boat climate/background sequence triggered in title town with full-world houses; no boat navigation or island scene change'}
    compile_command = [str(CC / 'gcc.exe'), '-std=gnu11', '-O2', '-DTARGET_PC',
                       '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                       '@CMakeFiles/ac_pc.dir/includes_C.rsp',
                       '-c', str(ROOT / 'pc/tests/island_transition_native.c'), '-o', str(obj)]
    link_command = [str(CC / 'g++.exe'), '-static-libgcc', '-static-libstdc++',
                    '-Wl,-Bstatic,--whole-archive', str(BUILD / 'libwinpthread_stripped.a'),
                    '-Wl,-Bdynamic,--no-whole-archive', '-Wl,--large-address-aware', '-mwindows',
                    '-Wl,--whole-archive', str(archive), '-Wl,--no-whole-archive', str(obj),
                    '-Wl,--wrap=pc_platform_swap_buffers,--wrap=SDL_CreateWindow,--wrap=Actor_info_draw_actor',
                    '-o', str(exe), '@CMakeFiles/ac_pc.dir/linkLibs.rsp']
    with (OUT / 'build.log').open('w') as log:
        for command in (compile_command, link_command):
            result = subprocess.run(command, cwd=BUILD, env=env, text=True, stdout=log, stderr=log)
            if result.returncode:
                print((OUT / 'build.log').read_text()); return result.returncode
    base = BUILD / 'smoke-fp-dialogue'
    for name in ('SDL2.dll', 'openvr_api.dll'):
        shutil.copy2(base / name, FIXTURE / name)
    shutil.copytree(ROOT / 'pc/shaders', FIXTURE / 'shaders')
    (FIXTURE / 'rom').mkdir()
    rom = next((base / 'rom').glob('*.ciso'))
    shutil.copy2(rom, FIXTURE / 'rom' / rom.name)
    manifest['rom_copy_sha256'] = sha(FIXTURE / 'rom' / rom.name)
    for slot in ('card_a', 'card_b'):(FIXTURE / 'save' / slot).mkdir(parents=True)
    (FIXTURE / 'settings.ini').write_text(
        '[Graphics]\nwindow_width=640\nwindow_height=480\nfullscreen=0\nvsync=0\nmax_fps=60\nmsaa=0\n'
        '[Enhancements]\npreload_textures=0\n[Audio]\nmaster_volume=70\n'
        '[VR]\nvr_mode=0\nvr_empty_hands=0\nvr_motion_swing=1\n'
        '[FirstPerson]\nfp_mode=0\nfp_snap_degrees=0\n')
    with (OUT / 'stdout.log').open('w') as stdout, (OUT / 'stderr.log').open('w') as stderr:
        try:
            result = subprocess.run([str(exe), '--no-vr', '--verbose'], cwd=FIXTURE, env=env,
                                    stdout=stdout, stderr=stderr, timeout=35,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
    manifest['exit_code'] = code
    manifest['production_exe_sha256_after'] = sha(original_exe)
    manifest['production_exe_unchanged'] = manifest['production_exe_sha256_before'] == manifest['production_exe_sha256_after']
    manifest['diagnostic_exe_sha256'] = sha(exe)
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    report = FIXTURE / 'island-transition-results.txt'
    if report.exists(): print(report.read_text())
    if args.expect_crash:
        crash=FIXTURE/'crash.txt'
        text=report.read_text() if report.exists() else ''
        confirmed=code not in (0,124) and 'GAP_BEFORE_DRAW' in text and 'GAP_AFTER_DRAW' not in text and crash.exists()
        manifest['expected_crash_confirmed']=confirmed
        if crash.exists():print(crash.read_text())
        (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        print('Receipt:',OUT)
        return 0 if confirmed and manifest['production_exe_unchanged'] else 11
    print('Receipt:', OUT)
    return code or (0 if manifest['production_exe_unchanged'] else 10)


if __name__ == '__main__':
    raise SystemExit(main())
