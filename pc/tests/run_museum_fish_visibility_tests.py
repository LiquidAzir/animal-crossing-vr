"""Actual museum tank/plant/fish draw submission; no ROM, headset or game launch."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[2]
def function(source,name):
    match=re.search(r'^(?:static\s+)?\w+[ \t]+(?:\*\s*)?'+re.escape(name)+r'\([^;]*?\)\s*\{',source,re.M)
    if not match:raise ValueError('Missing function '+name)
    depth=1;start=source.index('{',match.start())
    for end in range(start+1,len(source)):
        depth+=(source[end]=='{')-(source[end]=='}')
        if not depth:return source[match.start():end+1]
    raise ValueError('Unclosed function '+name)
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--revision')
    args=parser.parse_args();out=ROOT/'pc/build32/museum-fish-visibility'/('baseline' if args.revision else 'fixed');out.mkdir(parents=True,exist_ok=True)
    def read(path):
        if args.revision:return subprocess.check_output(['git','show',args.revision+':'+path],cwd=ROOT).decode('utf-8')
        return (ROOT/path).read_text(encoding='utf-8')
    source=read('src/actor/ac_museum_fish.c')
    funcs=['mfish_cull_check','Museum_Fish_Suisou_draw','Museum_Fish_Kusa_Draw','Museum_Fish_Actor_draw']
    (out/'museum_fish_visibility_source.inc').write_text('\n'.join(function(source,n) for n in funcs),encoding='utf-8')
    arrays=[]
    for name in ['suisou_pos','kusa_pos']:
        match=re.search(r'(?:static )?xyz_t '+name+r'\[\d+\]\s*=\s*\{.*?\};',source,re.S)
        if not match:raise ValueError('Missing tank array '+name)
        arrays.append(match.group())
    (out/'museum_fish_positions.inc').write_text('\n'.join(arrays),encoding='utf-8')
    fish_enum=re.search(r'enum fish_type\s*\{.*?\};',read('include/ac_gyoei.h'),re.S).group()
    (out/'museum_fish_enum.inc').write_text(fish_enum,encoding='utf-8')
    env=dict(os.environ);env['PATH']=r'C:\msys64\mingw32\bin;'+env.get('PATH','')
    codes=[]
    for pc in [True,False]:
        name='pc' if pc else 'legacy';exe=out/(name+'.exe')
        command=[r'C:\msys64\mingw32\bin\gcc.exe','-std=c11','-O2','-Wall','-Wextra','-Wno-unused-variable','-I'+str(out)]
        if pc:command+=['-DTARGET_PC']
        subprocess.run(command+[str(ROOT/'pc/tests/museum_fish_visibility.c'),'-o',str(exe)],cwd=ROOT,env=env,check=True)
        result=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,timeout=30)
        (out/(name+'-results.txt')).write_bytes(result.stdout+result.stderr)
        print((result.stdout+result.stderr).decode(errors='replace'),end='');codes.append(result.returncode)
    (out/'receipt.json').write_text(json.dumps({'revision':args.revision,'actor_sha256':hashlib.sha256(source.encode()).hexdigest(),'results':codes},indent=2))
    print('Receipt:',out/'receipt.json');return int(any(codes))
if __name__=='__main__':raise SystemExit(main())
