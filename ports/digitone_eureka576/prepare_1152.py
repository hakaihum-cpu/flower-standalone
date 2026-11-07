#!/usr/bin/env python3
"""Combine *original* approved 576 video frames and 576 PG84 yellow-padded
user frames without changing a single source image pixel.

Only the XOR-delta/RLE wrapping is recomputed, so the bank boundary at 576
works with the stock shared framebuffer. Every sixth frame has an independent
1024-byte seek snapshot, exactly as in PG79/PG84.
"""
from pathlib import Path
import hashlib
import importlib.util

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("eureka_approved_04", ROOT / "prepare.py")
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)
SHA_OLD = "43cfac794ccd53d4f174806412773b7933537cc2928a21809dbd3fba983d64c2"
SHA_NEW = "bd7577db4d789f3a6b5bbae819cd6f5d5604eb4851d0dcf0d7f6a2ac67d4a411"
OLD = ROOT / "original_frames.bin"
NEW = ROOT / "frames.bin"

def prepare():
    source_old = OLD.read_bytes()
    source_new = NEW.read_bytes()
    assert hashlib.sha256(source_old).hexdigest() == SHA_OLD, "PG79 576 source changed"
    assert hashlib.sha256(source_new).hexdigest() == SHA_NEW, "PG84 576 source changed"
    frames_old = codec.decode(source_old, 576)
    frames_new = codec.decode(source_new, 576)
    assert len(frames_old) == len(frames_new) == 576
    assert codec.encode(frames_old)[0] == source_old
    assert codec.encode(frames_new)[0] == source_new
    combined = frames_old + frames_new
    assert len(combined) == 1152
    stream, modes = codec.encode(combined)
    assert codec.decode(stream, 1152) == combined
    seek = codec.build_seek(combined, stream)
    assert len(seek) == 192 * 1028
    for i in (0,35,575,576,577,611,1150,1151):
        assert combined[i] == (frames_old[i] if i < 576 else frames_new[i-576])
    assert combined[576:1152] == frames_new
    assert combined[:576] == frames_old
    assert len(stream) < 1152 * 1024
    (ROOT / "user_frames_before_merge.bin").write_bytes(source_new)
    (ROOT / "frames.bin").write_bytes(stream)
    (ROOT / "seek.bin").write_bytes(seek)
    (ROOT / "reference.bin").write_bytes(b"".join(combined))
    codec.save_preview(combined, ROOT / "eureka_lcd_preview.png")
    print("PASS 1152 exact original+user frames, 32 scenes x36, seek every 6")
    print(f"PASS packed={len(stream)}, seek={len(seek)}, raw={1152*1024}, modes={modes}")
    print(f"PASS image SHA256={hashlib.sha256(stream).hexdigest()}")
    print(f"PASS seek SHA256={hashlib.sha256(seek).hexdigest()}")
    return combined, stream, seek

if __name__ == "__main__":
    prepare()
