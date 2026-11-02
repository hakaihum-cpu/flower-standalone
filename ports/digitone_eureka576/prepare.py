#!/usr/bin/env python3
"""Convert all 576 original EUREKA frames using the approved RG DOT 1-bit display.
2-byte LE record size; mode 0 raw frame, mode 1 XOR-delta zero/literal RLE.
The resulting 1-bit frames are compressed losslessly and verified. No frame is omitted.
"""
from pathlib import Path
from io import BytesIO
import argparse
import hashlib
import struct
from PIL import Image, ImageOps

W, H, EXPECTED = 128, 64, 576
FRAME_BYTES = 1024

# Match the approved RG Rotate DOT presentation, reduced to a 1-bit LCD:
# preserve the source composition, do not detect/crop faces, do not draw or
# substitute features. Fit the entire image with black letterboxing.
BAYER4 = (
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5,
)

def pack_image(jpeg, frame_index):
    """Convert exactly one original JPEG to a 128x64 RG-DOT 1-bit LCD frame."""
    with Image.open(BytesIO(jpeg)) as source:
        source.load()
        original = source.convert("RGB")

    fitted = ImageOps.contain(
        original, (W, H), method=Image.Resampling.LANCZOS)
    frame = Image.new("RGB", (W, H), (0, 0, 0))
    frame.paste(
        fitted,
        ((W - fitted.width) // 2, (H - fitted.height) // 2))

    pixels = frame.load()
    out = bytearray(FRAME_BYTES)
    for y in range(H):
        for x in range(W):
            r, g, b = pixels[x, y]
            luminance = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0
            threshold = 0.13 + 0.63 * BAYER4[(x & 3) + ((y & 3) << 2)] / 16.0
            if luminance >= threshold:
                out[7 - (y >> 3) + 8 * x] |= 1 << (y & 7)
    return bytes(out)


def save_preview(frames, path):
    """Contact sheet displaying the actual 1-bit firmware output in LCD yellow."""
    sheet = Image.new("RGB", (4 * W, 4 * H), (0, 0, 0))
    for idx in range(16):
        frame = frames[round(idx * (len(frames) - 1) / 15)]
        tile = Image.new("RGB", (W, H), (0, 0, 0))
        pix = tile.load()
        for x in range(W):
            for y in range(H):
                if frame[7 - (y >> 3) + 8 * x] & (1 << (y & 7)):
                    pix[x, y] = (245, 187, 17)
        sheet.paste(tile, ((idx % 4) * W, (idx // 4) * H))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


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
    """58 exact decoded keyframes plus LE offsets to the next compressed record."""
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
    if pos != len(encoded) or len(output) != ((EXPECTED + 5) // 6) * 1028:
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

def convert(source, out, seek_out=None, preview_out=None):
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
    jpegs = []
    for i in range(count):
        off, flags, size = struct.unpack_from("<III", data, 8+12*i)
        if off < head or off+size > len(data) or size < 32:
            raise ValueError(f"invalid frame index {i}")
        jpeg = data[off:off+size]
        if jpeg[:2] != b"\xff\xd8" or jpeg[-2:] != b"\xff\xd9":
            raise ValueError(f"invalid JPEG {i}")
        jpegs.append(jpeg)
    for i, jpeg in enumerate(jpegs):
        frames.append(pack_image(jpeg, i))
    encoded, modes = encode(frames)
    if decode(encoded, count) != frames:
        raise ValueError("lossless decode mismatch")
    # EUREKA RG DOT conversion needs to pass the final in-place depack budget.
    # Only the final elekloader in-place depack/flash verification is definitive.
    if len(encoded) >= count * FRAME_BYTES:
        raise ValueError(f"no meaningful compression: packed {len(encoded)}")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(encoded)
    if preview_out is not None:
        save_preview(frames, preview_out)
        print(f'PASS LCD original-composition RG DOT preview: {preview_out}')
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
    ap.add_argument("--preview", type=Path, default=None)
    a = ap.parse_args()
    convert(a.source, a.out, a.seek, a.preview)
