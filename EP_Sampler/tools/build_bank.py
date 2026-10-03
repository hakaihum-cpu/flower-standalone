#!/usr/bin/env python3
"""Build EPBANK1 from the 24 long capture WAV files.

Expected input names:
  EP_V016_RR1.wav ... EP_V127_RR3.wav

The timing comes from capture_manifest.csv generated with the MIDI kit.
Output is a single mmap-friendly epbank.bin consumed by the Android app.
"""
from __future__ import annotations
import argparse, csv, os, struct, sys, wave
from pathlib import Path

MAGIC = b"EPBANK1\0"
HEADER = struct.Struct("<8sIIIIIIQ")  # 40 bytes
ENTRY = struct.Struct("<BBBBIQQ")     # 24 bytes
VERSION = 1
PART_SUSTAIN = 0
PART_RELEASE = 1


def pcm24_to_pcm16(data: bytes) -> bytes:
    if len(data) % 3:
        raise ValueError("24-bit PCM byte count is not divisible by 3")
    out = bytearray((len(data)//3)*2)
    oi = 0
    for i in range(0, len(data), 3):
        # little-endian signed 24 bit -> signed 16 by dropping the low 8 bits
        v = data[i] | (data[i+1] << 8) | (data[i+2] << 16)
        if v & 0x800000:
            v -= 1 << 24
        s = max(-32768, min(32767, v >> 8))
        struct.pack_into('<h', out, oi, s)
        oi += 2
    return bytes(out)


def load_manifest(path: Path):
    with path.open('r', encoding='utf-8-sig', newline='') as f:
        rows = list(csv.DictReader(f))
    required = {"midi_file","expected_wav","midi_note","velocity","round_robin","note_on_sec","note_off_sec","slot_end_sec"}
    if not rows or not required.issubset(rows[0]):
        raise ValueError("Unexpected capture_manifest.csv format")
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('wav_dir', type=Path, help='Folder containing the 24 rendered WAV files')
    ap.add_argument('-m','--manifest', type=Path, default=Path(__file__).with_name('capture_manifest.csv'))
    ap.add_argument('-o','--output', type=Path, default=Path('epbank.bin'))
    ap.add_argument('--target-bits', type=int, choices=(16,24), default=24,
                    help='24 preserves the render; 16 reduces bank size by one third')
    ap.add_argument('--latency-ms', type=float, default=0.0,
                    help='Shift all slice points later/earlier if your DAW render has uncompensated latency')
    args = ap.parse_args()

    rows = load_manifest(args.manifest)
    # Each manifest row becomes SUSTAIN + RELEASE.
    entry_count = len(rows) * 2
    data_offset = HEADER.size + entry_count * ENTRY.size
    args.output.parent.mkdir(parents=True, exist_ok=True)

    by_wav = {}
    for r in rows:
        by_wav.setdefault(r['expected_wav'], []).append(r)

    missing = [name for name in by_wav if not (args.wav_dir / name).is_file()]
    if missing:
        print("Missing WAV files:", file=sys.stderr)
        for x in missing: print("  "+x, file=sys.stderr)
        return 2

    entries = []
    current_offset = data_offset
    sample_rate = None
    channels = None
    target_width = args.target_bits // 8

    with args.output.open('wb') as out:
        out.write(b'\0' * data_offset)
        for wav_name in sorted(by_wav):
            wav_path = args.wav_dir / wav_name
            with wave.open(str(wav_path), 'rb') as w:
                sr, ch, sw, comp = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getcomptype()
                if comp != 'NONE': raise ValueError(f"{wav_name}: compressed WAV is unsupported")
                if ch != 2: raise ValueError(f"{wav_name}: expected stereo, got {ch} channels")
                if sw not in (2,3): raise ValueError(f"{wav_name}: expected 16/24-bit PCM, got {sw*8} bit")
                if sample_rate is None: sample_rate, channels = sr, ch
                if sr != sample_rate or ch != channels: raise ValueError(f"{wav_name}: sample format differs from first WAV")
                if sr != 48000: print(f"WARNING: {wav_name} is {sr} Hz, capture spec recommends 48000 Hz", file=sys.stderr)

                for r in sorted(by_wav[wav_name], key=lambda x: int(x['midi_note'])):
                    note = int(r['midi_note']); vel = int(r['velocity']); rr = int(r['round_robin'])
                    shift = args.latency_ms / 1000.0
                    t0 = float(r['note_on_sec']) + shift
                    t1 = float(r['note_off_sec']) + shift
                    t2 = float(r['slot_end_sec']) + shift
                    for part, a, b in ((PART_SUSTAIN,t0,t1),(PART_RELEASE,t1,t2)):
                        start = max(0, round(a*sr)); end = max(start, round(b*sr)); frames = end-start
                        if end > w.getnframes():
                            raise ValueError(f"{wav_name}: slice exceeds WAV length at note {note}")
                        w.setpos(start)
                        raw = w.readframes(frames)
                        if sw == 3 and args.target_bits == 16:
                            raw = pcm24_to_pcm16(raw)
                        elif sw == 2 and args.target_bits == 24:
                            # Expand signed 16 to signed 24, preserving value exactly in top 16 bits.
                            src = memoryview(raw)
                            exp = bytearray((len(raw)//2)*3)
                            oi=0
                            for i in range(0,len(raw),2):
                                s = struct.unpack_from('<h',src,i)[0]
                                v = (s << 8) & 0xFFFFFF
                                exp[oi]=v&255; exp[oi+1]=(v>>8)&255; exp[oi+2]=(v>>16)&255; oi+=3
                            raw = bytes(exp)
                        out.write(raw)
                        entries.append((note,vel,rr,part,frames,current_offset,len(raw)))
                        current_offset += len(raw)

        out.seek(0)
        out.write(HEADER.pack(MAGIC, VERSION, sample_rate, channels, args.target_bits, len(entries), 0, data_offset))
        for e in entries:
            out.write(ENTRY.pack(*e))

    gib = args.output.stat().st_size / (1024**3)
    print(f"Built {args.output}")
    print(f"Entries: {len(entries)} ({len(entries)//2} captures x sustain/release)")
    print(f"Format: {sample_rate} Hz / stereo / {args.target_bits}-bit PCM")
    print(f"Size: {gib:.2f} GiB")
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
