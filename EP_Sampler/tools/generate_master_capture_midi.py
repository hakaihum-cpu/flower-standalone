#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, json, struct
from collections import OrderedDict
from pathlib import Path

PPQ = 960
TEMPO_US_PER_QN = 1_000_000  # 60 BPM => 1 quarter note == 1 second

def vlq(value: int) -> bytes:
    if value < 0:
        raise ValueError("VLQ cannot encode negative values")
    buf = [value & 0x7F]
    value >>= 7
    while value:
        buf.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(buf))

def sec_to_tick(sec: float) -> int:
    return round(sec * PPQ)

def load_groups(manifest: Path):
    with manifest.open("r", encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    required = {"midi_file","expected_wav","midi_note","velocity","note_on_sec","note_off_sec","slot_end_sec"}
    if not rows or not required.issubset(rows[0]):
        raise ValueError("Unexpected capture manifest")
    groups = OrderedDict()
    for row in rows:
        groups.setdefault(row["midi_file"], []).append(row)
    return groups

def meta_event(kind: int, data: bytes) -> bytes:
    return bytes((0xFF, kind)) + vlq(len(data)) + data

def main() -> int:
    ap = argparse.ArgumentParser(description="Generate one long sampling MIDI from EP_Sampler capture_manifest.csv")
    ap.add_argument("-m", "--manifest", type=Path, default=Path(__file__).with_name("capture_manifest.csv"))
    ap.add_argument("-o", "--output", type=Path, default=Path("MASTER_CAPTURE.mid"))
    ap.add_argument("--layout", type=Path, default=None, help="Sidecar layout JSON path")
    ap.add_argument("--gap-sec", type=float, default=5.0, help="Silence inserted between capture blocks")
    args = ap.parse_args()

    if args.gap_sec < 0:
        raise ValueError("--gap-sec must be >= 0")

    groups = load_groups(args.manifest)
    events = []
    events.append((0, 0, meta_event(0x51, TEMPO_US_PER_QN.to_bytes(3, "big"))))
    events.append((0, 0, meta_event(0x58, bytes((4, 2, 24, 8)))))
    events.append((0, 0, meta_event(0x03, b"EP Sampler MASTER CAPTURE")))

    base_sec = 0.0
    layout_blocks = []
    group_items = list(groups.items())
    for group_index, (midi_name, rows) in enumerate(group_items):
        block_end = max(float(r["slot_end_sec"]) for r in rows)
        expected_wavs = {r["expected_wav"] for r in rows}
        if len(expected_wavs) != 1:
            raise ValueError(f"{midi_name}: expected one output name, got {expected_wavs}")
        expected_wav = next(iter(expected_wavs))
        layout_blocks.append({
            "midi_file": midi_name,
            "expected_wav": expected_wav,
            "start_sec": base_sec,
            "duration_sec": block_end,
        })
        label = Path(midi_name).stem.encode("ascii", errors="replace")
        events.append((sec_to_tick(base_sec), 0, meta_event(0x06, label)))
        for r in rows:
            note = int(r["midi_note"])
            velocity = int(r["velocity"])
            on_tick = sec_to_tick(base_sec + float(r["note_on_sec"]))
            off_tick = sec_to_tick(base_sec + float(r["note_off_sec"]))
            events.append((on_tick, 2, bytes((0x90, note, velocity))))
            events.append((off_tick, 1, bytes((0x80, note, 0))))
        base_sec += block_end
        if group_index + 1 < len(group_items):
            base_sec += args.gap_sec

    end_tick = sec_to_tick(base_sec)
    events.append((end_tick, 9, meta_event(0x2F, b"")))
    events.sort(key=lambda x: (x[0], x[1]))

    track = bytearray()
    prev = 0
    for tick, _, payload in events:
        track += vlq(tick - prev)
        track += payload
        prev = tick

    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ)
    body = b"MTrk" + struct.pack(">I", len(track)) + track
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(header + body)

    layout_path = args.layout or args.output.with_suffix(".layout.json")
    layout = {
        "version": 1,
        "ppq": PPQ,
        "tempo_bpm": 60,
        "inter_block_gap_sec": args.gap_sec,
        "total_duration_sec": base_sec,
        "blocks": layout_blocks,
    }
    layout_path.write_text(json.dumps(layout, indent=2) + "\n", encoding="utf-8")

    print(f"Generated: {args.output}")
    print(f"Layout: {layout_path}")
    print(f"Capture blocks: {len(groups)}")
    print(f"Total duration: {base_sec:.3f} sec ({base_sec/3600:.2f} h)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
