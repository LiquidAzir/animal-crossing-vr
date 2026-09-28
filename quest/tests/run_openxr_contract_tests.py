"""Exercise the actual Quest math/frame functions with instrumented XR calls.

No Android runtime or ROM required. The separate headset probe verifies the
real loader/session path; this harness verifies deterministic edge cases.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'pc/tests'))
from run_vr_tool_tests import function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', default='C:/msys64/mingw32/bin/g++.exe')
    args = parser.parse_args()
    sdk = ROOT.parent / 'third_party/OpenXR-SDK'
    out = ROOT.parent / 'build/openxr-contract-tests'
    out.mkdir(parents=True, exist_ok=True)
    chunks = []
    for path, names in [
        ('quest/src/quest_xr_runtime.cpp', ['QuestXrRuntime::timing_record',
         'QuestXrRuntime::timing_finish', 'QuestXrRuntime::check',
         'QuestXrRuntime::poll_events', 'QuestXrRuntime::begin_frame',
         'QuestXrRuntime::acquire_eye', 'QuestXrRuntime::end_frame',
         'QuestXrRuntime::locate_head']),
        ('quest/src/quest_vr.cpp', ['m34_identity', 'm34_mul',
         'm34_invert_rigid', 'pcvr_far_m']),
        ('quest/src/quest_vr_openxr.c_inc', ['pcvr_from_pose', 'pcvr_recenter',
         'pcvr_update_eye_projection', 'pcvr_float_action', 'pcvr_digital',
         'pcvr_analog', 'quest_vr_set_resumed']),
    ]:
        source = (ROOT / path).read_text()
        for name in names:
            body = function(source, name)
            if body is None and '::' in name:
                adapted = source.replace(') const {', ')       {')
                body = function(adapted, name)
                if body is not None:
                    start = adapted.index(body)
                    body = source[start:start + len(body)]
            if body is None:
                raise RuntimeError(f'Missing production function {name}')
            chunks.append(body)
    header = (ROOT / 'quest/include/quest_xr_runtime.h').read_text()
    # Keep the actual runtime data/class contract, replacing only platform
    # includes with lightweight host declarations in the test translation unit.
    (out / 'runtime_class.inc').write_text(header[header.index('void quest_xr_log'):])
    (out / 'contract_source.inc').write_text('\n\n'.join(chunks))
    source = (ROOT / 'quest/src/quest_vr_openxr.c_inc').read_text()
    bindings = re.findall(r'\{s_vr\.(act_\w+(?:\[\d\])?), "(/user/hand/[^"]+)"\}', source)
    actions = dict(re.findall(r'\{&s_vr\.(act_\w+(?:\[\d\])?), (XR_ACTION_TYPE_\w+)', source))
    profile = ET.parse(sdk / 'specification/registry/xr.xml').find(
        ".//interaction_profile[@name='/interaction_profiles/oculus/touch_controller']")
    supported = {}
    for component in profile.findall('component'):
        for hand in (component.get('user_path'),) if component.get('user_path') else (
                '/user/hand/left', '/user/hand/right'):
            supported[hand + component.get('subpath')] = component.get('type')
    assert len(bindings) == 17, len(bindings)
    for action, path in bindings:
        assert supported.get(path) == actions[action], (action, path, supported.get(path))
    print(f'{len(bindings)} action bindings match the pinned Khronos Touch profile and action types')
    cxx = Path(args.cxx).resolve()
    env = dict(os.environ, PATH=str(cxx.parent) + os.pathsep + os.environ['PATH'])
    exe = out / 'contract.exe'
    subprocess.run([str(cxx), '-std=c++17', '-O2', '-Wall', '-Wextra',
                    '-Wno-missing-field-initializers', '-Wno-unused-parameter',
                    '-I' + str(sdk / 'include'), '-I' + str(out), '-I' + str(ROOT / 'pc/include'),
                    '-I' + str(ROOT / 'quest/include'),
                    str(ROOT / 'quest/tests/openxr_contract.cpp'), '-o', str(exe)],
                   env=env, check=True)
    result = subprocess.run([str(exe)], env=env, text=True, capture_output=True)
    (out / 'results.txt').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
