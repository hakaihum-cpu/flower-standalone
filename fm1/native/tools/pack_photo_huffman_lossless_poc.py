#!/usr/bin/env python3
"""Losslessly repack MIYAKO's *existing* 628 VQ frames. NOT an installable res.bin.

This experimental MHU1 resource preserves every decoded 96x96 VQ pixel;
it cannot restore detail already lost when the user's originals were VQ-encoded.
Standard Python library only. Does not alter firmware, flash map, or CI.
"""
from __future__ import annotations
import argparse, hashlib, heapq, json, random, struct, zlib
from pathlib import Path
from pack_miyako_resource import read_payload, CHUNK_DIR, SHA256

N, CLIPS, STEP, BOOK, MAP = 628, 24, 27, 4096, 288
CAP = 69 * 4096 - 32
HDR = struct.Struct('<4sHHHHIIIII')
DESC = struct.Struct('<8s128sBI')
PAL, CDIR = HDR.size, HDR.size + CLIPS * 32
FDIR, DATA = CDIR + CLIPS * DESC.size, CDIR + CLIPS * DESC.size + N * 4

def huffman(data: bytes, alphabet: int) -> list[int]:
    count = [0] * alphabet
    for c in data: count[c] += 1
    heap, serial = [], 0
    for s, n in enumerate(count):
        if n:
            heapq.heappush(heap, (n, serial, s))
            serial += 1
    if len(heap) == 1:
        out = [0] * alphabet
        out[heap[0][2]] = 1
        return out
    out = [0] * alphabet
    while len(heap) > 1:
        a, b = heapq.heappop(heap), heapq.heappop(heap)
        heapq.heappush(heap, (a[0] + b[0], serial, (a[2], b[2])))
        serial += 1
    def walk(node, depth):
        if isinstance(node, int): out[node] = max(1, depth)
        else:
            walk(node[0], depth + 1)
            walk(node[1], depth + 1)
    walk(heap[0][2], 0)
    return out

def canonical(lengths):
    code = prev = 0
    result = {}
    for bits, symbol in sorted((n, i) for i, n in enumerate(lengths) if n):
        if bits > 15: raise ValueError('Huffman length exceeds 15')
        code <<= bits - prev
        if code >= 1 << bits: raise ValueError('invalid Huffman tree')
        result[symbol] = (code, bits)
        code += 1
        prev = bits
    return result

def enc(data, lengths):
    table = canonical(lengths)
    result = bytearray()
    word = bits = 0
    for v in data:
        code, n = table[v]
        word, bits = (word << n) | code, bits + n
        while bits >= 8:
            bits -= 8
            result.append((word >> bits) & 255)
            word &= (1 << bits) - 1
    if bits: result.append((word << (8 - bits)) & 255)
    return bytes(result)

def dec(data, lengths, n):
    table = {(nbits, code): sym
             for sym, (code, nbits) in canonical(lengths).items()}
    result = bytearray()
    code = bits = 0
    for v in data:
        for bit in range(7, -1, -1):
            code = (code << 1) | ((v >> bit) & 1)
            bits += 1
            if bits > 15: raise ValueError('invalid Huffman bitstream')
            sym = table.get((bits, code))
            if sym is not None:
                result.append(sym)
                code = bits = 0
                if len(result) == n: return bytes(result)
    raise ValueError('truncated Huffman stream')

def packlens(ls):
    if max(ls) > 15: raise ValueError('length overflow')
    return bytes((ls[i] << 4) | ls[i + 1] for i in range(0, len(ls), 2))

def unpacklens(b):
    return [n for v in b for n in (v >> 4, v & 15)]

def framepixels(book, ids):
    out = bytearray(4608)
    for ty in range(24):
        for tx in range(12):
            k = ids[ty * 12 + tx] * 16
            for y in range(4):
                p = (ty * 4 + y) * 48 + tx * 4
                out[p:p+4] = book[k+y*4:k+y*4+4]
    return bytes(out)

def pack(source):
    if len(source) != 279936 or hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError('approved original VQ archive does not match SHA256')
    data, clips, frame_starts = bytearray(), bytearray(), []
    for c in range(CLIPS):
        n = min(STEP, N - c * STEP)
        book = source[768 + c*BOOK:768 + (c+1)*BOOK]
        maps = source[99072 + c*STEP*MAP:99072 + (c*STEP+n)*MAP]
        nib = bytes(x for v in book for x in (v >> 4, v & 15))
        bcodes = huffman(nib, 16)
        bpayload = enc(nib, bcodes) if max(bcodes) <= 15 else book
        rawbook = len(bpayload) >= BOOK
        if rawbook: bcodes, bpayload = [0] * 16, book
        mcodes = huffman(maps, 256)
        parts = [enc(maps[i*MAP:(i+1)*MAP], mcodes) for i in range(n)] if max(mcodes) <= 15 else []
        rawmaps = not parts or sum(len(x) for x in parts) >= len(maps)
        if rawmaps:
            mcodes = [0] * 256
            parts = [maps[i*MAP:(i+1)*MAP] for i in range(n)]
        clips.extend(DESC.pack(packlens(bcodes), packlens(mcodes),
                               rawbook | (rawmaps << 1), DATA + len(data)))
        data.extend(bpayload)
        for block in parts:
            frame_starts.append(DATA + len(data))
            data.extend(block)
    result = bytearray(DATA)
    result[PAL:CDIR] = source[:768]
    result[CDIR:FDIR] = clips
    for f, p in enumerate(frame_starts): struct.pack_into('<I', result, FDIR + 4*f, p)
    result.extend(data)
    if len(result) > CAP: raise ValueError('resource capacity exceeded')
    HDR.pack_into(result, 0, b'MHU1', 1, N, CLIPS, STEP, PAL, CDIR, FDIR,
                  DATA, zlib.crc32(result[HDR.size:]))
    return bytes(result)

def unpack(blob):
    if not DATA <= len(blob) <= CAP: raise ValueError('invalid payload length')
    if HDR.unpack_from(blob) != (b'MHU1', 1, N, CLIPS, STEP, PAL, CDIR, FDIR,
                                DATA, zlib.crc32(blob[HDR.size:])):
        raise ValueError('invalid header or CRC')
    clips = [DESC.unpack_from(blob, CDIR + i*DESC.size) for i in range(CLIPS)]
    offsets = [struct.unpack_from('<I', blob, FDIR + f*4)[0] for f in range(N)]
    for c, (_, _, flags, bookstart) in enumerate(clips):
        if flags & ~3 or not DATA <= bookstart < offsets[c*STEP]:
            raise ValueError('invalid dictionary offset')
    if any(a >= b for a, b in zip(offsets, offsets[1:])):
        raise ValueError('non-monotonic frame offsets')
    books = {}
    for f in range(N):
        c = f // STEP
        blens, mlens, flags, bookstart = clips[c]
        if c not in books:
            bookend = offsets[c*STEP]
            raw = blob[bookstart:bookend]
            if flags & 1: book = raw
            else:
                nibbles = dec(raw, unpacklens(blens), BOOK*2)
                book = bytes(nibbles[i] << 4 | nibbles[i+1]
                             for i in range(0, len(nibbles), 2))
            if len(book) != BOOK: raise ValueError('invalid book length')
            books[c] = book
        start = offsets[f]
        if f == N-1: end = len(blob)
        elif (f+1) % STEP == 0: end = clips[c+1][3]
        else: end = offsets[f+1]
        if not DATA <= start < end <= len(blob): raise ValueError('frame bounds')
        raw = blob[start:end]
        ids = raw if flags & 2 else dec(raw, unpacklens(mlens), MAP)
        if len(ids) != MAP: raise ValueError('invalid frame map')
        yield f, blob[PAL+c*32:PAL+(c+1)*32], books[c], ids

def verify(source, candidate):
    digest = hashlib.sha256()
    recovered = list(unpack(candidate))
    if len(recovered) != N: raise AssertionError('lost frames')
    for order in (range(N), random.Random(0x4d49).sample(range(N), N), range(N-1,-1,-1)):
        for f in order:
            _, palette, book, selectors = recovered[f]
            c = f // STEP
            old = (source[c*32:(c+1)*32],
                   source[768+c*BOOK:768+(c+1)*BOOK],
                   source[99072+f*MAP:99072+(f+1)*MAP])
            if (palette, book, selectors) != old or framepixels(book, selectors) != framepixels(old[1], old[2]):
                raise AssertionError(f'lossy frame {f}')
            if order == range(N):
                digest.update(palette); digest.update(framepixels(book, selectors))
    for change in ('header', 'offset', 'truncated'):
        bad = bytearray(candidate)
        if change == 'header': bad[0] ^= 1
        elif change == 'offset': struct.pack_into('<I', bad, FDIR, 0xffffffff)
        else: bad.pop()
        try: list(unpack(bytes(bad)))
        except ValueError: pass
        else: raise AssertionError('accepted corrupted ' + change)
    return digest.hexdigest()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--report', type=Path)
    args = ap.parse_args()
    if args.out.suffix != '.mhu1':
        raise SystemExit('output must be .mhu1; NEVER install as res.bin/.fwsc')
    source = read_payload(CHUNK_DIR)
    blob = pack(source)
    digest = verify(source, blob)
    report = {'status':'PASS', 'format':'MHU1 offline prototype',
              'frames':N, 'source_bytes':len(source), 'candidate_bytes':len(blob),
              'saved_bytes':len(source)-len(blob), 'resource_limit':CAP,
              'free_bytes':CAP-len(blob), 'source_sha256':hashlib.sha256(source).hexdigest(),
              'candidate_sha256':hashlib.sha256(blob).hexdigest(),
              'decoded_palette_pixels_sha256':digest,
              'verified':'628 forward/shuffled/reverse + invalid header/offset/truncated',
              'quality':'bit-exact relative to approved VQ, NOT original pre-VQ images',
              'installable':False, 'firmware_build':False}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(blob)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
    print(json.dumps(report,indent=2,ensure_ascii=False))

if __name__ == '__main__': main()
