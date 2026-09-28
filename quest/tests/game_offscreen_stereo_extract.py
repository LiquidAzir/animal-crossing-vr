"""Extract production Quest renderer helpers for the fixed-pose shell host."""
import re
from pathlib import Path


def function(source: str, name: str) -> str:
    start = re.search(r'^.*\b' + re.escape(name) + r'\([^;]*?\)\s*\{', source, re.M)
    if not start:
        raise ValueError(f'Production helper not found: {name}')
    opening = source.index('{', start.start())
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start.start():end] + '\n'


def generate(root: Path, output: Path) -> None:
    common = (root / 'quest/src/quest_vr.cpp').read_text()
    xr = (root / 'quest/src/quest_vr_openxr.c_inc').read_text()
    state_start = common.index('static struct {')
    state_end = common.index('} s_vr;', state_start) + len('} s_vr;')
    chunks = ['/* Generated from current production source; do not edit. */\n',
              common[state_start:state_end] + '\n']
    for name in ['m34_identity', 'm34_mul', 'm34_invert_rs', 'm34_invert_rigid',
                 'pcvr_far_m', 'pcvr_create_target', 'pcvr_destroy_target']:
        chunks.append(function(common, name))
    chunks.append(common[common.index('static const char* s_panel_vs ='):
                         common.index('static GLuint pcvr_compile(')])
    for name in ['pcvr_compile', 'pcvr_create_panel_gl', 'pcvr_update_view_correction',
                 'pc_vr_set_fp_scale', 'pc_vr_notify_game_view', 'pc_vr_begin_eye',
                 'pc_vr_current_eye', 'pc_vr_in_scene_pass', 'pc_vr_eye_projection',
                 'pc_vr_view_correction', 'pc_vr_set_scene_depth_range',
                 'pc_vr_bind_ui_target', 'pc_vr_skip_ui_draws', 'pc_vr_world_scale',
                 'pc_vr_set_flat_scene', 'pc_vr_flat_scene_active',
                 'pc_vr_end_scene_passes', 'pcvr_draw_panel', 'pc_vr_ensure_submitted']:
        body = function(common, name)
        if name == 'pc_vr_flat_scene_active':
            body = body.replace('extern int g_pc_paused;', '')
            body = body.replace('g_pc_paused', 'offscreen_paused()')
        chunks.append(body)
    chunks.append(function(xr, 'pcvr_update_eye_projection'))
    output.write_text('\n'.join(chunks))
