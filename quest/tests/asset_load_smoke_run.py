"""Run actual ARM32 game asset initialization separately from the Android app.

Uses only the user's existing private staged ROM. Copies to our private shell
test directory; never starts an Activity or opens any save directory.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
WORKSPACE=ROOT.parent
REMOTE='/data/local/tmp/acquest-assets'


class Disc:
    def __init__(self,path):
        self.file=path.open('rb')
        header=self.file.read(0x8000)
        self.blocks=None
        if header[:4]==b'CISO':
            self.block_size=struct.unpack_from('<I',header,4)[0]
            self.blocks=[]
            physical=0
            for present in header[8:]:
                self.blocks.append(physical if present else None)
                physical+=bool(present)
    def read(self,offset,length):
        if self.blocks is None:
            self.file.seek(offset);return self.file.read(length)
        result=bytearray()
        while length:
            block,inside=divmod(offset,self.block_size)
            amount=min(length,self.block_size-inside)
            physical=self.blocks[block] if block<len(self.blocks) else None
            if physical is None:result.extend(bytes(amount))
            else:
                self.file.seek(0x8000+physical*self.block_size+inside)
                result.extend(self.file.read(amount))
            offset+=amount;length-=amount
        return bytes(result)


def yaz0(data):
    if data[:4]!=b'Yaz0':return data
    size=struct.unpack_from('>I',data,4)[0]
    result=bytearray();source=16
    while len(result)<size:
        flags=data[source];source+=1
        for bit in range(7,-1,-1):
            if len(result)>=size:break
            if flags&(1<<bit):result.append(data[source]);source+=1
            else:
                a,b=data[source:source+2];source+=2
                distance=((a&15)<<8)+b+1
                length=(a>>4)+2
                if a>>4==0:length=data[source]+18;source+=1
                for _ in range(length):result.append(result[-distance])
    assert len(result)==size
    return bytes(result)


def expectations(rom):
    disc=Disc(rom)
    header=disc.read(0,0x440)
    assert struct.unpack_from('>I',header,0x1c)[0]==0xc2339f3d,'Invalid GameCube disc'
    dol_offset,fst_offset,fst_size=struct.unpack_from('>III',header,0x420)
    dol_header=disc.read(dol_offset,0x100)
    offsets=struct.unpack_from('>18I',dol_header,0)
    sizes=struct.unpack_from('>18I',dol_header,0x90)
    dol=disc.read(dol_offset,max(o+s for o,s in zip(offsets,sizes)))
    fst=disc.read(fst_offset,fst_size)
    count=struct.unpack_from('>I',fst,8)[0];rel=None
    for i in range(1,count):
        name_offset,offset,size=struct.unpack_from('>III',fst,i*12)
        if name_offset>>24:continue
        name_start=count*12+(name_offset&0xffffff)
        name=fst[name_start:fst.index(0,name_start)].decode('ascii')
        if name=='foresta.rel.szs':rel=yaz0(disc.read(offset,size));break
    assert rel is not None,'REL not found'
    rows=re.findall(r'\{"assets/[^\"]+",\s*(\w+),\s*(0x[\dA-Fa-f]+),\s*(0x[\dA-Fa-f]+),\s*(\d+),\s*(\d+)\}',
                    (ROOT/'pc/src/pc_assets.c').read_text())
    result=[]
    for name,size,offset,source,swap in rows:
        size=int(size,16);offset=int(offset,16);source=int(source);swap=int(swap)
        raw=bytearray((rel if source==0 else dol)[offset:offset+size]);assert len(raw)==size
        if swap in (1,3):
            width=2 if swap==1 else 4
            raw=b''.join(raw[i:i+width][::-1] for i in range(0,size,width))
        elif swap==2:
            for vertex in range(0,size,16):
                for component in range(0,12,2):
                    i=vertex+component;raw[i],raw[i+1]=raw[i+1],raw[i]
        hash_value=2166136261
        for byte in raw:hash_value=((hash_value^byte)*16777619)&0xffffffff
        result.append(f'{{"{name}", {size}u, 0x{hash_value:08x}u}},')
    disc.file.close()
    return '\n'.join(result)+'\n',len(rows),len(dol),len(rel)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--rom',type=Path,default=WORKSPACE/'data/imports/rom/AnimalCrossing.ciso')
    parser.add_argument('--build',type=Path,default=WORKSPACE/'build/game-arm32')
    args=parser.parse_args()
    paths=json.loads((WORKSPACE/'toolchain/paths.json').read_text())
    build=WORKSPACE/'build/asset-load-smoke';build.mkdir(parents=True,exist_ok=True)
    receipt=WORKSPACE/'research'/datetime.datetime.now().strftime('asset-load-smoke-%Y%m%d-%H%M%S')
    receipt.mkdir(parents=True)
    expected,count,dol_bytes,rel_bytes=expectations(args.rom)
    (build/'asset_expectations.inc').write_text(expected)
    exe=build/'asset-load-smoke'
    subprocess.run([paths['clang_armv7_api24'],'-std=c11','-D_POSIX_C_SOURCE=200809L','-O2','-fPIE','-pie',
                    '-I'+str(build),str(ROOT/'quest/tests/asset_load_smoke.c'),'-ldl','-o',str(exe)],check=True)
    files=[exe,args.build/'game/libmain.so',args.build/'sdl/libSDL2.so',
           args.build/'openxr/src/loader/libopenxr_loader.so',args.rom]
    manifest={'abi':'armeabi-v7a','central_asset_count':count,'dol_bytes':dol_bytes,'rel_bytes':rel_bytes,
              'remote_test_directory':REMOTE,'files':[]}
    for path in files:
        manifest['files'].append({'name':path.name,'bytes':path.stat().st_size,
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    adb=paths['adb']
    def call(*args,**kwargs):return subprocess.run([adb,*args],check=True,text=True,capture_output=True,**kwargs)
    call('shell',f'mkdir -p {REMOTE}/rom && chmod 700 {REMOTE} {REMOTE}/rom')
    for path in files:
        remote=REMOTE+('/rom/AnimalCrossing.ciso' if path==args.rom else '/'+path.name)
        call('push',str(path),remote)
        print(path.name,'copied')
    for item in manifest['files']:
        remote=REMOTE+('/rom/AnimalCrossing.ciso' if item['name']==args.rom.name else '/'+item['name'])
        device_hash=call('shell','sha256sum '+remote).stdout.split()[0]
        assert device_hash==item['sha256'],f"Device hash mismatch: {item['name']}"
        item['device_sha256_verified']=True
    call('shell',f'chmod 500 {REMOTE}/asset-load-smoke && chmod 400 {REMOTE}/rom/AnimalCrossing.ciso')
    command=f'cd {REMOTE} && LD_LIBRARY_PATH={REMOTE} ./asset-load-smoke'
    result=subprocess.run([adb,'shell',command],text=True,capture_output=True,timeout=90)
    manifest['exit_code']=result.returncode
    (receipt/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (receipt/'results.txt').write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr,end='')
    print('Receipt:',receipt)
    return result.returncode


if __name__=='__main__':raise SystemExit(main())
