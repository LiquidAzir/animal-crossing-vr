"""Run production VR menu navigation/settings code without a game or headset.

Only SDL display/bindings side effects are stubbed. Actual settings parsing and
saving run in an isolated build directory, never against a player's config.
"""
import os
from pathlib import Path
import subprocess
from run_vr_tool_tests import ROOT, function


def main():
    out = ROOT / 'pc/build32/vr-settings-menu-tests'
    out.mkdir(parents=True, exist_ok=True)
    settings = (ROOT / 'pc/src/pc_settings.c').read_text(encoding='utf-8')
    menu = (ROOT / 'pc/src/pc_settings_menu.c').read_text(encoding='utf-8')
    chunks = [settings[settings.index('PCSettings g_pc_settings ='):
                       settings.index('static const char* skip_ws')]]
    for name in ('skip_ws', 'trim_end', 'apply_setting', 'write_defaults',
                 'pc_settings_save', 'pc_settings_load'):
        code = function(settings, name)
        assert code, name
        chunks.append(code)
    chunks += [menu[menu.index('enum {'):menu.index('/* The game font atlas')],
               menu[menu.index('static PCSettings s_startup;'):
                    menu.index('/* --- Dirty + helpers --- */')]]
    for name in ('recompute_dirty', 'snapshot', 'item_cycle', 'item_format',
                 'item_changed', 'item_differs_from_startup',
                 'recompute_restart_needed', 'cur_tab', 'cur_item_count',
                 'idx_apply', 'idx_back', 'res_confirm_keep',
                 'res_confirm_revert', 'res_confirm_finish', 'apply_pending',
                 'pc_settings_menu_enter', 'pc_settings_menu_enter_vr',
                 'pc_settings_menu_active', 'pc_settings_menu_nav_up',
                 'pc_settings_menu_nav_down', 'nav_horizontal',
                 'pc_settings_menu_nav_left', 'pc_settings_menu_nav_right',
                 'pc_settings_menu_confirm', 'pc_settings_menu_cancel'):
        code = function(menu[menu.index('/* --- Dirty + helpers --- */'):], name)
        assert code, name
        chunks.append(code)
    (out / 'vr_settings_menu_source.inc').write_text(
        '\n\n'.join(chunks), encoding='utf-8', newline='\n')
    cc = Path('C:/msys64/mingw32/bin/gcc.exe')
    env = dict(os.environ, PATH=str(cc.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'vr-settings-menu.exe'
    subprocess.run([str(cc), '-std=gnu11', '-O2', '-Wall', '-Wextra',
                    '-Wno-unused-function', '-Ipc/include', '-I' + str(out),
                    'pc/tests/vr_settings_menu.c', '-o', str(exe)],
                   cwd=ROOT, env=env, check=True)
    result = subprocess.run([str(exe)], cwd=out, env=env,
                            capture_output=True, text=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr,
                                    encoding='utf-8', newline='\n')
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
