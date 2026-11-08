#!/usr/bin/env python3
"""Offline common-tile prototype for MIYAKO FM-1 (NOT production firmware).

LZ4-compress 24 existing 8x4 VQ tile dictionaries and 628 independent tile
placement maps. Lossless ONLY against the already decoded VQ pixels. This
does not restore the original colour/edge detail lost by the VQ encoder.
Never overwrites the approved res.bin source or changes app.c.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct

try:
    import lz4.block
except ImportError as e:
    raise SystemExit("Offline packer requires Python lz4; no target dependency") from e

MAGIC = b"MTL1"
N, CLIPS, STEP, MAP, BOOK = 628, 24, 27, 288, 4096
CAPACITY = 69 * 4096 - 32
OLD_BYTES = 279936
HEADER = struct.Struct("<4sHHHHIIII")
ENTRY = struct.Struct("<IHH")
PAL = HEADER.size
BOOK_DIR = PAL + CLIPS * 32
MAP_DIR = BOOK_DIR + CLIPS * ENTRY.size
DATA = MAP_DIR + N * ENTRY.size


def decompress_lz4(inp: bytes, expected: int) -> bytes:
    """Bounded LZ4 block decoder, independent from the packer's lz4 module."""
    out, i = bytearray(), 0
    while i < len(inp):
        t = inp[i]; i += 1
        lit = t >> 4
        if lit == 15:
            while True:
                if i == len(inp): raise ValueError("literal length truncated")
                n = inp[i]; i += 1; lit += n
                if n != 255: break
        if i + lit > len(inp) or len(out) + lit > expected:
            raise ValueError("literal out of bounds")
        out += inp[i:i+lit]; i += lit
        if i == len(inp): break
        if i + 2 > len(inp): raise ValueError("missing match")
        off = inp[i] | (inp[i+1] << 8); i += 2
        if off == 0 or off > len(out): raise ValueError("invalid back-reference")
        length = (t & 15) + 4
        if (t & 15) == 15:
            while True:
                if i == len(inp): raise ValueError("match length truncated")
                n = inp[i]; i += 1; length += n
                if n != 255: break
        if len(out) + length > expected: raise ValueError("match out of bounds")
        for _ in range(length): out.append(out[-off])
    if len(out) != expected: raise ValueError("wrong decoded length")
    return bytes(out)


def compressed(data: bytes) -> tuple[bytes,int]:
    c = lz4.block.compress(data, mode="high_compression",
                           compression=9, store_size=False)
    if len(c) >= len(data): return data, 1  # raw fallback
    if decompress_lz4(c, len(data)) != data:
        raise AssertionError("LZ4 decode mismatch")
    return c, 0


def encode(old: bytes) -> bytes:
    if len(old) != OLD_BYTES: raise ValueError("old photo VQ payload size")
    blocks = []
    index = []
    p = DATA
    for clip in range(CLIPS):
        raw = old[768 + clip*BOOK:768+(clip+1)*BOOK]
        blob, flag = compressed(raw)
        index.append((p,len(blob),flag)); blocks.append(blob); p += len(blob)
    for frame in range(N):
        raw = old[99072 + frame*MAP:99072+(frame+1)*MAP]
        blob, flag = compressed(raw)
        index.append((p,len(blob),flag)); blocks.append(blob); p += len(blob)
    result = HEADER.pack(MAGIC, 1, N, CLIPS, STEP, PAL,
                         BOOK_DIR, MAP_DIR, DATA)
    result += old[:CLIPS*32]
    result += b"".join(ENTRY.pack(*e) for e in index)
    result += b"".join(blocks)
    if len(result) != p or len(result) > CAPACITY:
        raise ValueError("candidate exceeds the approved resource region")
    return result


def read_block(blob:bytes, at:int, expect:int) -> bytes:
    offset,size,flag = ENTRY.unpack_from(blob,at)
    if flag not in (0,1) or not size or offset < DATA or offset+size > len(blob):
        raise ValueError("bad indexed block")
    if flag == 1:
        if size != expect: raise ValueError("raw block length")
        return blob[offset:offset+size]
    return decompress_lz4(blob[offset:offset+size],expect)


def frame_pixels(palette:bytes,book:bytes,ids:bytes) -> tuple[bytes,bytes]:
    frame = bytearray(96*96//2)
    for ty in range(24):
        for tx in range(12):
            b = book[ids[ty*12+tx]*16:ids[ty*12+tx]*16+16]
            for row in range(4):
                pos = (ty*4+row)*48+tx*4
                frame[pos:pos+4] = b[row*4:row*4+4]
    return palette,bytes(frame)


def original_frame(old: bytes, f:int) -> tuple[bytes,bytes]:
    c = f//STEP
    return frame_pixels(old[c*32:(c+1)*32],
                        old[768+c*BOOK:768+(c+1)*BOOK],
                        old[99072+f*MAP:99072+(f+1)*MAP])


def candidate_frame(data:bytes, f:int) -> tuple[bytes,bytes]:
    if not 0 <= f < N or not DATA <= len(data) <= CAPACITY:
        raise ValueError("frame/resource bounds")
    if HEADER.unpack_from(data) != (MAGIC,1,N,CLIPS,STEP,PAL,BOOK_DIR,MAP_DIR,DATA):
        raise ValueError("common tile header mismatch")
    c=f//STEP
    return frame_pixels(data[PAL+c*32:PAL+(c+1)*32],
                        read_block(data,BOOK_DIR+c*ENTRY.size,BOOK),
                        read_block(data,MAP_DIR+f*ENTRY.size,MAP))


def verify(old:bytes, encoded:bytes)->str:
    digest=hashlib.sha256()
    # 1. Full sequential pass, all frame pixels and per-clip palette.
    for f in range(N):
        expect=original_frame(old,f)
        actual=candidate_frame(encoded,f)
        if expect != actual: raise AssertionError(f"frame {f} incorrect")
        digest.update(actual[0]);digest.update(actual[1])
    # 2. Full shuffled random-access pass.
    order=list(range(N))
    random.Random(0x4D49).shuffle(order)
    for f in order:
        if candidate_frame(encoded,f)!=original_frame(old,f):
            raise AssertionError(f"frame {f} random access incorrect")
    # 3. Corrupted header/index must be rejected.
    bad=bytearray(encoded);bad[0]^=255
    try: candidate_frame(bad,0)
    except ValueError: pass
    else: raise AssertionError("corrupt header accepted")
    bad=bytearray(encoded)
    struct.pack_into("<I",bad,MAP_DIR,0xffffffff)
    try: candidate_frame(bad,0)
    except ValueError: pass
    else: raise AssertionError("out-of-bounds selector accepted")
    return digest.hexdigest()


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--old-payload",type=Path,
                    help="Offline test input; defaults to SHA-verified repository archive")
    ap.add_argument("--output",type=Path,help="Separate candidate only, never overwrites original")
    ap.add_argument("--report",type=Path)
    args=ap.parse_args()
    if args.old_payload:
        source=args.old_payload.read_bytes()
    else:
        from pack_miyako_resource import CHUNK_DIR,read_payload
        source=read_payload(CHUNK_DIR)
    output=encode(source)
    sha=verify(source,output)
    info={"frames":N,"size":len(output),"source_size":len(source),
          "saved":len(source)-len(output),"max_payload":CAPACITY,
          "source_sha256":hashlib.sha256(source).hexdigest(),
          "candidate_sha256":hashlib.sha256(output).hexdigest(),
          "decoded_palettes_and_pixels_sha256":sha,
          "scratch_book_plus_map_bytes":BOOK+MAP,
          "checks":"628 sequential + 628 shuffled + malformed-input rejection",
          "limit":"lossless versus old VQ frames only, NOT original user RGB"}
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_bytes(output)
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(info,indent=2,ensure_ascii=False))
    print(json.dumps(info,indent=2,ensure_ascii=False))


if __name__=="__main__":
    main()
