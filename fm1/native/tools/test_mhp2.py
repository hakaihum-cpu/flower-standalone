#!/usr/bin/env python3
"""Synthetic host test; no original source files are modified or generated."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import random
import tempfile
import subprocess
import sys
import mhp2


def synth():
    rng = random.Random(5432)
    palettes = bytes(rng.randrange(256) for _ in range(mhp2.PALETTE_BYTES))
    books = bytearray()
    for clip in range(mhp2.CLIPS):
        samples = [rng.randrange(256) for _ in range(60)]
        books.extend(bytes(rng.choice(samples) for _ in range(mhp2.BOOK_BYTES)))
    maps = bytearray()
    for _ in range(mhp2.FRAMES):
        frame = bytearray()
        for pos in range(mhp2.MAP_BYTES):
            if pos >= 12 and rng.random() < .28:
                frame.append(frame[pos - 12])
            else:
                frame.append(rng.randrange(256))
        maps.extend(frame)
    return palettes + books + maps


def check(source, pair_dir):
    packed, report = mhp2.pack(source)
    assert mhp2.unpack(packed) == source
    broken = bytearray(packed)
    broken[-1] ^= 0x80
    try:
        mhp2.unpack(bytes(broken))
    except ValueError:
        pass
    else:
        raise AssertionError("MHP2 accepted bad CRC")
    assert len(packed) <= mhp2.RESOURCE_PAYLOAD_LIMIT
    (pair_dir / "reference.vq").write_bytes(source)
    (pair_dir / "encoded.mhp2").write_bytes(packed)
    print("HOST Python PASS", {k: v for k,v in report.items() if k != "per_clip"},
          "SHA", hashlib.sha256(source).hexdigest())


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, default=None, help="optional real 279936B VQ payload")
    p.add_argument("--repository-source", action="store_true",
                   help="load SHA256-approved current GitHub chunks in this checkout")
    p.add_argument("--pair-dir", type=Path, default=Path("/tmp/fm1_mhp2_test"))
    args = p.parse_args()
    args.pair_dir.mkdir(parents=True, exist_ok=True)
    if args.source and args.repository_source:
        raise SystemExit("select only one source mode")
    source = (mhp2.load_canonical() if args.repository_source else
              args.source.read_bytes() if args.source else synth())
    check(source, args.pair_dir)

if __name__ == "__main__":
    main()