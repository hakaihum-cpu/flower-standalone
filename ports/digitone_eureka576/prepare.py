#!/usr/bin/env python3
"""Convert CRF1 EUREKA frame bank to Digitone's 128x64, 1-bit panel format."""
from pathlib import Path
from io import BytesIO
import argparse
import hashlib
import struct
from PIL import Image

W, H, EXPECTED = 128, 64, 576
FRAME_BYTES = W * H // 8

def pack_digitone_image(data: bytes) -> bytes:
    with Image.open(BytesIO(data)) as source:
        source.load()
        frame = source.convert('L')
    scale = min(W / frame.width, H / frame.height)
    new_w = max(1, round(frame.width * scale))
    new_h = max(1, round(frame.height * scale))
    frame = frame.resize((new_w, new_h), Image.Resampling.LANCZOS)
    dest = Image.new('L', (W, H), 0)
    dest.paste(frame, ((W-new_w)//2, (H-new_h)//2))
    bw = dest.point(lambda v: 255 if v > 127 else 0).convert('1')
    px = bw.load()
    packed = bytearray(FRAME_BYTES)
    for x in range(W):
        for y in range(H):
            if px[x,y]:
                packed[(7-(y>>3))+8*x] |= (1 << (y&7))
    return bytes(packed)

def convert(source: Path, out: Path):
    data = source.read_bytes()
    if data[:4] != b'CRF1':
        raise ValueError('CRF1 magic invalid')
    n, = struct.unpack_from('<I',data,4)
    if n != EXPECTED:
        raise ValueError(f'expected {EXPECTED}, got {n}')
    header = 8 + n * 12
    if len(data) < header:
        raise ValueError('truncated CRF1 index')
    result=bytearray()
    for idx in range(n):
        off, flags, size = struct.unpack_from('<III',data,8+12*idx)
        if off < header or off+size > len(data) or size < 32:
            raise ValueError(f'bad frame range {idx}')
        encoded = data[off:off+size]
        if encoded[:2]!=b'\xff\xd8' or encoded[-2:]!=b'\xff\xd9':
            raise ValueError(f'invalid JPEG {idx}')
        result.extend(pack_digitone_image(encoded))
    assert len(result)==EXPECTED*FRAME_BYTES
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_bytes(result)
    print('frames',n,'bytes',len(result),'sha256',hashlib.sha256(result).hexdigest())

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('source',type=Path)
    ap.add_argument('out',type=Path)
    a=ap.parse_args()
    convert(a.source,a.out)
