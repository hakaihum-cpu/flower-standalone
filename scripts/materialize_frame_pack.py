#!/usr/bin/env python3
from pathlib import Path
import base64
import hashlib
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# Existing 300-frame classroom bank.
CLASSROOM_CHUNK_DIR = ROOT / 'Resources' / 'frame_chunks'
CLASSROOM_OUT = ROOT / 'Resources' / 'classroom_frames.pack'
CLASSROOM_SHA256 = '1e888dbed69e259c52d2cb2bd192faa5c7f29dcf76eafcda9fdb2a2d03f2d658'
CLASSROOM_SIZE = 4464088

# Exact user-supplied EUREKA video sources. These remain the source of truth;
# CI extracts display-only JPEG frames and packs them into CRF1 for the JUCE UI.
EUREKA_SOURCE_DIR = ROOT / 'Resources' / 'eureka_video_sources'
EUREKA_OUT = ROOT / 'Resources' / 'eureka_frames.pack'
EUREKA_SOURCES = [
    ('clip_01.mp4', 627188, 'a48afddd2550d532502efa0b5f10f982d946fea0b83be208a70fa5e7e7f0626a'),
    ('clip_02.mp4', 749450, '5e307f8889b732256a8a6c0ca091a7838e8be9c1e65e9e4cfbf430f770fe3110'),
    ('clip_03.mp4', 655365, '3caaf1eb4f34becccafd44202d07e27a6f2b2656480a42185c236af1aff7510a'),
    ('clip_04.mp4', 694071, '3178cc26ac3edb7787cd6219cc5db73108b3442cfefe4acb32cc35af2adeca29'),
]
EUREKA_FPS = 24
EUREKA_FRAMES_PER_CLIP = 192
EUREKA_COUNT = len(EUREKA_SOURCES) * EUREKA_FRAMES_PER_CLIP

# User-selected launcher artwork.
ICON_B64 = ROOT / 'Resources' / 'effects_app_icon.b64'
ICON_OUT = ROOT / 'Resources' / 'effects_app_icon.jpg'
ICON_SHA256 = 'f4d9d841e5f4789588c53c8babcc849663911583907b259436e683c8268ee703'
ICON_SIZE = 27540


def decode_chunks(paths):
    decoded = bytearray()
    for path in paths:
        encoded = ''.join(path.read_text(encoding='ascii').split())
        decoded.extend(base64.b64decode(encoded, validate=True))
    return bytes(decoded)


def validate_crf1(data, expected_count, label):
    if data[:4] != b'CRF1':
        raise SystemExit(f'[FAIL] {label} magic mismatch')
    count = int.from_bytes(data[4:8], 'little')
    if count != expected_count:
        raise SystemExit(
            f'[FAIL] {label} frame count {count} != {expected_count}')


classroom_parts = sorted(CLASSROOM_CHUNK_DIR.glob('chunk_*.b64'))
if len(classroom_parts) != 13:
    raise SystemExit(
        f'[FAIL] expected 13 classroom frame chunks, got {len(classroom_parts)}')
classroom = decode_chunks(classroom_parts)
if len(classroom) != CLASSROOM_SIZE:
    raise SystemExit(
        f'[FAIL] reconstructed classroom pack size {len(classroom)} != {CLASSROOM_SIZE}')
classroom_sha = hashlib.sha256(classroom).hexdigest()
if classroom_sha != CLASSROOM_SHA256:
    raise SystemExit(
        f'[FAIL] reconstructed classroom pack sha256 {classroom_sha} != {CLASSROOM_SHA256}')
validate_crf1(classroom, 300, 'classroom frame pack')
CLASSROOM_OUT.write_bytes(classroom)

ffmpeg = shutil.which('ffmpeg')
if ffmpeg is None:
    raise SystemExit('[FAIL] ffmpeg is required to materialize EUREKA video frames')

for name, expected_size, expected_sha in EUREKA_SOURCES:
    source = EUREKA_SOURCE_DIR / name
    if not source.is_file():
        raise SystemExit(f'[FAIL] missing EUREKA video source: {source}')
    data = source.read_bytes()
    if len(data) != expected_size:
        raise SystemExit(
            f'[FAIL] EUREKA source {name} size {len(data)} != {expected_size}')
    sha = hashlib.sha256(data).hexdigest()
    if sha != expected_sha:
        raise SystemExit(
            f'[FAIL] EUREKA source {name} sha256 {sha} != {expected_sha}')
    if len(data) < 12 or data[4:8] != b'ftyp':
        raise SystemExit(f'[FAIL] EUREKA source {name} is not an MP4 file')

frame_payloads = []
with tempfile.TemporaryDirectory(prefix='eureka-video-frames-') as tmp:
    tmp_root = Path(tmp)
    for clip_index, (name, _, _) in enumerate(EUREKA_SOURCES):
        clip_dir = tmp_root / f'clip_{clip_index + 1:02d}'
        clip_dir.mkdir()
        pattern = clip_dir / '%04d.jpg'
        command = [
            ffmpeg, '-hide_banner', '-loglevel', 'error', '-y',
            '-i', str(EUREKA_SOURCE_DIR / name),
            '-vf', f'fps={EUREKA_FPS}',
            '-q:v', '8',
            str(pattern),
        ]
        subprocess.run(command, check=True)
        frame_files = sorted(clip_dir.glob('*.jpg'))
        if len(frame_files) != EUREKA_FRAMES_PER_CLIP:
            raise SystemExit(
                f'[FAIL] EUREKA {name} extracted {len(frame_files)} frames '
                f'!= {EUREKA_FRAMES_PER_CLIP}')
        for frame in frame_files:
            payload = frame.read_bytes()
            if not payload.startswith(b'\xff\xd8'):
                raise SystemExit(
                    f'[FAIL] EUREKA extracted frame is not JPEG: {frame}')
            frame_payloads.append(payload)

if len(frame_payloads) != EUREKA_COUNT:
    raise SystemExit(
        f'[FAIL] EUREKA total extracted frames {len(frame_payloads)} '
        f'!= {EUREKA_COUNT}')

table_size = 8 + EUREKA_COUNT * 12
offset = table_size
table = bytearray()
for payload in frame_payloads:
    table.extend(struct.pack('<QI', offset, len(payload)))
    offset += len(payload)

eureka = (
    b'CRF1'
    + struct.pack('<I', EUREKA_COUNT)
    + bytes(table)
    + b''.join(frame_payloads)
)
validate_crf1(eureka, EUREKA_COUNT, 'EUREKA video frame pack')
EUREKA_OUT.write_bytes(eureka)
eureka_sha = hashlib.sha256(eureka).hexdigest()

icon_encoded = ''.join(ICON_B64.read_text(encoding='ascii').split())
icon = base64.b64decode(icon_encoded, validate=True)
if len(icon) != ICON_SIZE:
    raise SystemExit(
        f'[FAIL] launcher icon size {len(icon)} != {ICON_SIZE}')
icon_sha = hashlib.sha256(icon).hexdigest()
if icon_sha != ICON_SHA256:
    raise SystemExit(
        f'[FAIL] launcher icon sha256 {icon_sha} != {ICON_SHA256}')
if not icon.startswith(b'\xff\xd8'):
    raise SystemExit('[FAIL] launcher icon is not JPEG')
ICON_OUT.write_bytes(icon)

print(
    f'[PASS] classroom pack: {len(classroom)} bytes sha256={classroom_sha}')
print(
    f'[PASS] EUREKA video pack: {len(eureka)} bytes / '
    f'{len(EUREKA_SOURCES)} clips / {EUREKA_COUNT} frames / '
    f'{EUREKA_FPS} fps sha256={eureka_sha}')
print(
    f'[PASS] launcher icon: {len(icon)} bytes sha256={icon_sha}')
