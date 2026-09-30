"""Actual ARM32 climate/controller draw regression, private shell-only fixture.

Coordinate the GPU slot first. No APK install, app launch, or personal saves.
The synthetic trigger tests the boat's climate/background lifecycle in a town;
it is not an end-to-end boat navigation or an island scene-load test.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT.parent
BUILD = WORK / 'build/game-arm32'
PATHS = json.loads((WORK / 'toolchain/paths.json').read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--libmain', type=Path, default=BUILD / 'game/libmain.so')
    parser.add_argument('--expect-crash', action='store_true')
    parser.add_argument('--compile-only', action='store_true')
    args = parser.parse_args()
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out = WORK / 'research' / ('island-transition-offscreen-' + stamp)
    out.mkdir(parents=True)
    remote = '/data/local/tmp/acquest-island-transition/' + stamp
    exe = out / 'island-transition'
    command = [PATHS['clang_armv7_api24'], '-std=c11', '-O2', '-fPIE', '-pie',
               '-DTARGET_PC', '-D_LANGUAGE_C', '-DF3DEX_GBI_2', '-DVERSION=0',
               '-Wl,--export-dynamic', '-I' + str(WORK / 'third_party/SDL/include'),
               '-I' + str(ROOT / 'include'), '-I' + str(ROOT / 'quest/tests'),
               '-I' + str(ROOT / 'pc/include'),
               str(ROOT / 'quest/tests/island_transition_offscreen.c'),
               '-L' + str(BUILD / 'sdl'), '-lSDL2', '-lGLESv3', '-lEGL', '-ldl', '-o', str(exe)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (out / 'compile.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr)
        return compiled.returncode
    if args.compile_only:
        print('Compiled:', exe)
        return 0

    def adb(*parts, check=True, timeout=60):
        result = subprocess.run([PATHS['adb'], '-s', args.serial, *parts],
                                capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=timeout)
        if check:
            result.check_returncode()
        return result

    live = adb('shell', 'pidof com.liquidazir.animalcrossingquest', check=False)
    activity = adb('shell', 'dumpsys activity activities').stdout
    resumed = [line.strip() for line in activity.splitlines()
               if ('mResumedActivity' in line or 'topResumedActivity' in line)
               and 'null' not in line]
    (out / 'device-preflight.txt').write_text(live.stdout + '\n' + '\n'.join(resumed), encoding='utf-8')
    if live.stdout.strip() or any('HomeActivity' not in line for line in resumed):
        raise SystemExit('A game or non-Home activity is active; GPU test skipped.')

    settings = out / 'settings.ini'
    settings.write_text('[Graphics]\nwindow_width=640\nwindow_height=480\nfullscreen=0\n'
                        'vsync=0\nmax_fps=60\nmsaa=0\n[Enhancements]\npreload_textures=0\n'
                        '[FirstPerson]\nfp_mode=0\nvr_draw_radius=0\n'
                        '[VR]\nvr_mode=0\nvr_town_residency=1\n')
    files = {exe: 'island-transition', settings: 'settings.ini', args.libmain: 'libmain.so',
             BUILD / 'sdl/libSDL2.so': 'libSDL2.so',
             BUILD / 'openxr/src/loader/libopenxr_loader.so': 'libopenxr_loader.so',
             WORK / 'data/imports/rom/AnimalCrossing.ciso': 'rom/AnimalCrossing.ciso',
             ROOT / 'pc/shaders/default.vert': 'shaders/default.vert',
             ROOT / 'pc/shaders/default.frag': 'shaders/default.frag'}
    manifest = {'serial': args.serial, 'remote': remote, 'mode': __doc__, 'files': [],
                'personal_data': 'no source GCI or app data; fresh empty save directories',
                'device_preflight_resumed': resumed}
    adb('shell', f'mkdir -p {remote}/rom {remote}/shaders {remote}/save/card_a {remote}/save/card_b')
    for path, name in files.items():
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        adb('push', str(path), remote + '/' + name)
        manifest['files'].append({'name': name, 'sha256': digest})
    adb('shell', f'chmod 500 {remote}/island-transition && chmod 400 {remote}/rom/AnimalCrossing.ciso')
    result = adb('shell', f'cd {remote} && LD_LIBRARY_PATH={remote} ACQUEST_TEST_PROFILE=0 '
                 'ACQUEST_TEST_SAMPLES=1800 ./island-transition', check=False)
    # adb output includes translated game strings; Windows' locale encoding
    # cannot preserve those. Save UTF-8 before collecting the native report.
    (out / 'results.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    for name in ('island-transition-results.txt', 'town-before.bmp',
                 'island-climate-restored.bmp', 'town-return-restored.bmp'):
        adb('pull', remote + '/' + name, str(out / name), check=False)
    report = out / 'island-transition-results.txt'
    report_text = report.read_text(encoding='utf-8') if report.exists() else ''
    manifest['exit_code'] = result.returncode
    good = result.returncode == 0 and 'PASS ' in report_text
    if args.expect_crash:
        good = (result.returncode == 139 and 'GAP_BEFORE_DRAW' in report_text
                and 'GAP_AFTER_DRAW' not in report_text)
        manifest['expected_crash_confirmed'] = good
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(report_text)
    print('Actual exit:', result.returncode, 'expected result:', good)
    print('Receipt:', out)
    return 0 if good else 10


if __name__ == '__main__':
    raise SystemExit(main())
