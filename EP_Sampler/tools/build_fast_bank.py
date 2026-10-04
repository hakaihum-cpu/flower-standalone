#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, math, struct, subprocess, tempfile
from pathlib import Path

MAGIC = b'EPBANK1\0'
HEADER = struct.Struct('<8sIIIIIIQ')
ENTRY = struct.Struct('<BBBBIQQ')
VERSION = 1
SUSTAIN = 0
RELEASE = 1
SR = 48000
CH = 2
BITS = 24
BPF = CH * (BITS // 8)
TARGET_VELS = (16, 32, 48, 64, 80, 96, 112, 127)
RR_VALUES = (1, 2, 3)

def sha256(path: Path, block: int = 1024 * 1024) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        while True:
            b = f.read(block)
            if not b:
                break
            h.update(b)
    return h.hexdigest()

def probe(path: Path) -> tuple[int, int, int]:
    p = subprocess.run([
        'ffprobe', '-v', 'error', '-select_streams', 'a:0',
        '-show_entries', 'stream=sample_rate,channels,bits_per_raw_sample',
        '-of', 'json', str(path)
    ], capture_output=True, text=True, check=True)
    s = json.loads(p.stdout)['streams'][0]
    sr = int(s['sample_rate'])
    ch = int(s['channels'])
    bits = int(s.get('bits_per_raw_sample') or 0)
    if sr != SR or ch != CH or bits not in (16, 24):
        raise ValueError(f'{path.name}: expected 48kHz/stereo FLAC at 16-bit or 24-bit, got {sr}Hz/{bits}bit/{ch}ch')
    return sr, ch, bits

def load_layout(path: Path) -> dict:
    d = json.loads(path.read_text(encoding='utf-8'))
    if d.get('version') != 2 or d.get('mode') != 'fast-c2-c8':
        raise ValueError('Unexpected FAST_CAPTURE layout')
    if (int(d.get('sample_rate', 0)), int(d.get('channels', 0)), int(d.get('bits_per_sample', 0))) != (SR, CH, BITS):
        raise ValueError('FAST_CAPTURE layout format is not 48kHz/24-bit/stereo')
    captures = d.get('captures') or []
    if not captures:
        raise ValueError('FAST_CAPTURE layout has no captures')
    expected = len(d['capture_notes']) * len(d['capture_velocities'])
    if len(captures) != expected:
        raise ValueError(f'FAST_CAPTURE capture count {len(captures)} != expected {expected}')
    return d

def nearest(value: int, choices: list[int]) -> int:
    return min(choices, key=lambda x: (abs(x - value), x))

def decode_master(master: Path, raw: Path) -> None:
    subprocess.run([
        'ffmpeg', '-y', '-v', 'error', '-i', str(master), '-map', '0:a:0',
        '-f', 's24le', '-acodec', 'pcm_s24le', '-ar', str(SR), '-ac', str(CH), str(raw)
    ], check=True)

def read_exact_region(raw_file, offset: int, byte_count: int) -> bytes:
    raw_file.seek(offset)
    data = raw_file.read(byte_count)
    if len(data) != byte_count:
        raise EOFError(f'Raw master ended early: requested {byte_count}, got {len(data)}')
    return data

def pitch_shift_region(raw_path: Path, start_sec: float, duration_sec: float, semitones: int) -> bytes:
    factor = 2.0 ** (semitones / 12.0)
    tempo = 1.0 / factor
    filt = (
        f'asetrate={SR}*{factor:.12f},'
        f'aresample={SR},'
        f'atempo={tempo:.12f},'
        f'apad=pad_dur={duration_sec:.9f},'
        f'atrim=duration={duration_sec:.9f}'
    )
    p = subprocess.run([
        'ffmpeg', '-v', 'error',
        '-ss', f'{start_sec:.9f}',
        '-f', 's24le', '-ar', str(SR), '-ac', str(CH), '-i', str(raw_path),
        '-af', filt,
        '-f', 's24le', '-acodec', 'pcm_s24le', '-ar', str(SR), '-ac', str(CH), 'pipe:1'
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode != 0:
        raise RuntimeError(f'ffmpeg pitch shift failed ({semitones:+d} st): {p.stderr.decode(errors="replace")}')
    expected = round(duration_sec * SR) * BPF
    data = p.stdout
    if len(data) < expected:
        data += b'\0' * (expected - len(data))
    elif len(data) > expected:
        data = data[:expected]
    return data

def main() -> int:
    ap = argparse.ArgumentParser(description='Build a short-source EPBANK1 bank for the existing EP-SAMPLE app.')
    ap.add_argument('master_audio', type=Path, help='FL Studio FAST_CAPTURE render (48kHz/stereo FLAC, 16-bit or 24-bit)')
    ap.add_argument('-l', '--layout', type=Path, default=Path(__file__).with_name('FAST_CAPTURE.layout.json'))
    ap.add_argument('-o', '--output', type=Path, default=Path('fast_epbank.bin'))
    args = ap.parse_args()

    in_sr, in_ch, in_bits = probe(args.master_audio)
    print(f'Input render: {in_sr} Hz / {in_ch} ch / {in_bits}-bit FLAC')
    if in_bits == 16:
        print('Input is 16-bit; converting to 24-bit PCM internally for EPBANK1 generation.')
    layout = load_layout(args.layout)
    start_note = int(layout['start_note'])
    end_note = int(layout['end_note'])
    capture_notes = [int(x) for x in layout['capture_notes']]
    capture_vels = [int(x) for x in layout['capture_velocities']]
    sustain_sec = float(layout['sustain_sec'])
    release_sec = float(layout['release_sec'])
    active_sec = sustain_sec + release_sec
    sustain_frames = round(sustain_sec * SR)
    release_frames = round(release_sec * SR)
    active_frames = sustain_frames + release_frames
    active_bytes = active_frames * BPF
    sustain_bytes = sustain_frames * BPF
    release_bytes = release_frames * BPF

    cap_lookup: dict[tuple[int, int], dict] = {}
    for c in layout['captures']:
        cap_lookup[(int(c['note']), int(c['velocity']))] = c
    for n in capture_notes:
        for v in capture_vels:
            if (n, v) not in cap_lookup:
                raise ValueError(f'Missing capture in layout: note={n} velocity={v}')

    notes = list(range(start_note, end_note + 1))
    entry_count = len(notes) * len(TARGET_VELS) * len(RR_VALUES) * 2
    data_offset = HEADER.size + entry_count * ENTRY.size
    region_count = len(notes) * len(capture_vels)
    expected_size = data_offset + region_count * active_bytes

    def region_index(note: int, capture_vel: int) -> int:
        return (note - start_note) * len(capture_vels) + capture_vels.index(capture_vel)

    entries = []
    for note in notes:
        for vel in TARGET_VELS:
            source_vel = nearest(vel, capture_vels)
            base = data_offset + region_index(note, source_vel) * active_bytes
            for rr in RR_VALUES:
                entries.append((note, vel, rr, SUSTAIN, sustain_frames, base, sustain_bytes))
                entries.append((note, vel, rr, RELEASE, release_frames, base + sustain_bytes, release_bytes))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='epbank_fast_') as td:
        raw_path = Path(td) / 'master.s24le'
        print('Decoding master render once...')
        decode_master(args.master_audio, raw_path)
        needed_frames = math.ceil(float(layout['total_duration_sec']) * SR)
        needed_bytes = needed_frames * BPF
        actual_bytes = raw_path.stat().st_size
        if actual_bytes < needed_bytes:
            missing_bytes = needed_bytes - actual_bytes
            missing_sec = missing_bytes / float(BPF * SR)
            # FL Studio may stop the render at the final NoteOff and omit only
            # the final release tail. That is safe to zero-pad. A larger
            # shortage means the capture itself is incomplete and must fail.
            max_safe_shortfall = release_sec + 0.25
            if missing_sec <= max_safe_shortfall:
                print(f'Master render is {missing_sec:.3f} sec shorter than layout; padding final release tail with silence.')
                with raw_path.open('ab') as pad:
                    pad.write(b'\\0' * missing_bytes)
            else:
                actual_sec = actual_bytes / float(BPF * SR)
                expected_sec = needed_bytes / float(BPF * SR)
                raise ValueError(
                    f'Master render is too short: {actual_sec:.3f} sec, expected {expected_sec:.3f} sec '
                    f'(short by {missing_sec:.3f} sec)'
                )

        with args.output.open('wb') as out:
            out.write(HEADER.pack(MAGIC, VERSION, SR, CH, BITS, len(entries), 0, data_offset))
            for e in entries:
                out.write(ENTRY.pack(*e))
            if out.tell() != data_offset:
                raise AssertionError('index size mismatch')

            with raw_path.open('rb') as raw:
                done = 0
                for note in notes:
                    source_note = nearest(note, capture_notes)
                    semitones = note - source_note
                    for capture_vel in capture_vels:
                        c = cap_lookup[(source_note, capture_vel)]
                        start_sec = float(c['note_on_sec'])
                        if semitones == 0:
                            start_byte = round(start_sec * SR) * BPF
                            data = read_exact_region(raw, start_byte, active_bytes)
                        else:
                            data = pitch_shift_region(raw_path, start_sec, active_sec, semitones)
                        if len(data) != active_bytes:
                            raise AssertionError('generated region size mismatch')
                        out.write(data)
                        done += 1
                    if (note - start_note) % 6 == 0 or note == end_note:
                        print(f'Generated note {note}/{end_note} ({done}/{region_count} regions)')

    actual_size = args.output.stat().st_size
    if actual_size != expected_size:
        raise ValueError(f'Bank size mismatch: {actual_size} != {expected_size}')

    with args.output.open('rb') as f:
        h = HEADER.unpack(f.read(HEADER.size))
        if h[0] != MAGIC or h[1] != VERSION or h[2:5] != (SR, CH, BITS) or h[5] != entry_count or h[7] != data_offset:
            raise ValueError('Header read-back validation failed')
        seen = set()
        for _ in range(entry_count):
            e = ENTRY.unpack(f.read(ENTRY.size))
            note, vel, rr, part, frames, off, byte_count = e
            if off + byte_count > actual_size:
                raise ValueError('Entry points beyond EOF')
            seen.add((note, vel, rr, part))
        expected_keys = {
            (n, v, rr, p)
            for n in notes for v in TARGET_VELS for rr in RR_VALUES for p in (SUSTAIN, RELEASE)
        }
        if seen != expected_keys:
            raise ValueError('Index completeness validation failed')

    print(f'Built: {args.output}')
    print(f'Playable range: MIDI {start_note}-{end_note} (C2-C8)')
    print(f'Captured notes: {len(capture_notes)}; generated target notes: {len(notes)}')
    print(f'Captured velocities: {capture_vels}; target velocity slots: {list(TARGET_VELS)}')
    print('RR1/RR2/RR3 share the same PCM by design.')
    print(f'Index entries: {entry_count}')
    print(f'Size: {actual_size / (1024**2):.1f} MiB')
    print(f'SHA-256: {sha256(args.output)}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
