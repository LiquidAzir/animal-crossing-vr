"""Configure, build and package the standalone Animal Crossing Quest app.

Uses existing local tools and pinned dependencies; never downloads, installs,
deletes directories, changes remotes, or accesses the working PC installation.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

SOURCE = Path(__file__).resolve().parents[2]
WORKSPACE = SOURCE.parent
BUILD_ROOT = WORKSPACE/'build'
DEPENDENCIES = {'SDL': 'release-2.30.10', 'OpenXR-SDK': 'release-1.1.63'}


def git_output(directory, *arguments):
    return subprocess.run(['git', '-C', str(directory), *arguments], check=True,
                          capture_output=True, text=True).stdout.strip()


def checked_local_path(paths, name, expected=None):
    path = Path(paths[name]).resolve()
    if not path.exists():
        raise ValueError(f'Missing {name}: {path}')
    if expected is not None and path != expected.resolve():
        raise ValueError(f'{name} must use this Quest workspace: {expected}')
    return path


def check_existing_cache(build, abi, game):
    cache = build/'CMakeCache.txt'
    if not cache.exists():
        return
    values = {}
    for line in cache.read_text(errors='replace').splitlines():
        if '=' in line and ':' in line and not line.startswith(('#', '//')):
            key, value = line.split('=', 1)
            values[key.split(':', 1)[0]] = value
    if values.get('CMAKE_HOME_DIRECTORY') and Path(values['CMAKE_HOME_DIRECTORY']).resolve() != (SOURCE/'quest').resolve():
        raise ValueError('Existing CMake cache belongs to a different source tree; choose another build directory')
    if values.get('ANDROID_ABI', abi) != abi or values.get('QUEST_BUILD_GAME', game) != game:
        raise ValueError('Existing build has a different ABI/mode; choose another build directory')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['probe', 'game'], default='probe')
    parser.add_argument('--abi', choices=['armeabi-v7a', 'arm64-v8a'], default='armeabi-v7a')
    parser.add_argument('--build-dir', type=Path, help='Optional directory under ../build, resolved from the source root')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument('--version', type=int, default=1, help='APK version code')
    parser.add_argument('--configure-only', action='store_true')
    parser.add_argument('--no-package', action='store_true')
    parser.add_argument('--dry-run', action='store_true', help='Validate inputs and print commands without writing/building')
    args = parser.parse_args()
    if args.mode == 'game' and args.abi != 'armeabi-v7a':
        parser.error('The full game currently requires ARM32; ARM64 is supported only for the OpenXR probe')
    if args.jobs < 1 or args.version < 1:
        parser.error('--jobs and --version must be positive')
    build_root = BUILD_ROOT.resolve()
    if not build_root.is_relative_to(WORKSPACE):
        raise ValueError('Quest build root resolves outside this workspace')
    label = 'arm32' if args.abi == 'armeabi-v7a' else 'arm64'
    build = (SOURCE/args.build_dir).resolve() if args.build_dir else build_root/(args.mode+'-'+label)
    if build == build_root or not build.is_relative_to(build_root):
        raise ValueError('Build directory must be a child of the isolated Quest workspace build directory')
    game = 'ON' if args.mode == 'game' else 'OFF'
    check_existing_cache(build, args.abi, game)

    paths_file = WORKSPACE/'toolchain/paths.json'
    paths = json.loads(paths_file.read_text())
    cmake = checked_local_path(paths, 'cmake')
    ninja = checked_local_path(paths, 'ninja')
    if not ninja.is_relative_to(WORKSPACE/'toolchain'):
        raise ValueError('Ninja must be inside the isolated Quest toolchain')
    sdk = checked_local_path(paths, 'sdk_root', WORKSPACE/'toolchain/sdk')
    jdk = checked_local_path(paths, 'java_home', WORKSPACE/'toolchain/jdk-21')
    ndk = checked_local_path(paths, 'ndk_root', sdk/'ndk/27.3.13750724')
    toolchain = checked_local_path(paths, 'ndk_cmake_toolchain', ndk/'build/cmake/android.toolchain.cmake')
    checked_local_path(paths, 'android_jar', sdk/'platforms/android-34/android.jar')
    checked_local_path(paths, 'build_tools', sdk/'build-tools/35.0.0')

    deps = {}
    for name, tag in DEPENDENCIES.items():
        directory = (WORKSPACE/'third_party'/name).resolve()
        if not directory.is_relative_to(WORKSPACE/'third_party'):
            raise ValueError(f'{name} resolves outside the isolated dependencies')
        head = git_output(directory, 'rev-parse', 'HEAD')
        if head != git_output(directory, 'rev-parse', tag+'^{commit}'):
            raise ValueError(f'{name} must be checked out at {tag}')
        if git_output(directory, 'status', '--porcelain', '--untracked-files=no'):
            raise ValueError(f'{name} has modified tracked files; restore the pinned dependency or review a new pin')
        deps[name] = directory
        print(f'{name}: {tag} ({head})', flush=True)

    env = dict(os.environ)
    env.update(JAVA_HOME=str(jdk), ANDROID_HOME=str(sdk), ANDROID_SDK_ROOT=str(sdk),
               ANDROID_NDK_HOME=str(ndk), ANDROID_NDK_ROOT=str(ndk), NDK_HOME=str(ndk),
               ANDROID_USER_HOME=str(WORKSPACE/'toolchain/android-user-home'),
               GRADLE_USER_HOME=str(WORKSPACE/'toolchain/gradle-user-home'),
               TEMP=str(WORKSPACE/'toolchain/temp'), TMP=str(WORKSPACE/'toolchain/temp'))
    env['PATH'] = os.pathsep.join([str(jdk/'bin'), str(ninja.parent), str(cmake.parent), env.get('PATH', '')])
    commands = [[cmake, '-S', SOURCE/'quest', '-B', build, '-G', 'Ninja',
                 '-DCMAKE_MAKE_PROGRAM='+str(ninja), '-DCMAKE_TOOLCHAIN_FILE='+str(toolchain),
                 '-DANDROID_ABI='+args.abi, '-DANDROID_PLATFORM=android-29',
                 '-DANDROID_STL=c++_static', '-DCMAKE_BUILD_TYPE=Release',
                 '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', '-DQUEST_BUILD_GAME='+game,
                 '-DQUEST_SDL_ROOT='+str(deps['SDL']), '-DQUEST_OPENXR_ROOT='+str(deps['OpenXR-SDK'])]]
    if not args.configure_only:
        commands.append([cmake, '--build', build, '--parallel', args.jobs])
        if not args.no_package:
            package = [sys.executable, SOURCE/'quest/tools/package_apk.py', '--build-dir', build,
                       '--abi', args.abi, '--version', args.version]
            if args.mode == 'probe':
                package.append('--probe')
            commands.append(package)
    if not args.dry_run:
        for name in ['ANDROID_USER_HOME', 'GRADLE_USER_HOME', 'TEMP']:
            Path(env[name]).mkdir(parents=True, exist_ok=True)
    for command in commands:
        command = [str(part) for part in command]
        # Display quoting is informational. Execution uses an argument list,
        # never a shell command, so spaces in workspace/tool paths are safe.
        print(subprocess.list2cmdline(command), flush=True)
        if not args.dry_run:
            subprocess.run(command, env=env, check=True, cwd=SOURCE)


if __name__ == '__main__':
    main()
