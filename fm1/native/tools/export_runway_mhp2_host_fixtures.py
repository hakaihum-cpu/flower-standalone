#!/usr/bin/env python3
"""Materialize reference fixtures for the EXISTING native MHP2 C decoder test.

Output is temporary prebuild test data, NEVER an installable FM-1 resource.
Use the exact GitHub-committed source-only MHP2 chunks and verify SHA/count.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path
from pack_miyako_resource import CHUNK_DIR, MHP2_CORE_BYTES, read_payload
from mhp2 import unpack

FRAMES=628
FRAME_BYTES=4608
OFFSET_BYTES=629*4
EXPECTED_RESOURCE_SHA='2426992e3f5c06870a654c49086923293e6c00247a1796f9f14048824bf1c17c'
EXPECTED_INDEX_SHA='387652dd94d90d51d2f636320a72449c1557bacdf759d7cb2f62999c15e6d4a0'


def export(payload: bytes, output: Path):
    assert hashlib.sha256(payload).hexdigest()==EXPECTED_RESOURCE_SHA
    vq=unpack(payload[:MHP2_CORE_BYTES])
    assert len(vq)==279936
    overlay=payload[MHP2_CORE_BYTES:]
    assert len(overlay)==79916
    frame_offsets=struct.unpack_from('<629I',overlay)
    assert frame_offsets[0]==0 and frame_offsets[-1]==len(overlay)-OFFSET_BYTES
    output.mkdir(parents=True,exist_ok=True)
    images=bytearray(FRAMES*FRAME_BYTES)
    indices_sha=hashlib.sha256()
    patches=0
    for f in range(FRAMES):
        clip=f//27
        book=vq[768+clip*4096:768+(clip+1)*4096]
        tile_ids=vq[99072+f*288:99072+(f+1)*288]
        frame=bytearray(FRAME_BYTES)
        for tile_id in range(288):
            ty,tx=divmod(tile_id,12)
            from_dict=tile_ids[tile_id]*16
            for row in range(4):
                dst=(ty*4+row)*48+tx*4
                start=from_dict+row*4
                frame[dst:dst+4]=book[start:start+4]
        lo,hi=frame_offsets[f:f+2]
        assert 0<=lo<=hi<=len(overlay)-OFFSET_BYTES and (hi-lo)%18==0
        encountered=set()
        for off in range(lo,hi,18):
            start=OFFSET_BYTES+off
            tile=struct.unpack_from('<H',overlay,start)[0]
            assert 0<=tile<288 and tile not in encountered
            encountered.add(tile)
            ty,tx=divmod(tile,12)
            for row in range(4):
                dst=(ty*4+row)*48+tx*4
                src=start+2+row*4
                frame[dst:dst+4]=overlay[src:src+4]
        patches+=len(encountered)
        at=f*FRAME_BYTES
        images[at:at+FRAME_BYTES]=frame
        for byte in frame:
            indices_sha.update(bytes([byte>>4,byte&15]))
    assert patches==4300
    assert indices_sha.hexdigest()==EXPECTED_INDEX_SHA,indices_sha.hexdigest()
    (output/'reference.vq').write_bytes(vq)
    (output/'encoded.mhp2').write_bytes(payload[:MHP2_CORE_BYTES])
    (output/'overlay.bin').write_bytes(overlay)
    (output/'expected_patched.bin').write_bytes(images)
    assert len(images)==FRAMES*FRAME_BYTES
    print('PASS: 628 actual Runway frames, 4300 corrections, exact pixels and CRC-protected MHP2 data')
    print('HOST C fixtures: reference.vq encoded.mhp2 overlay.bin expected_patched.bin')


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--chunk-dir',type=Path,default=CHUNK_DIR)
    args=p.parse_args()
    export(read_payload(args.chunk_dir),args.out)

if __name__=='__main__':main()
