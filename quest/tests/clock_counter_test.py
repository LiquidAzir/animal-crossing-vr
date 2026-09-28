"""Exercise the actual osGetTime source on Windows and Quest with injected counters."""
from pathlib import Path
import json
import os
import subprocess

SOURCE = Path(__file__).resolve().parents[2]
QUEST = SOURCE.parent
OUT = QUEST / 'research' / 'clock-counter'
OUT.mkdir(parents=True, exist_ok=True)
paths = json.loads((QUEST / 'toolchain/paths.json').read_text(encoding='utf-8-sig'))
clock = 40500000
max64 = (1 << 64) - 1
cases = []
for frequency in [1000000, 10000000, 1000000000]:
    old_boundary = max64 // clock
    deltas = {0, 1, frequency - 1, frequency, frequency + 1}
    deltas.update(old_boundary + n for n in range(-4, 5))
    for seconds in [455, 456, 600, 3600, 86400, 365 * 86400, 50 * 365 * 86400]:
        deltas.update(seconds * frequency + n for n in [-1, 0, 1])
    for delta in sorted(deltas):
        for start in [0, 1234567890, max64 - 987654321]:
            epoch = 34123456789012345
            now = (start + delta) & max64
            expected = epoch + delta * clock // frequency
            cases.append(f'{{{start}ULL,{now}ULL,{frequency}ULL,{epoch}LL,{expected}LL}}')

def extract(path):
    source = path.read_text()
    start = source.index('s64 osGetTime(void) {')
    end = source.index('\n}', start) + 2
    return source[start:end]

template = '''#include <stdint.h>
#include <stdio.h>
typedef uint64_t u64;
typedef int64_t s64;
#define GC_TIMER_CLOCK 40500000u
static u64 time_base_start, test_now, test_freq;
static s64 gc_epoch_offset_ticks;
static u64 SDL_GetPerformanceCounter(void) { return test_now; }
static u64 SDL_GetPerformanceFrequency(void) { return test_freq; }
FUNCTION
typedef struct { u64 start, now, freq; s64 epoch, expected; } Case;
static const Case cases[] = { CASES };
int main(void) {
  unsigned failures=0, i;
  for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
    time_base_start=cases[i].start; test_now=cases[i].now; test_freq=cases[i].freq;
    gc_epoch_offset_ticks=cases[i].epoch;
    s64 actual=osGetTime();
    if(actual!=cases[i].expected) {
      if(failures<3) printf("case%u frequency=%llu delta=%llu expected=%lld actual=%lld\\n",i,(unsigned long long)test_freq,(unsigned long long)(test_now-time_base_start),(long long)cases[i].expected,(long long)actual);
      ++failures;
    }
  }
  printf("checks=%u failures=%u\\n",i,failures);
  return failures ? 1 : 0;
}
'''.replace('CASES', ',\n'.join(cases))
env = dict(os.environ)
env['PATH'] = r'C:\msys64\mingw32\bin;' + env.get('PATH', '')
summary = {}
for label, path in [('fixed', SOURCE/'pc/src/pc_os.c'), ('before', QUEST.parent/'Animal Crossing VR/animal-crossing-vr-rewrite/pc/src/pc_os.c')]:
    c = OUT / f'{label}.c'
    c.write_text(template.replace('FUNCTION', extract(path)))
    for target, compiler in [('windows', r'C:\msys64\mingw32\bin\gcc.exe'), ('arm32', paths['clang_armv7_api24'])]:
        binary = OUT / f'{label}-{target}'
        if target == 'windows': binary = binary.with_suffix('.exe')
        subprocess.run([compiler, '-std=gnu11', '-O2', str(c), '-o', str(binary)], env=env, check=True, timeout=60)
        if target == 'arm32':
            remote = '/data/local/tmp/acquest-clock-counter'
            subprocess.run([paths['adb'], 'push', str(binary), remote], check=True, capture_output=True, timeout=30)
            subprocess.run([paths['adb'], 'shell', 'chmod', '700', remote], check=True, capture_output=True, timeout=30)
            command = [paths['adb'], 'shell', remote]
        else: command = [str(binary)]
        run = subprocess.run(command, capture_output=True, env=env, timeout=30)
        (OUT / f'{label}-{target}.log').write_bytes(run.stdout + run.stderr)
        summary[f'{label}-{target}'] = {'exit': run.returncode, 'output': run.stdout.decode().strip()}
        if (label == 'fixed') != (run.returncode == 0): raise RuntimeError(summary[f'{label}-{target}'])
(OUT / 'results.json').write_text(json.dumps(summary, indent=2))
print(json.dumps(summary, indent=2))
