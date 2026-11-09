#!/usr/bin/env python3
"""Host-only cross-language validation of experimental MHU1. NEVER builds firmware.

Uses the checked-in original 12 SHA256-verified VQ chunks and requires a C
compiler. Independent freestanding C decoder runs through 628 frames in three
orders, comparing exact palettes, dictionaries, selectors and 96x96 pixels.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
import tempfile
from pack_miyako_resource import read_payload, CHUNK_DIR
from pack_photo_huffman_lossless_poc import pack, verify

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    original = read_payload(CHUNK_DIR)
    packed = pack(original)
    digest = verify(original, packed)
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        source = tmp/'source_vq.bin'
        candidate = tmp/'candidate.mhu1'
        executable = tmp/'test_mhu1'
        source.write_bytes(original)
        candidate.write_bytes(packed)
        cmd = [args.cc, '-std=c99', '-O1', '-Wall', '-Wextra',
               '-Werror', '-pedantic', '-g', '-I', str(ROOT)]
        if args.sanitize:
            cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cmd += [str(ROOT/'mi_hu_codec_poc.c'),
                str(ROOT/'test_mi_hu_codec_poc.c'), '-o', str(executable)]
        subprocess.run(cmd, check=True)
        subprocess.run([str(executable), str(source), str(candidate)], check=True)
    print(f'PASS Python+C exact 628-frame MHU1: '
          f'{len(original)} -> {len(packed)} B, {len(original)-len(packed)} B saved')
    print(f'Decoded palette/pixels SHA256: {digest}')

if __name__ == '__main__':
    main()
