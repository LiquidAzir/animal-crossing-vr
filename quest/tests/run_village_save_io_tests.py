"""Test actual village temporary-write/publish path without loading a real save."""
from pathlib import Path
import argparse,datetime,hashlib,json,os,subprocess
source=Path(__file__).resolve().parents[2];workspace=source.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--serial')
parser.add_argument('--check-baseline',action='store_true',help='Prove tracked old source fails these fault cases')
args=parser.parse_args();stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
out=workspace/'research'/('village-save-io-'+stamp);out.mkdir()
env=dict(os.environ);env['PATH']=r'C:\msys64\mingw32\bin;'+env.get('PATH','')
def run(command,log,cwd=out,check=True):
    r=subprocess.run([str(x) for x in command],cwd=cwd,env=env,capture_output=True,timeout=60)
    (out/log).write_bytes(r.stdout+r.stderr);print((r.stdout+r.stderr).decode(errors='replace'),end='')
    if check:r.check_returncode()
    return r
def extract(s):
    start=s.index('static void pc_save_rotate_backups(');end=s.index('\nstatic void pc_ensure_save_dirs',start)
    rotate=s[start:end]
    start=s.index('    /* write temp file → rotate backups → rename */')
    end=s.index('\n/* Read a GCI file',start)
    return rotate+'\nstatic int write_test_gci(const char* gci_path,const char* tmp_path){\n'+\
        'FILE* fp; unsigned char dir_hdr[64]; unsigned char* file_data=malloc(GCI_FILE_DATA_SIZE);\n'+\
        'if(!file_data)return FALSE;\nfor(int i=0;i<64;++i)dir_hdr[i]=(unsigned char)(i+3);\n'+\
        'for(int i=0;i<GCI_FILE_DATA_SIZE;++i)file_data[i]=(unsigned char)(i*17+5);\n'+s[start:end]
actual=source/'pc/src/pc_m_card.c';production=actual.read_text(encoding='utf-8')
include=out/'village_save_actual.inc';include.write_text(extract(production),encoding='utf-8')
common=['-std=c11','-O2','-Wall','-Wextra','-I'+str(out),source/'quest/tests/village_save_io_faults.c']
run([r'C:\msys64\mingw32\bin\gcc.exe',*common,'-o',out/'test.exe'],'host-compile.log')
fixture=out/'host-fixture';fixture.mkdir();run([out/'test.exe'],'host-results.log',cwd=fixture)
paths=json.loads((workspace/'toolchain/paths.json').read_text(encoding='utf-8-sig'))
run([paths['clang_armv7_api24'],*common,'-o',out/'village-save-test'],'arm32-compile.log')
receipt={'production_sha256':hashlib.sha256(actual.read_bytes()).hexdigest()}
if args.check_baseline:
    old=subprocess.run(['git','show','HEAD:pc/src/pc_m_card.c'],cwd=source,capture_output=True,check=True).stdout.decode()
    include.write_text(extract(old),encoding='utf-8')
    run([r'C:\msys64\mingw32\bin\gcc.exe',*common,'-o',out/'baseline.exe'],'baseline-compile.log')
    baseline_fixture=out/'baseline-fixture';baseline_fixture.mkdir()
    result=run([out/'baseline.exe'],'baseline-results.log',cwd=baseline_fixture,check=False)
    if not result.returncode:raise RuntimeError('Baseline unexpectedly passed failure regression')
    receipt['baseline_exit']=result.returncode
    include.write_text(extract(production),encoding='utf-8')
if args.serial:
    adb=[paths['adb'],'-s',args.serial];remote='/data/local/tmp/acquest-village-save-io/'+stamp
    run([*adb,'shell','mkdir','-p',remote],'mkdir.log')
    run([*adb,'push',out/'village-save-test',remote+'/test'],'push.log')
    run([*adb,'shell','chmod','700',remote+'/test'],'chmod.log')
    result=run([*adb,'shell','cd '+remote+' && ./test'],'device-results.log',check=False)
    receipt.update(serial=args.serial,remote=remote,device_exit=result.returncode)
(out/'receipt.json').write_text(json.dumps(receipt,indent=2));print('Receipt:',out/'receipt.json')
raise SystemExit(receipt.get('device_exit',0))
