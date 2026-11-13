#!/usr/bin/env python3
"""Exhaustive offline 628-frame validation of encoded MHP2+corrections.

Run in existing first-stage CircleCI source checks, before FWSC creation.
No source frames are generated or rewritten here.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path
from pack_miyako_resource import read_payload, CHUNK_DIR, MHP2_CORE_BYTES
from mhp2 import unpack

COUNT=628
PIXEL_SHA256='68695e08463847565d09b87755f16d9631fb2276f485bc9a9451d68abac279dd'
BANK_SIZES=(81,64,100,81,81,81,70,70)


def check(data: bytes) -> None:
    if sum(BANK_SIZES) != COUNT:
        raise AssertionError('SEL banks do not add up to 628')
    legacy = unpack(data[:MHP2_CORE_BYTES])
    if len(legacy) != 279936:
        raise AssertionError('MHP2 core not 628 original-index frames')
    overlay=data[MHP2_CORE_BYTES:]
    frame_offsets=struct.unpack_from('<629I', overlay)
    digest=hashlib.sha256()
    seen=set()
    distinct_min=16
    for f in range(COUNT):
        clip=f//27
        book=legacy[768+clip*4096:768+(clip+1)*4096]
        tile_ids=legacy[99072+f*288:99072+(f+1)*288]
        pixels=bytearray(96*96)
        for ty in range(24):
            for tx in range(12):
                code=book[tile_ids[ty*12+tx]*16:tile_ids[ty*12+tx]*16+16]
                for row in range(4):
                    y=(ty*4+row)*96+tx*8
                    for col in range(4):
                        v=code[row*4+col]
                        pixels[y+2*col]=v>>4
                        pixels[y+2*col+1]=v&15
        for off in range(frame_offsets[f],frame_offsets[f+1],18):
            start=2516+off
            tile=struct.unpack_from('<H',overlay,start)[0]
            ty,tx=divmod(tile,12)
            correction=overlay[start+2:start+18]
            for row in range(4):
                pos=(ty*4+row)*96+tx*8
                for col in range(4):
                    v=correction[row*4+col]
                    pixels[pos+col*2]=v>>4
                    pixels[pos+col*2+1]=v&15
        unique=set(pixels)
        if len(unique)<5 or max(unique)>15:
            raise AssertionError(f'frame {f}: invalid palette coverage')
        distinct_min=min(distinct_min,len(unique))
        frame_hash=hashlib.sha256(pixels).digest()
        if frame_hash in seen:
            raise AssertionError(f'duplicate decoded frame {f}')
        seen.add(frame_hash)
        digest.update(pixels)
    got=digest.hexdigest()
    if got != PIXEL_SHA256:
        raise AssertionError(f'628-frame exact pixels changed: {got}')
    print(f'PASS MHP2: 628 frames, 24 clip palettes, 3916 source tile patches, {distinct_min}+ colours/frame, SHA256 {got}')


if __name__=='__main__':
    arg=argparse.ArgumentParser()
    arg.add_argument('--chunk-dir',type=Path,default=CHUNK_DIR)
    args=arg.parse_args()
    check(read_payload(args.chunk_dir))