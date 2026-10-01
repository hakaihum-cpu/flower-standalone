#!/usr/bin/env python3
from pathlib import Path
import sys
ARGS=["-DCMAKE_JOB_POOLS:STRING=compile=2;link=1","-DCMAKE_JOB_POOL_COMPILE:STRING=compile","-DCMAKE_JOB_POOL_LINK:STRING=link"]
p=Path(sys.argv[1] if len(sys.argv)>1 else 'Builds/Android/app/build.gradle')
if not p.is_file(): raise SystemExit(f'[FAIL] generated Gradle file missing: {p}')
s=p.read_text(); lines=s.splitlines(True)
idx=[i for i,l in enumerate(lines) if 'arguments(' in l and '-DANDROID_TOOLCHAIN=' in l]
if len(idx)!=1: raise SystemExit(f'[FAIL] expected one CMake arguments line, found {len(idx)}')
i=idx[0]; line=lines[i]
if all(a in line for a in ARGS): print('[PASS] job pools already present'); raise SystemExit(0)
if any(a in line for a in ARGS): raise SystemExit('[FAIL] partial job-pool injection')
nl='\n' if line.endswith('\n') else ''; body=line[:-1] if nl else line; close=body.rfind(')')
if close<0: raise SystemExit('[FAIL] malformed CMake arguments line')
lines[i]=body[:close]+', '+', '.join(f'"{a}"' for a in ARGS)+body[close:]+nl
p.write_text(''.join(lines))
print('[PASS] injected compile=2/link=1 job pools')