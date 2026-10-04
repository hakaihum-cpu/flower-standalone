#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, shutil, subprocess, tempfile
from pathlib import Path

SR = 48000
CH = 2
BITS = 24
BPF = CH * (BITS // 8)

def probe(path: Path):
    p = subprocess.run([
        "ffprobe","-v","error","-select_streams","a:0",
        "-show_entries","stream=sample_rate,channels,bits_per_raw_sample",
        "-of","json",str(path)
    ], capture_output=True, text=True, check=True)
    s = json.loads(p.stdout)["streams"][0]
    sr = int(s["sample_rate"])
    ch = int(s["channels"])
    bits = int(s.get("bits_per_raw_sample") or 0)
    if (sr, ch, bits) != (SR, CH, BITS):
        raise ValueError(
            f"{path.name}: render must be 48kHz/24-bit/stereo, got {sr}Hz/{bits}bit/{ch}ch"
        )

def load_layout(path: Path):
    d = json.loads(path.read_text(encoding="utf-8"))
    blocks = d.get("blocks") or []
    if d.get("version") != 1 or not blocks:
        raise ValueError("Unexpected or empty MASTER_CAPTURE layout")
    prev_end = 0.0
    for b in blocks:
        for key in ("expected_wav","start_sec","duration_sec"):
            if key not in b:
                raise ValueError(f"Layout block missing {key}")
        if float(b["start_sec"]) < prev_end:
            raise ValueError("Overlapping/out-of-order layout blocks")
        prev_end = float(b["start_sec"]) + float(b["duration_sec"])
    return blocks

def encode_flac(raw: Path, out: Path):
    subprocess.run([
        "ffmpeg","-y","-v","error",
        "-f","s24le","-ar",str(SR),"-ac",str(CH),"-i",str(raw),
        "-c:a","flac","-compression_level","8",str(out)
    ], check=True)
    probe(out)

def read_exact_to_file(pipe, byte_count: int, path: Path, hasher):
    remaining = byte_count
    with path.open("wb") as f:
        while remaining:
            chunk = pipe.read(min(1024 * 1024, remaining))
            if not chunk:
                raise EOFError("Master render ended early")
            f.write(chunk)
            hasher.update(chunk)
            remaining -= len(chunk)

def discard_exact(pipe, byte_count: int):
    remaining = byte_count
    while remaining:
        chunk = pipe.read(min(1024 * 1024, remaining))
        if not chunk:
            raise EOFError("Master render ended early while skipping inter-block gap")
        remaining -= len(chunk)

def main() -> int:
    ap = argparse.ArgumentParser(description="Split one long FL Studio render using MASTER_CAPTURE.layout.json")
    ap.add_argument("master_audio", type=Path)
    ap.add_argument("-l", "--layout", type=Path, default=Path(__file__).with_name("MASTER_CAPTURE.layout.json"))
    ap.add_argument("-o", "--output-dir", type=Path, default=Path("capture_flac"))
    args = ap.parse_args()

    probe(args.master_audio)
    blocks = load_layout(args.layout)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    proc = subprocess.Popen([
        "ffmpeg","-v","error","-i",str(args.master_audio),"-map","0:a:0",
        "-f","s24le","-acodec","pcm_s24le","-ar",str(SR),"-ac",str(CH),"pipe:1"
    ], stdout=subprocess.PIPE)

    seen = {}
    cursor_frame = 0
    try:
        with tempfile.TemporaryDirectory(prefix="epcapture_") as td:
            td = Path(td)
            for index, b in enumerate(blocks, 1):
                start_frame = round(float(b["start_sec"]) * SR)
                frames = round(float(b["duration_sec"]) * SR)
                if start_frame < cursor_frame:
                    raise ValueError("Layout moved backwards")
                discard_exact(proc.stdout, (start_frame - cursor_frame) * BPF)
                cursor_frame = start_frame

                target = args.output_dir / Path(b["expected_wav"]).with_suffix(".flac").name
                raw = td / "block.raw"
                h = hashlib.sha256()
                read_exact_to_file(proc.stdout, frames * BPF, raw, h)
                cursor_frame += frames
                digest = h.hexdigest()

                if digest in seen:
                    shutil.copyfile(seen[digest], target)
                    kind = f"dedup copy of {seen[digest].name}"
                else:
                    encode_flac(raw, target)
                    seen[digest] = target
                    kind = "encoded unique block"
                print(f"[{index:02d}/{len(blocks):02d}] {target.name}: {kind}")
    finally:
        if proc.stdout:
            proc.stdout.close()
        rc = proc.wait()
        if rc != 0:
            raise RuntimeError(f"ffmpeg decode failed: {rc}")

    print(f"Split complete: {args.output_dir}")
    print(f"Logical captures: {len(blocks)}; unique PCM blocks: {len(seen)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
