#!/usr/bin/env python3
"""Losslessly pack all 576 user-provided EUREKA frames for Digitone 128x64 LCD.
2-byte LE record size; mode 0 raw frame, mode 1 XOR-delta zero/literal RLE.
All outputs and decoding are checked. No image/frame is omitted.
"""
from pathlib import Path
from io import BytesIO
import argparse
import hashlib
import struct
from PIL import Image

W, H, EXPECTED = 128, 64, 576
FRAME_BYTES = 1024

def pack_image(jpeg):
    with Image.open(BytesIO(jpeg)) as source:
        source.load()
        gray = source.convert("L")
    scale = min(W / gray.width, H / gray.height)
    nw, nh = max(1, round(gray.width*scale)), max(1, round(gray.height*scale))
    resized = gray.resize((nw, nh), Image.Resampling.LANCZOS)
    canvas = Image.new("L", (W, H), 0)
    canvas.paste(resized, ((W-nw)//2, (H-nh)//2))
    pixels = canvas.point(lambda v: 255 if v > 127 else 0).convert("1").load()
    out = bytearray(FRAME_BYTES)
    for x in range(W):
        for y in range(H):
            if pixels[x, y]:
                out[7-(y >> 3)+8*x] |= 1 << (y & 7)
    return bytes(out)

def delta_rle(delta):
    encoded = bytearray()
    p = 0
    while p < FRAME_BYTES:
        run = 0
        while p+run < FRAME_BYTES and run < 128 and delta[p+run] == 0:
            run += 1
        if run >= 2:
            encoded.append(run-1)
            p += run
            continue
        start = p
        p += 1
        while p-start < 128 and p < FRAME_BYTES:
            if delta[p] == 0 and p+1 < FRAME_BYTES and delta[p+1] == 0:
                break
            p += 1
        encoded.append(0x80 | (p-start-1))
        encoded.extend(delta[start:p])
    return bytes(encoded)

def encode(frames):
    previous = bytes(FRAME_BYTES)
    out = bytearray()
    modes = [0, 0]
    for frame in frames:
        if len(frame) != FRAME_BYTES:
            raise ValueError("frame length")
        delta = bytes(a ^ b for a, b in zip(frame, previous))
        rle = delta_rle(delta)
        mode = 1 if len(rle) < FRAME_BYTES else 0
        payload = bytes([mode]) + (rle if mode else frame)
        out.extend(struct.pack("<H", len(payload)))
        out.extend(payload)
        modes[mode] += 1
        previous = frame
    return bytes(out), modes

def decode(data, n):
    previous = bytearray(FRAME_BYTES)
    pos = 0
    frames = []
    for i in range(n):
        if pos+3 > len(data):
            raise ValueError(f"truncated record {i}")
        length, = struct.unpack_from("<H", data, pos)
        pos += 2
        if length < 2 or pos+length > len(data):
            raise ValueError(f"invalid record length {i}")
        end = pos+length
        mode = data[pos]
        pos += 1
        if mode == 0:
            if end-pos != FRAME_BYTES:
                raise ValueError(f"raw size {i}")
            previous[:] = data[pos:end]
            pos = end
        elif mode == 1:
            cursor = 0
            while pos < end:
                tag = data[pos]
                pos += 1
                count = (tag & 127)+1
                if cursor+count > FRAME_BYTES:
                    raise ValueError(f"RLE overflow {i}")
                if tag & 128:
                    if pos+count > end:
                        raise ValueError(f"truncated literal {i}")
                    for k in range(count):
                        previous[cursor+k] ^= data[pos+k]
                    pos += count
                cursor += count
            if cursor != FRAME_BYTES:
                raise ValueError(f"incomplete frame {i}")
        else:
            raise ValueError(f"bad mode {i}")
        frames.append(bytes(previous))
    if pos != len(data):
        raise ValueError("trailing bytes")
    return frames


def build_seek(frames, encoded):
    """96 exact decoded keyframes plus LE offsets to the next compressed record."""
    if len(frames) != EXPECTED:
        raise ValueError("invalid seek frame count")
    pos = 0
    output = bytearray()
    for i, frame in enumerate(frames):
        if pos + 2 > len(encoded):
            raise ValueError("seek record header missing")
        length, = struct.unpack_from("<H", encoded, pos)
        pos += 2 + length
        if pos > len(encoded):
            raise ValueError("seek record exceeds stream")
        if i % 6 == 0:
            output.extend(struct.pack("<I", pos))
            output.extend(frame)
    if pos != len(encoded) or len(output) != 96 * 1028:
        raise ValueError("seek table malformed")
    for i in range(EXPECTED):
        slot = (i // 6) * 1028
        cursor, = struct.unpack_from("<I", output, slot)
        previous = bytearray(output[slot + 4:slot + 1028])
        for _ in range(i % 6):
            if cursor + 3 > len(encoded):
                raise ValueError("seek points past stream")
            length, = struct.unpack_from("<H", encoded, cursor)
            payload = encoded[cursor + 2:cursor + 2 + length]
            if len(payload) != length:
                raise ValueError("truncated seek payload")
            if payload[0] == 0:
                if len(payload) != 1025:
                    raise ValueError("invalid raw seek record")
                previous[:] = payload[1:]
                cursor += 2 + length
                continue
            if payload[0] != 1:
                raise ValueError("invalid seek record mode")
            used, p = 0, 1
            while p < len(payload):
                tag = payload[p]
                p += 1
                count = (tag & 127) + 1
                if used + count > 1024:
                    raise ValueError("seek RLE overflow")
                if tag & 128:
                    if p + count > len(payload):
                        raise ValueError("truncated seek literal")
                    for j in range(count):
                        previous[used + j] ^= payload[p + j]
                    p += count
                used += count
            if used != 1024 or p != len(payload):
                raise ValueError("seek RLE incomplete")
            cursor += 2 + length
        if bytes(previous) != frames[i]:
            raise ValueError(f"checkpoint seek mismatch at frame {i}")
    return bytes(output)

def convert(source, out, seek_out=None):
    data = source.read_bytes()
    if len(data) < 8 or data[:4] != b"CRF1":
        raise ValueError("CRF1 header invalid")
    count, = struct.unpack_from("<I", data, 4)
    if count != EXPECTED:
        raise ValueError(f"expected {EXPECTED} frames, got {count}")
    head = 8+count*12
    if head > len(data):
        raise ValueError("truncated index")
    frames = []
    for i in range(count):
        off, flags, size = struct.unpack_from("<III", data, 8+12*i)
        if off < head or off+size > len(data) or size < 32:
            raise ValueError(f"invalid frame index {i}")
        jpeg = data[off:off+size]
        if jpeg[:2] != b"\xff\xd8" or jpeg[-2:] != b"\xff\xd9":
            raise ValueError(f"invalid JPEG {i}")
        frames.append(pack_image(jpeg))
    encoded, modes = encode(frames)
    if decode(encoded, count) != frames:
        raise ValueError("lossless decode mismatch")
    # The prior failed OS depacker verification was short by 66,539 bytes.
    # Demand >130k byte raw saving (and still let elekloader's final in-place
    # depack verifier be authoritative; this is a preflight fail-fast only).
    if len(encoded) >= count*FRAME_BYTES-130000:
        raise ValueError(f"insufficient compression gain: packed {len(encoded)}")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(encoded)
    if seek_out is not None:
        seek_data = build_seek(frames, encoded)
        seek_out.parent.mkdir(parents=True, exist_ok=True)
        seek_out.write_bytes(seek_data)
        print(f"PASS checkpoints=96 bytes={len(seek_data)} sha256={hashlib.sha256(seek_data).hexdigest()}")
    print(f"PASS frames={count} raw={count*FRAME_BYTES} packed={len(encoded)} "
          f"mode_raw={modes[0]} mode_delta={modes[1]} sha256={hashlib.sha256(encoded).hexdigest()}")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("source", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--seek", type=Path, default=None)
    a = ap.parse_args()
    convert(a.source, a.out, a.seek)
