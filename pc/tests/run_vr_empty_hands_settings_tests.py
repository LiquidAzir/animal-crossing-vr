"""Exercise actual settings/menu code and preserve existing SteamVR mappings.

No game, headset, ROM, or real settings file is needed. The native test runs in
an isolated build directory. Runtime SDL/game side effects are stubbed; actual
defaults, parsing, saving, menu dispatch, dirty tracking and Apply are executed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import ROOT, function


def action_checks():
    # Semantic hashes captured before adding empty-hand actions. Removing only
    # the two additions must reproduce every old action, source, haptic, label,
    # and tool-tip pose. Intentional future control changes should update these.
    old_hashes = {
        'actionmanifest.json': '561c2ea54c06d65327ebd589e8144a0c05e0bcf1967a3dcf8d7ce4083fdf4cf0',
        'bindings_knuckles.json': '28e0a3482737b262c4a7d4fd9373b097b1c072a8cd59d839f365728538b92c94',
        'bindings_oculus_touch.json': '9e850ff31a7ae3a14b02e4ff05d15dec800032f61410c54986913406db3dea66',
    }
    added = {'/actions/main/in/empty_hand_' + side: side for side in ('left', 'right')}
    checks = 0
    for name, old_hash in old_hashes.items():
        data = json.loads((ROOT / 'pc/vr_actions' / name).read_text())
        if name == 'actionmanifest.json':
            for action in added:
                entries = [a for a in data['actions'] if a['name'] == action]
                assert entries == [{'name': action, 'type': 'pose', 'requirement': 'suggested'}]
                checks += 1
                assert all(loc.get(action, '').strip() for loc in data['localization'])
                checks += 1
            data['actions'] = [a for a in data['actions'] if a['name'] not in added]
            for loc in data['localization']:
                for action in added:
                    del loc[action]
        else:
            main = data['bindings']['/actions/main']
            for action, side in added.items():
                entries = [p for p in main['poses'] if p['output'] == action]
                assert entries == [{'output': action, 'path': f'/user/hand/{side}/pose/handgrip'}]
                checks += 1
            main['poses'] = [p for p in main['poses'] if p['output'] not in added]
        canonical = json.dumps(data, sort_keys=True, separators=(',', ':')).encode()
        assert hashlib.sha256(canonical).hexdigest() == old_hash, f'Existing controls changed: {name}'
        checks += 1
    print(f'VR empty-hand action bindings: {checks} checks passed; existing mappings unchanged')


def controller_profile_checks(runtime):
    """Validate pose sources against the driver's real supported endpoints.

    SteamVR accepts the JSON even when a pose source is absent from its driver
    profile, so syntax and expected-string tests alone cannot catch that error.
    --steamvr-root allows a runtime/profile checkout on any development host.
    """
    if runtime is None:
        local = os.environ.get('LOCALAPPDATA')
        paths_file = Path(local) / 'openvr/openvrpaths.vrpath' if local else None
        if paths_file and paths_file.is_file():
            paths = json.loads(paths_file.read_text()).get('runtime', [])
            runtime = next((Path(p) for p in paths if (Path(p) / 'drivers').is_dir()), None)
    if runtime is None:
        print('SteamVR controller profiles unavailable; use --steamvr-root to validate real pose sources')
        return
    profiles = {
        'bindings_oculus_touch.json': 'oculus/resources/input/touch_profile.json',
        'bindings_knuckles.json': 'indexcontroller/resources/input/index_controller_profile.json',
    }
    checks = 0
    for binding, relative in profiles.items():
        profile_path = runtime / 'drivers' / relative
        profile = json.loads(profile_path.read_text())
        data = json.loads((ROOT / 'pc/vr_actions' / binding).read_text())
        assert profile['controller_type'] == data['controller_type'], profile_path
        checks += 1
        for entry in data['bindings']['/actions/main']['poses']:
            path = entry['path']
            source = next((path[len(hand):] for hand in ('/user/hand/left', '/user/hand/right')
                           if path.startswith(hand + '/')), None)
            assert source and profile['input_source'].get(source, {}).get('type') == 'pose', (
                f"Unsupported pose source {path} for {entry['output']} in {profile_path}")
            checks += 1
    print(f'SteamVR installed controller profiles: {checks} checks passed; all pose sources supported')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--steamvr-root', type=Path,
                        help='SteamVR runtime root (auto-discovered from openvrpaths.vrpath when installed)')
    args = parser.parse_args()
    action_checks()
    controller_profile_checks(args.steamvr_root)
    out = ROOT / 'pc/build32/vr-empty-hands-settings-tests'
    out.mkdir(parents=True, exist_ok=True)
    settings = (ROOT / 'pc/src/pc_settings.c').read_text()
    menu = (ROOT / 'pc/src/pc_settings_menu.c').read_text()
    chunks = [settings[settings.index('PCSettings g_pc_settings ='):settings.index('static const char* skip_ws')]]
    settings_functions = ('skip_ws', 'trim_end', 'apply_setting', 'write_defaults',
                          'pc_settings_save', 'pc_settings_load')
    for name in settings_functions:
        code = function(settings, name)
        assert code, name
        chunks.append(code)
    chunks += [menu[menu.index('enum {'):menu.index('/* --- Bindings editor')],
               menu[menu.index('static PCSettings s_startup;'):menu.index('/* --- Dirty + helpers --- */')]]
    for name in ('recompute_dirty', 'snapshot', 'item_cycle', 'item_format', 'item_changed',
                 'item_differs_from_startup', 'recompute_restart_needed', 'apply_pending'):
        code = function(menu[menu.index('/* --- Dirty + helpers --- */'):], name)
        assert code, name
        chunks.append(code)
    (out / 'vr_empty_hands_settings_source.inc').write_text('\n\n'.join(chunks))
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'vr-empty-hands-settings.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Wno-unused-variable',
                    '-Ipc/include', '-I' + str(out), 'pc/tests/vr_empty_hands_settings.c',
                    '-o', str(exe)], cwd=ROOT, env=env, check=True)
    # Always start without the generated config, never touch a real game config.
    config = out / 'settings.ini'
    if config.exists():
        config.unlink()
    result = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
