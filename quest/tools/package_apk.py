"""Package this isolated Quest build with official SDK tools and a local debug key.

No ROM/save is packaged. The APK contains only libraries, Java host, shaders and
Quest defaults. This never reads or changes the PC installation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile

SOURCE = Path(__file__).resolve().parents[2]
WORKSPACE = SOURCE.parent
SDK = WORKSPACE / 'toolchain/sdk'
JDK = WORKSPACE / 'toolchain/jdk-21'
# D8 from 34.0.0 crashes on javac 21 anonymous-class metadata even when
# --release 8 is used. 35.0.0 converts the same SDL classes successfully.
TOOLS = SDK / 'build-tools/35.0.0'
ANDROID = SDK / 'platforms/android-34/android.jar'
SDL = WORKSPACE / 'third_party/SDL'


def verify_badging(text, package, version_code, abi, label):
    identity = re.search(r"^package: name='([^']+)' versionCode='([^']+)'", text, re.MULTILINE)
    native = re.search(r'^native-code:(.*)$', text, re.MULTILINE)
    application = re.search(r"^application: label='([^']*)'", text, re.MULTILINE)
    if not identity or identity.groups() != (package, str(version_code)):
        raise ValueError('Signed APK package/version does not match the requested build')
    if not native or re.findall(r"'([^']+)'", native.group(1)) != [abi]:
        raise ValueError('Signed APK native ABI does not match the requested build')
    if not application or application.group(1) != label:
        raise ValueError('Signed APK application label does not match the requested build')
    return {'package':identity.group(1), 'version_code':int(identity.group(2)),
            'abi':abi, 'label':application.group(1)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--abi', choices=['armeabi-v7a', 'arm64-v8a'], required=True)
    parser.add_argument('--probe', action='store_true')
    parser.add_argument('--version', type=int, default=1)
    args = parser.parse_args()
    build = args.build_dir.resolve()
    if not build.is_relative_to(WORKSPACE):
        raise ValueError('Build must be inside the isolated Quest workspace')
    if not build.is_dir() or args.version < 1:
        raise ValueError('Build directory must exist and version must be positive')
    for path in [JDK/'bin/java.exe', JDK/'bin/javac.exe', ANDROID,
                 TOOLS/'aapt2.exe', TOOLS/'zipalign.exe',
                 TOOLS/'lib/d8.jar', TOOLS/'lib/apksigner.jar']:
        if not path.is_file():
            raise FileNotFoundError(f'Missing isolated packaging tool: {path}')
    libraries = []
    elf_class, elf_machine = {'armeabi-v7a': (1, 40), 'arm64-v8a': (2, 183)}[args.abi]
    for name in ['libmain.so', 'libSDL2.so', 'libopenxr_loader.so']:
        matches = list(build.rglob(name))
        if len(matches) != 1:
            raise ValueError(f'Expected exactly one {name}, found {matches}')
        with matches[0].open('rb') as library:
            header = library.read(20)
        if (len(header) != 20 or header[:4] != b'\x7fELF' or
                header[4] != elf_class or header[5] != 1 or
                int.from_bytes(header[18:20], 'little') != elf_machine):
            raise ValueError(f'{matches[0]} does not match ABI {args.abi}')
        libraries.append(matches[0])
    output = build / 'apk'
    output.mkdir(exist_ok=True)
    # Never reuse old classes/dex/assets after a failed or changed build.
    # Keep staging for diagnosis; no recursive cleanup or user-data paths.
    stage = Path(tempfile.mkdtemp(prefix='stage-', dir=output))
    for sub in ['classes', 'dex', 'java-gen', 'assets/shaders']:
        (stage / sub).mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, JAVA_HOME=str(JDK), ANDROID_HOME=str(SDK))
    env['PATH'] = str(JDK/'bin') + os.pathsep + env['PATH']
    env['ANDROID_SDK_ROOT'] = str(SDK)
    env['ANDROID_USER_HOME'] = str(WORKSPACE/'toolchain/android-user-home')
    env['TEMP'] = env['TMP'] = str(WORKSPACE/'toolchain/temp')
    Path(env['ANDROID_USER_HOME']).mkdir(parents=True, exist_ok=True)
    Path(env['TEMP']).mkdir(parents=True, exist_ok=True)

    def run(command):
        subprocess.run([str(part) for part in command], env=env, check=True, cwd=SOURCE)

    def version(command):
        return subprocess.run([str(part) for part in command], env=env, check=True,
                              cwd=SOURCE, capture_output=True, text=True).stdout.strip()

    package = 'com.liquidazir.animalcrossingquest' + ('.probe' if args.probe else '')
    manifest = SOURCE/'quest/android/AndroidManifest.xml'
    android_namespace = 'http://schemas.android.com/apk/res/android'
    ET.register_namespace('android', android_namespace)
    manifest_tree = ET.parse(manifest)
    application = manifest_tree.getroot().find('application')
    if application is None:
        raise ValueError('Manifest has no application element')
    label = application.attrib['{'+android_namespace+'}label']
    if args.probe:
        label = 'Animal Crossing Quest XR Test'
        application.set('{'+android_namespace+'}label', label)
        manifest = stage/'AndroidManifest.xml'
        manifest_tree.write(manifest, encoding='utf-8', xml_declaration=True)
    for shader in ['default.vert', 'default.frag']:
        shutil.copy2(SOURCE/'pc/shaders'/shader, stage/'assets/shaders'/shader)
    shutil.copy2(SOURCE/'quest/android/assets/quest-settings.ini', stage/'assets/quest-settings.ini')
    unsigned = stage/'unsigned.apk'
    run([TOOLS/'aapt2.exe', 'link', '-o', unsigned, '-I', ANDROID,
         '--manifest', manifest,
         '--rename-manifest-package', package, '--version-code', args.version, '--replace-version',
         '-A', stage/'assets', '--java', stage/'java-gen'])
    java_sources = sorted((SDL/'android-project/app/src/main/java').rglob('*.java'))
    java_sources += sorted((SOURCE/'quest/android/java').rglob('*.java'))
    java_sources += sorted((stage/'java-gen').rglob('*.java'))
    arguments = stage/'javac.args'
    arguments.write_text('\n'.join('"'+str(p).replace('\\', '/')+'"' for p in java_sources), encoding='utf-8')
    run([JDK/'bin/javac.exe', '--release', '8', '-encoding', 'UTF-8',
         '-classpath', ANDROID, '-d', stage/'classes', '@'+str(arguments)])
    classes_jar = stage/'classes.jar'
    with zipfile.ZipFile(classes_jar, 'w') as archive:
        for path in sorted((stage/'classes').rglob('*.class')):
            archive.write(path, path.relative_to(stage/'classes').as_posix())
    run([JDK/'bin/java.exe', '-cp', TOOLS/'lib/d8.jar', 'com.android.tools.r8.D8',
         '--lib', ANDROID, '--min-api', '29', '--output', stage/'dex', classes_jar])
    with zipfile.ZipFile(unsigned, 'a', compression=zipfile.ZIP_DEFLATED) as archive:
        for path in libraries:
            archive.write(path, f'lib/{args.abi}/{path.name}')
        for path in sorted((stage/'dex').glob('*.dex')):
            archive.write(path, path.name)
    aligned = stage/'aligned.apk'
    run([TOOLS/'zipalign.exe', '-f', '-p', '4', unsigned, aligned])
    keys = SOURCE/'quest/.local'
    keys.mkdir(parents=True, exist_ok=True)
    key = keys/'debug.keystore'
    if not key.exists():
        run([JDK/'bin/keytool.exe', '-genkeypair', '-keystore', key, '-storepass', 'android',
             '-alias', 'androiddebugkey', '-keypass', 'android', '-keyalg', 'RSA',
             '-keysize', '2048', '-validity', '10000', '-dname', 'CN=Quest Development,O=Development,C=US'])
    apk = output/f'AnimalCrossingQuest-{("probe-" if args.probe else "")}{args.abi}.apk'
    signed = stage/'signed.apk'
    run([JDK/'bin/java.exe', '-jar', TOOLS/'lib/apksigner.jar', 'sign', '--ks', key,
         '--ks-pass', 'pass:android', '--key-pass', 'pass:android', '--out', signed, aligned])
    run([JDK/'bin/java.exe', '-jar', TOOLS/'lib/apksigner.jar', 'verify', '--verbose', signed])
    run([TOOLS/'zipalign.exe', '-c', '-p', '4', signed])
    badging = version([TOOLS/'aapt2.exe', 'dump', 'badging', signed])
    (stage/'signed-badging.txt').write_text(badging+'\n', encoding='utf-8')
    verified_manifest = verify_badging(badging, package, args.version, args.abi, label)
    signed.replace(apk)
    receipt = {'package':package, 'abi':args.abi, 'version':args.version, 'apk':str(apk),
               'sha256':hashlib.sha256(apk.read_bytes()).hexdigest(),
               'libraries':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in libraries},
               'staging':str(stage), 'build_tools':TOOLS.name, 'verified_manifest':verified_manifest,
               'javac':version([JDK/'bin/javac.exe', '--version']),
               'd8':version([JDK/'bin/java.exe', '-cp', TOOLS/'lib/d8.jar',
                             'com.android.tools.r8.D8', '--version'])}
    (output/'package-receipt.json').write_text(json.dumps(receipt, indent=2))
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
