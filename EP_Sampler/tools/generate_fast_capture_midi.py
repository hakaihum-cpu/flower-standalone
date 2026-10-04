#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, struct
from pathlib import Path

PPQ = 960
TEMPO_US_PER_QN = 1_000_000
START_NOTE = 36
END_NOTE = 108
NOTE_STEP = 3
CAPTURE_VELOCITIES = (16, 48, 80, 127)
PRE_GAP_SEC = 0.25
SUSTAIN_SEC = 3.0
RELEASE_SEC = 1.0
SLOT_SEC = PRE_GAP_SEC + SUSTAIN_SEC + RELEASE_SEC

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

def meta_event(kind: int, data: bytes) -> bytes:
    return bytes((0xFF, kind)) + vlq(len(data)) + data

def note_name(note: int) -> str:
    names = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    return f"{names[note % 12]}{note // 12 - 1}"

def main() -> int:
    ap = argparse.ArgumentParser(description="Generate the short C2-C8 EPBANK capture MIDI.")
    ap.add_argument("-o", "--output", type=Path, default=Path("FAST_CAPTURE.mid"))
    ap.add_argument("--layout", type=Path, default=None)
    args = ap.parse_args()

    capture_notes = list(range(START_NOTE, END_NOTE + 1, NOTE_STEP))
    if capture_notes[-1] != END_NOTE:
        capture_notes.append(END_NOTE)

    events = []
    events.append((0, 0, meta_event(0x51, TEMPO_US_PER_QN.to_bytes(3, "big"))))
    events.append((0, 0, meta_event(0x58, bytes((4, 2, 24, 8)))))
    events.append((0, 0, meta_event(0x03, b"EPBANK FAST C2-C8")))

    captures = []
    cursor = 0.0
    for velocity in CAPTURE_VELOCITIES:
        for note in capture_notes:
            slot_start = cursor
            note_on = slot_start + PRE_GAP_SEC
            note_off = note_on + SUSTAIN_SEC
            slot_end = note_off + RELEASE_SEC
            marker = f"V{velocity:03d}_{note_name(note)}".encode("ascii")
            events.append((sec_to_tick(slot_start), 0, meta_event(0x06, marker)))
            events.append((sec_to_tick(note_on), 2, bytes((0x90, note, velocity))))
            events.append((sec_to_tick(note_off), 1, bytes((0x80, note, 0))))
            captures.append({
                "note": note,
                "note_name": note_name(note),
                "velocity": velocity,
                "slot_start_sec": round(slot_start, 6),
                "note_on_sec": round(note_on, 6),
                "note_off_sec": round(note_off, 6),
                "slot_end_sec": round(slot_end, 6),
            })
            cursor += SLOT_SEC

    end_tick = sec_to_tick(cursor)
    events.append((end_tick, 9, meta_event(0x2F, b"")))
    events.sort(key=lambda x: (x[0], x[1]))

    track = bytearray()
    prev = 0
    for tick, _, payload in events:
        track += vlq(tick - prev)
        track += payload
        prev = tick

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(
        b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ)
        + b"MTrk" + struct.pack(">I", len(track)) + track
    )

    layout_path = args.layout or args.output.with_suffix(".layout.json")
    layout = {
        "version": 2,
        "mode": "fast-c2-c8",
        "sample_rate": 48000,
        "channels": 2,
        "bits_per_sample": 24,
        "start_note": START_NOTE,
        "end_note": END_NOTE,
        "capture_note_step": NOTE_STEP,
        "capture_notes": capture_notes,
        "capture_velocities": list(CAPTURE_VELOCITIES),
        "target_velocities": [16, 32, 48, 64, 80, 96, 112, 127],
        "pre_gap_sec": PRE_GAP_SEC,
        "sustain_sec": SUSTAIN_SEC,
        "release_sec": RELEASE_SEC,
        "slot_sec": SLOT_SEC,
        "total_duration_sec": round(cursor, 6),
        "captures": captures,
    }
    layout_path.write_text(json.dumps(layout, indent=2) + "\n", encoding="utf-8")

    print(f"Generated: {args.output}")
    print(f"Layout: {layout_path}")
    print(f"Capture notes: {len(capture_notes)} ({note_name(capture_notes[0])}-{note_name(capture_notes[-1])})")
    print(f"Velocity layers captured: {len(CAPTURE_VELOCITIES)}")
    print(f"Total source notes: {len(captures)}")
    print(f"Duration: {cursor:.2f} sec ({int(cursor//60)}:{cursor%60:04.1f})")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
