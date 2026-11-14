#!/usr/bin/env python3
"""Experimental FM-1 SEL MHP2 lossless repack of approved 96x96 8x4 VQ.

Host-side encoder and verifier ONLY. Output is NOT an installable res.bin.
Never modifies the approved 628-frame VQ pixels and does not promise
better source fidelity. It reserves more room for a separately proven
source-derived correction layer.

Format: fixed 64-byte header, 24 RGB565 palettes, 24 pairs of 5-bit packed
canonical Huffman length tables, 24 one-byte map modes, 25 book offsets,
629 frame offsets, 24 compressed 4KB dictionaries, 628 independent maps.
Every block is independently byte-aligned; map frames are random-access.
All offsets in the header are absolute, directory values are relative to
the corresponding compressed stream start. All values little-endian.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import heapq
import json
import struct
import zlib
from pathlib import Path

MAGIC = b"MHP2"
VERSION = 1
HEADER = 64
CLIPS = 24
FRAMES = 628
WIDTH = HEIGHT = 96
PALETTE_BYTES = CLIPS * 16 * 2
TABLE_BYTES = 160  # 256 five-bit lengths
LENGTHS_BYTES = CLIPS * 2 * TABLE_BYTES
MODES_BYTES = CLIPS
BOOK_OFFSETS_BYTES = (CLIPS + 1) * 4
FRAME_OFFSETS_BYTES = (FRAMES + 1) * 4
BOOK_BYTES = 256 * 16
MAP_BYTES = 12 * 24
SOURCE_BYTES = 279936
SOURCE_SHA256 = "df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4"
RESOURCE_PAYLOAD_LIMIT = 69 * 4096 - 32
SOURCE_DIR = Path(__file__).resolve().parents[3] / "Resources/fm1_photo_96_chunks"


class Bits:
    def __init__(self):
        self.buf = bytearray()
        self.value = 0
        self.count = 0

    def put(self, value: int, count: int):
        if count < 0 or count > 31 or value < 0 or value >= 1 << count:
            raise ValueError("invalid bitfield")
        for i in range(count - 1, -1, -1):
            self.value = (self.value << 1) | ((value >> i) & 1)
            self.count += 1
            if self.count == 8:
                self.buf.append(self.value)
                self.value = self.count = 0

    def finish(self) -> bytes:
        if self.count:
            self.buf.append(self.value << (8 - self.count))
            self.value = self.count = 0
        return bytes(self.buf)


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def bit(self) -> int:
        if self.pos >= len(self.data) * 8:
            raise ValueError("truncated bitstream")
        value = (self.data[self.pos >> 3] >> (7 - (self.pos & 7))) & 1
        self.pos += 1
        return value


def huffman_lengths(data: bytes) -> list[int]:
    freq = [0] * 256
    for byte in data:
        freq[byte] += 1
    heap = []
    serial = 0
    for symbol, count in enumerate(freq):
        if count:
            heapq.heappush(heap, (count, symbol, serial, symbol))
            serial += 1
    if not heap:
        raise ValueError("empty Huffman alphabet")
    lengths = [0] * 256
    if len(heap) == 1:
        lengths[heap[0][1]] = 1
    else:
        while len(heap) > 1:
            a = heapq.heappop(heap)
            b = heapq.heappop(heap)
            node = (a[3], b[3])
            heapq.heappush(heap, (a[0] + b[0], min(a[1], b[1]), serial, node))
            serial += 1
        def visit(node, depth):
            if isinstance(node, int):
                lengths[node] = depth
            else:
                visit(node[0], depth + 1)
                visit(node[1], depth + 1)
        visit(heap[0][3], 0)
    # Maximum length 31 is mandatory for the compact lengths table and the C
    # reader. A full 8-bit alphabet remains valid if a pathological input
    # produces an over-deep tree.
    if max(lengths) > 31:
        return [8] * 256
    return lengths


def canonical(lengths: list[int]):
    if len(lengths) != 256 or not 0 < max(lengths) <= 31:
        raise ValueError("invalid code lengths")
    sorted_symbols = sorted((n, i) for i, n in enumerate(lengths) if n)
    codes = [(0, 0)] * 256
    decode = {}
    code = 0
    prev = 0
    for n, symbol in sorted_symbols:
        code <<= n - prev
        if code >= 1 << n:
            raise ValueError("oversubscribed Huffman tree")
        codes[symbol] = (code, n)
        decode[(n, code)] = symbol
        code += 1
        prev = n
    return codes, decode, max(lengths)


def write_codes(data: bytes, codes) -> bytes:
    bits = Bits()
    for b in data:
        c, n = codes[b]
        if not n:
            raise ValueError("symbol absent from Huffman table")
        bits.put(c, n)
    return bits.finish()


def read_symbol(reader: Reader, decoder: dict, maxlen: int) -> int:
    code = 0
    for n in range(1, maxlen + 1):
        code = (code << 1) | reader.bit()
        result = decoder.get((n, code))
        if result is not None:
            return result
    raise ValueError("invalid Huffman code")


def pack_lengths(lengths: list[int]) -> bytes:
    bits = Bits()
    for x in lengths:
        bits.put(x, 5)
    result = bits.finish()
    if len(result) != TABLE_BYTES:
        raise AssertionError("bad length table")
    return result


def unpack_lengths(data: bytes) -> list[int]:
    if len(data) != TABLE_BYTES:
        raise ValueError("bad length table size")
    r = Reader(data)
    return [sum(r.bit() << i for i in range(4, -1, -1)) for _ in range(256)]


def encode_map(frame: bytes, codes, mode: int) -> bytes:
    if len(frame) != MAP_BYTES:
        raise ValueError("invalid map size")
    bits = Bits()
    for at, b in enumerate(frame):
        if mode == 1 and at >= 12:
            same = b == frame[at - 12]
            bits.put(int(same), 1)
            if same:
                continue
        code, n = codes[b]
        bits.put(code, n)
    return bits.finish()


def decode_map(data: bytes, decoder: dict, maxlen: int, mode: int) -> bytes:
    bits = Reader(data)
    frame = bytearray(MAP_BYTES)
    for at in range(MAP_BYTES):
        if mode == 1 and at >= 12 and bits.bit():
            frame[at] = frame[at - 12]
        else:
            frame[at] = read_symbol(bits, decoder, maxlen)
    if bits.pos > len(data) * 8 or (len(data) - (bits.pos + 7) // 8):
        raise ValueError("map has unused whole bytes")
    return bytes(frame)


def pack(source: bytes):
    if len(source) != SOURCE_BYTES:
        raise ValueError("source must contain 628 exact VQ frames")
    palettes = source[:PALETTE_BYTES]
    books = source[PALETTE_BYTES:PALETTE_BYTES + CLIPS * BOOK_BYTES]
    maps = source[PALETTE_BYTES + CLIPS * BOOK_BYTES:]
    tables = bytearray()
    modes = bytearray()
    book_data = bytearray()
    map_data = bytearray()
    book_offsets = [0]
    frame_offsets = [0]
    chosen = []
    for clip in range(CLIPS):
        book = books[clip * BOOK_BYTES:(clip + 1) * BOOK_BYTES]
        start, end = clip * 27, min((clip + 1) * 27, FRAMES)
        group = [maps[f * MAP_BYTES:(f + 1) * MAP_BYTES] for f in range(start, end)]
        b_lengths = huffman_lengths(book)
        b_codes, _, _ = canonical(b_lengths)
        book_data.extend(write_codes(book, b_codes))
        book_offsets.append(len(book_data))
        candidates = []
        for mode in (0, 1):
            if mode == 0:
                symbols = b"".join(group)
            else:
                symbols = bytes(b for frame in group for at, b in enumerate(frame)
                                if at < 12 or b != frame[at - 12])
            lengths = huffman_lengths(symbols)
            codes, _, _ = canonical(lengths)
            encoded = [encode_map(frame, codes, mode) for frame in group]
            candidates.append((sum(map(len, encoded)), mode, lengths, encoded))
        _, mode, m_lengths, compressed = min(candidates, key=lambda x: (x[0], x[1]))
        tables.extend(pack_lengths(b_lengths))
        tables.extend(pack_lengths(m_lengths))
        modes.append(mode)
        for frame in compressed:
            map_data.extend(frame)
            frame_offsets.append(len(map_data))
        chosen.append({"clip": clip, "frames": end - start, "map_mode": "vertical" if mode else "plain",
                       "book_bytes": book_offsets[-1] - book_offsets[-2],
                       "map_bytes": sum(map(len, compressed))})
    offset = HEADER
    off_palette = offset; offset += PALETTE_BYTES
    off_lengths = offset; offset += LENGTHS_BYTES
    off_modes = offset; offset += MODES_BYTES
    off_book_offsets = offset; offset += BOOK_OFFSETS_BYTES
    off_frame_offsets = offset; offset += FRAME_OFFSETS_BYTES
    off_book_data = offset; offset += len(book_data)
    off_map_data = offset; offset += len(map_data)
    header = bytearray(HEADER)
    struct.pack_into("<4sHHIIHHHH", header, 0,
                     MAGIC, VERSION, 0, offset, 0, CLIPS, FRAMES, WIDTH, HEIGHT)
    struct.pack_into("<7I", header, 24, off_palette, off_lengths, off_modes,
                     off_book_offsets, off_frame_offsets, off_book_data, off_map_data)
    payload = (palettes + tables + modes +
               struct.pack("<" + "I" * len(book_offsets), *book_offsets) +
               struct.pack("<" + "I" * len(frame_offsets), *frame_offsets) +
               book_data + map_data)
    if HEADER + len(payload) != offset:
        raise AssertionError("layout calculation differs")
    struct.pack_into("<I", header, 12, zlib.crc32(payload))
    data = bytes(header) + payload
    report = {"format": "MHP2 v1 experimental, NOT res.bin", "raw_bytes": len(source),
              "compressed_bytes": len(data), "resource_limit": RESOURCE_PAYLOAD_LIMIT,
              "saved_bytes": len(source) - len(data),
              "room_for_source_corrections": RESOURCE_PAYLOAD_LIMIT - len(data),
              "books_bytes": len(book_data), "maps_bytes": len(map_data),
              "header_and_tables_bytes": off_book_data,
              "modes": {"vertical": sum(x["map_mode"] == "vertical" for x in chosen),
                        "plain": sum(x["map_mode"] == "plain" for x in chosen)},
              "per_clip": chosen}
    return data, report


def unpack(data: bytes, *, frame_count: int = FRAMES) -> bytes:
    """Read MHP2 archives with the 628-frame legacy or 645-frame border-corrected count.

    Encoding of the historical 628-frame archive remains unchanged.
    """
    if frame_count not in (628, 645):
        raise ValueError("unsupported MHP2 frame count")
    if len(data) < HEADER or data[:4] != MAGIC:
        raise ValueError("not MHP2")
    magic, ver, flags, declared, crc, clips, frames, w, h = struct.unpack_from("<4sHHIIHHHH", data)
    if ver != VERSION or flags or declared != len(data) or clips != CLIPS or frames != frame_count or (w,h) != (WIDTH, HEIGHT):
        raise ValueError("bad MHP2 header")
    if zlib.crc32(data[HEADER:]) != crc:
        raise ValueError("MHP2 checksum mismatch")
    o = struct.unpack_from("<7I", data, 24)
    expected = [HEADER, HEADER + PALETTE_BYTES,
                HEADER + PALETTE_BYTES + LENGTHS_BYTES,
                HEADER + PALETTE_BYTES + LENGTHS_BYTES + MODES_BYTES,
                HEADER + PALETTE_BYTES + LENGTHS_BYTES + MODES_BYTES + BOOK_OFFSETS_BYTES]
    if list(o[:5]) != expected or not (o[4] < o[5] <= o[6] <= len(data)):
        raise ValueError("MHP2 section boundary mismatch")
    palettes = data[o[0]:o[1]]
    modes = data[o[2]:o[3]]
    if any(x not in (0,1) for x in modes):
        raise ValueError("unsupported MHP2 map mode")
    books_index = struct.unpack_from("<" + "I" * (CLIPS + 1), data, o[3])
    frame_index = struct.unpack_from("<" + "I" * (frame_count + 1), data, o[4])
    if books_index[0] or frame_index[0] or books_index[-1] != o[6] - o[5] or frame_index[-1] != len(data) - o[6]:
        raise ValueError("MHP2 directory terminal offset invalid")
    if any(x > y for x,y in zip(books_index, books_index[1:])) or any(x > y for x,y in zip(frame_index, frame_index[1:])):
        raise ValueError("MHP2 directory not monotonic")
    book_results = []
    map_results = [None] * frame_count
    for clip in range(CLIPS):
        tab = o[1] + clip * 2 * TABLE_BYTES
        b_lengths = unpack_lengths(data[tab:tab + TABLE_BYTES])
        m_lengths = unpack_lengths(data[tab + TABLE_BYTES:tab + 2 * TABLE_BYTES])
        _, b_dec, b_max = canonical(b_lengths)
        _, m_dec, m_max = canonical(m_lengths)
        a = o[5] + books_index[clip]
        b = o[5] + books_index[clip + 1]
        stream = Reader(data[a:b])
        book = bytes(read_symbol(stream, b_dec, b_max) for _ in range(BOOK_BYTES))
        if (stream.pos + 7) // 8 != b-a:
            raise ValueError("dictionary padding invalid")
        book_results.append(book)
        for f in range(clip * 27, min((clip + 1) * 27, frame_count)):
            a = o[6] + frame_index[f]
            b = o[6] + frame_index[f+1]
            map_results[f] = decode_map(data[a:b], m_dec, m_max, modes[clip])
    if any(x is None for x in map_results):
        raise AssertionError("missing decoded map")
    return palettes + b"".join(book_results) + b"".join(map_results)


def load_canonical():
    chunks = sorted(SOURCE_DIR.glob("part_????.b64"))
    if len(chunks) != 12:
        raise ValueError("require all 12 original, checked-in SEL VQ chunks")
    blob = base64.b64decode(b"".join(b"".join(p.read_bytes().split()) for p in chunks), validate=True)
    if hashlib.sha256(blob).hexdigest() != SOURCE_SHA256:
        raise ValueError("approved SEL VQ SHA mismatch")
    return blob


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True, help="experimental MHP2 payload, NOT installable")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    source = load_canonical()
    packed, report = pack(source)
    if unpack(packed) != source:
        raise AssertionError("MHP2 failed bit-exact 628-frame VQ roundtrip")
    # Corruption rejection is part of the source-only gate.
    broken = bytearray(packed)
    broken[-1] ^= 0x80
    try:
        unpack(bytes(broken))
    except ValueError:
        pass
    else:
        raise AssertionError("MHP2 accepted corrupted payload")
    if len(packed) > RESOURCE_PAYLOAD_LIMIT:
        raise RuntimeError("MHP2 exceeds physical res.bin payload window")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(packed)
    args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"PASS: all {FRAMES} maps and {CLIPS} books byte-exact; {len(source)} -> {len(packed)} B; "
          f"free after repack: {RESOURCE_PAYLOAD_LIMIT-len(packed)} B")
    print("NOT FLASHABLE. Does not improve the already-VQ image quality.")


if __name__ == "__main__":
    main()