#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, hashlib, json, os, struct, subprocess, sys
from pathlib import Path

MAGIC=b'EPBANK1\0'
HEADER=struct.Struct('<8sIIIIIIQ')
ENTRY=struct.Struct('<BBBBIQQ')
VERSION=1
SUSTAIN=0
RELEASE=1

def sha256(path: Path, block=1024*1024):
    h=hashlib.sha256()
    with path.open('rb') as f:
        while True:
            b=f.read(block)
            if not b: break
            h.update(b)
    return h.hexdigest()

def probe(path: Path):
    cmd=['ffprobe','-v','error','-select_streams','a:0',
         '-show_entries','stream=sample_rate,channels,bits_per_raw_sample,duration_ts,time_base',
         '-of','json',str(path)]
    p=subprocess.run(cmd,capture_output=True,text=True,check=True)
    s=json.loads(p.stdout)['streams'][0]
    sr=int(s['sample_rate']); ch=int(s['channels'])
    bits=int(s.get('bits_per_raw_sample') or 0)
    frames=int(s['duration_ts'])
    if ch!=2 or sr!=48000 or bits!=24:
        raise ValueError(f'{path.name}: expected 48kHz/24-bit/stereo, got {sr}Hz/{bits}bit/{ch}ch')
    return sr,ch,bits,frames

def load_manifest(path: Path):
    with path.open('r',encoding='utf-8-sig',newline='') as f:
        rows=list(csv.DictReader(f))
    required={'expected_wav','midi_note','velocity','round_robin','note_on_sec','note_off_sec','slot_end_sec'}
    if not rows or not required.issubset(rows[0]):
        raise ValueError('Unexpected capture manifest')
    return rows

def main():
    ap=argparse.ArgumentParser(description='Build EPBANK1 directly from FLAC, deduplicating byte-identical RR renders.')
    ap.add_argument('flac_dir',type=Path)
    ap.add_argument('-m','--manifest',type=Path,default=Path(__file__).with_name('capture_manifest.csv'))
    ap.add_argument('-o','--output',type=Path,default=Path('epbank.bin'))
    args=ap.parse_args()

    rows=load_manifest(args.manifest)
    expected=sorted({Path(r['expected_wav']).with_suffix('.flac').name for r in rows})
    missing=[n for n in expected if not (args.flac_dir/n).is_file()]
    if missing:
        raise FileNotFoundError('Missing FLAC: '+', '.join(missing))

    meta={}
    hashes={}
    groups={}
    for n in expected:
        p=args.flac_dir/n
        meta[n]=probe(p)
        h=sha256(p)
        hashes[n]=h
        groups.setdefault(h,[]).append(n)

    # Every source format must match exactly.
    fmts={(v[0],v[1],v[2]) for v in meta.values()}
    if fmts!={(48000,2,24)}:
        raise ValueError(f'Unexpected mixed formats: {fmts}')
    sr=48000; ch=2; bits=24; bpf=ch*(bits//8)

    # Plan one decoded PCM blob per unique FLAC hash.
    group_order=sorted(groups, key=lambda h: groups[h][0])
    entry_count=len(rows)*2
    data_offset=HEADER.size+entry_count*ENTRY.size
    group_base={}
    cur=data_offset
    for h in group_order:
        rep=groups[h][0]
        frames=meta[rep][3]
        group_base[h]=cur
        cur += frames*bpf

    entries=[]
    truncations=[]
    # Preserve all 3 RR logical entries. Identical RR files point at the same PCM bytes.
    for r in sorted(rows,key=lambda x:(Path(x['expected_wav']).name,int(x['midi_note']))):
        n=Path(r['expected_wav']).with_suffix('.flac').name
        total=meta[n][3]
        base=group_base[hashes[n]]
        note=int(r['midi_note']); vel=int(r['velocity']); rr=int(r['round_robin'])
        t0=round(float(r['note_on_sec'])*sr)
        t1=round(float(r['note_off_sec'])*sr)
        t2=round(float(r['slot_end_sec'])*sr)
        if t0>total:
            raise ValueError(f'{n}: note {note} starts beyond EOF')
        t1=min(t1,total); t2=min(t2,total)
        if t2 < round(float(r['slot_end_sec'])*sr):
            truncations.append((n,note,round(float(r['slot_end_sec'])*sr)-t2))
        for part,a,b in ((SUSTAIN,t0,t1),(RELEASE,t1,t2)):
            frames=max(0,b-a)
            off=base+a*bpf
            entries.append((note,vel,rr,part,frames,off,frames*bpf))

    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('wb') as out:
        out.write(HEADER.pack(MAGIC,VERSION,sr,ch,bits,len(entries),0,data_offset))
        for e in entries: out.write(ENTRY.pack(*e))
        if out.tell()!=data_offset: raise AssertionError('index size mismatch')

        for h in group_order:
            rep=groups[h][0]
            p=args.flac_dir/rep
            expected_bytes=meta[rep][3]*bpf
            before=out.tell()
            proc=subprocess.Popen(['ffmpeg','-v','error','-i',str(p),'-map','0:a:0','-f','s24le','-acodec','pcm_s24le','pipe:1'],stdout=subprocess.PIPE)
            while True:
                b=proc.stdout.read(1024*1024)
                if not b: break
                out.write(b)
            rc=proc.wait()
            if rc!=0: raise RuntimeError(f'ffmpeg failed for {rep}: {rc}')
            wrote=out.tell()-before
            if wrote!=expected_bytes:
                raise ValueError(f'{rep}: decoded bytes {wrote} != expected {expected_bytes}')

    print(f'Built: {args.output}')
    print(f'Logical capture files: {len(expected)}')
    print(f'Unique PCM sources: {len(group_order)}')
    print(f'Logical captures: {len(rows)}; index entries: {len(entries)}')
    print(f'Format: 48000 Hz / stereo / 24-bit')
    print(f'Size: {args.output.stat().st_size} bytes ({args.output.stat().st_size/(1024**3):.3f} GiB)')
    if truncations:
        # All known truncations are the final C8 release because render ends at 792.005208 s.
        print(f'EOF-clamped release entries: {len(truncations)}')
        for item in truncations[:6]: print(' ',item)
    print('Dedup groups:')
    for h in group_order:
        print(' ', ', '.join(groups[h]))
    return 0

if __name__=='__main__':
    raise SystemExit(main())
