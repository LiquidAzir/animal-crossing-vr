"""Relink existing PC objects with test wrappers and capture a hidden menu.

No game build, production binary replacement, headset access, or user saves.
All runtime files and settings live in a fresh build32 fixture.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'pc/build32'
OUT = BUILD / ('vr-menu-native-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
FIXTURE = OUT / 'fixture'
CC = Path('C:/msys64/mingw32/bin')


def main():
    FIXTURE.mkdir(parents=True)
    env = dict(os.environ, PATH=str(CC) + os.pathsep + os.environ['PATH'], SDL_AUDIODRIVER='dummy')
    obj = OUT / 'capture.o'
    exe = FIXTURE / 'AnimalMenuCapture.exe'
    archive = BUILD / 'CMakeFiles/ac_pc.dir/objects.a'
    original_exe = BUILD / 'bin/AnimalCrossing.exe'
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = {'production_archive_sha256': sha(archive),
                'production_exe_sha256_before': sha(original_exe),
                'mode': 'actual native menu/font/GX; hidden SDL window, no VR runtime',
                'data': 'private ROM copy, new empty saves, private settings; no user saves',
                'synthetic': 'public menu API from swap wrapper, test-only title guard bypass'}
    compile_command = [str(CC / 'gcc.exe'), '-std=gnu11', '-O2', '-DTARGET_PC',
                       '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
                       '@CMakeFiles/ac_pc.dir/includes_C.rsp',
                       '-c', str(ROOT / 'pc/tests/vr_menu_native_capture.c'), '-o', str(obj)]
    link_command = [str(CC / 'g++.exe'), '-static-libgcc', '-static-libstdc++',
                    '-Wl,-Bstatic,--whole-archive', str(BUILD / 'libwinpthread_stripped.a'),
                    '-Wl,-Bdynamic,--no-whole-archive', '-Wl,--large-address-aware', '-mwindows',
                    '-Wl,--whole-archive', str(archive), '-Wl,--no-whole-archive', str(obj),
                    '-Wl,--wrap=pc_platform_swap_buffers,--wrap=SDL_CreateWindow',
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
    report = FIXTURE / 'native-menu-results.txt'
    if report.exists(): print(report.read_text())
    print('Receipt:', OUT)
    return code or (0 if manifest['production_exe_unchanged'] else 10)


if __name__ == '__main__':
    raise SystemExit(main())
