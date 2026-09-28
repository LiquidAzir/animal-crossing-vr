"""Fault-inject the actual CARD source using isolated synthetic save files."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, subprocess

source=Path(__file__).resolve().parents[2]
workspace=source.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--serial',help='Also execute ARM32 tests in private /data/local/tmp storage')
args=parser.parse_args()
stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
out=workspace/'research'/('card-io-'+stamp)
out.mkdir()
actual=source/'pc/src/pc_card.c'
(out/'pc_card_actual.inc').write_bytes(actual.read_bytes())
(out/'pc_platform.h').write_text('#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdint.h>\ntypedef int32_t s32; typedef uint32_t u32; typedef uint16_t u16; typedef uint8_t u8;\n')
env=dict(os.environ);env['PATH']=r'C:\msys64\mingw32\bin;'+env.get('PATH','')
def run(command,log,cwd=out,check=True):
    r=subprocess.run([str(x) for x in command],cwd=cwd,capture_output=True,env=env,timeout=60)
    (out/log).write_bytes(r.stdout+r.stderr);print((r.stdout+r.stderr).decode(errors='replace'),end='')
    if check:r.check_returncode()
    return r
common=['-std=c11','-O2','-Wall','-Wextra','-Wno-unused-parameter','-I'+str(out),source/'quest/tests/card_io_faults.c']
run([r'C:\msys64\mingw32\bin\gcc.exe',*common,'-o',out/'test.exe'],'host-compile.log')
host_cwd=out/'host-fixture';host_cwd.mkdir()
run([out/'test.exe'],'host-results.log',cwd=host_cwd)
paths=json.loads((workspace/'toolchain/paths.json').read_text(encoding='utf-8-sig'))
run([paths['clang_armv7_api24'],*common,'-o',out/'card-io-test'],'arm32-compile.log')
receipt={'production_sha256':hashlib.sha256(actual.read_bytes()).hexdigest()}
if args.serial:
    adb=[paths['adb'],'-s',args.serial];remote='/data/local/tmp/acquest-card-io/'+stamp
    run([*adb,'shell','mkdir','-p',remote],'mkdir.log')
    run([*adb,'push',out/'card-io-test',remote+'/test'],'push.log')
    run([*adb,'shell','chmod','700',remote+'/test'],'chmod.log')
    result=run([*adb,'shell','cd '+remote+' && ./test'],'device-results.log',check=False)
    receipt.update(serial=args.serial,remote=remote,device_exit=result.returncode)
(out/'receipt.json').write_text(json.dumps(receipt,indent=2))
print('Receipt:',out/'receipt.json')
raise SystemExit(receipt.get('device_exit',0))
