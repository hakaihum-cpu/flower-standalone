#!/usr/bin/env python3
from pathlib import Path
import base64
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[1]

# Existing 300-frame classroom bank.
CLASSROOM_CHUNK_DIR = ROOT / 'Resources' / 'frame_chunks'
CLASSROOM_OUT = ROOT / 'Resources' / 'classroom_frames.pack'
CLASSROOM_SHA256 = '1e888dbed69e259c52d2cb2bd192faa5c7f29dcf76eafcda9fdb2a2d03f2d658'
CLASSROOM_SIZE = 4464088

# Original approved four EUREKA videos, 8 seconds / 36 JPEG frames each.
# Their immutable bytes are kept intact, then seven more user MP4 sources are
# appended in the same clip order and frame format (11 x 36 = 396 frames).
EUREKA_CHUNK_DIR = ROOT / 'Resources' / 'eureka_frame_chunks'
EUREKA_OUT = ROOT / 'Resources' / 'eureka_frames.pack'
EUREKA_SHA256 = '827b5aa6a339927cb909a5ffe1b129513d946911cc78ea96d34aa110e6cd1883'
EUREKA_SIZE = 1872671
EUREKA_COUNT = 144
EUREKA_ADDED_DIR = ROOT / 'Resources' / 'eureka_added_frame_chunks'
EUREKA_ADDED_SIZES = ROOT / 'Resources' / 'eureka_added_frame_sizes.txt'
EUREKA_ADDED_SOURCES = ROOT / 'Resources' / 'eureka_added_sources.txt'
EUREKA_ADDED_PARTS = 30
EUREKA_ADDED_COUNT = 7 * 36
EUREKA_ADDED_SIZE = 2475138
EUREKA_ADDED_SHA256 = 'be0772700a74386c87815f53949e7e925b36e93259397ff282ac300bf6eb0b0b'
EUREKA_FINAL_COUNT = EUREKA_COUNT + EUREKA_ADDED_COUNT
EUREKA_FINAL_SIZE = EUREKA_SIZE + EUREKA_ADDED_SIZE + EUREKA_ADDED_COUNT * 12

# Five more supplied 8-second clips; 36 ordered JPEGs per clip. 16 clips total.
# These are APPENDED to the earlier 11 clips, without transcoding old sources.
EUREKA_EXTRA_DIR = ROOT / 'Resources' / 'eureka_more_frame_chunks'
EUREKA_EXTRA_SIZES = ROOT / 'Resources' / 'eureka_more_frame_sizes.txt'
EUREKA_EXTRA_SOURCES = ROOT / 'Resources' / 'eureka_more_sources.txt'
EUREKA_EXTRA_PARTS = 31
EUREKA_EXTRA_COUNT = 5 * 36
EUREKA_EXTRA_SIZE = 2496161
EUREKA_EXTRA_SHA256 = 'eea7aae42c68680718cbb236c625f164b405f551a13ba1926e5a6b98d96eb27e'
# Replace only extra clip slots 1 and 2 (global clips 12, 13 of 16).
# Square source videos 1.mp4 / 2.mp4 (1080x1080) are directly downscaled
# to 720x720 JPEG, with NO 640x360 letterbox or black side padding.
# The other 14 user videos, CRF1 format, and audio remain untouched.
EUREKA_SWAP_DIR = ROOT / 'Resources' / 'eureka_replacement_frame_chunks'
EUREKA_SWAP_SIZES = ROOT / 'Resources' / 'eureka_replacement_frame_sizes.txt'
EUREKA_SWAP_PARTS = 20
EUREKA_SWAP_COUNT = 2 * 36
EUREKA_SWAP_BYTES = 1613885
EUREKA_SWAP_SHA256 = 'fb95820d9cdd35b8ea4f197aa87a35af538dfd4421c05be691d58cd794eeff0d'
EUREKA_ORIGINAL_PORTRAIT_BYTES = 266088 + 288551
EUREKA_SWAPPED_EXTRA_SIZE = EUREKA_EXTRA_SIZE - EUREKA_ORIGINAL_PORTRAIT_BYTES + EUREKA_SWAP_BYTES
EUREKA_COMPLETE_COUNT = EUREKA_FINAL_COUNT + EUREKA_EXTRA_COUNT
EUREKA_COMPLETE_SIZE = EUREKA_FINAL_SIZE + EUREKA_SWAPPED_EXTRA_SIZE + EUREKA_EXTRA_COUNT * 12

# 9 original user video clips, 8 seconds each at native 24 fps.
# Each original byte is stored in a deterministic Git blob chunk.
VONGON_CHUNK_DIR = ROOT / 'Resources' / 'vongon_frame_parts'
VONGON_OUT = ROOT / 'Resources' / 'vongon_frames.pack'
VONGON_SHA256 = '1de677ca54121e613375cbfcbb50852d9c64ee277357fed0d7498d36a0b6e131'
VONGON_SIZE = 12151319
VONGON_COUNT = 1728
VONGON_CHUNKS = 18
VONGON_CHUNK_SIZE = 691200

# User-selected launcher artwork.
ICON_B64 = ROOT / 'Resources' / 'effects_app_icon.b64'
ICON_OUT = ROOT / 'Resources' / 'effects_app_icon.jpg'
ICON_SHA256 = 'f4d9d841e5f4789588c53c8babcc849663911583907b259436e683c8268ee703'
ICON_SIZE = 27540
# MOOD MKII user-provided 343 JPEG frames. Reconstructed before Projucer
# generates BinaryData.cpp, using the same pinned frame-pack workflow.
MOOD_CHUNK_DIR = ROOT / 'Resources' / 'mood_frame_chunks'
MOOD_OUT = ROOT / 'Resources' / 'mood_frames.pack'
MOOD_COUNT = 343
MOOD_PARTS = 34
MOOD_SIZE = 2216566
MOOD_SHA256 = '035ef6cca6deff441b39ed51a9c3df9cbbe1b67548922bac1db3460a4f2438a4'
# The previously approved 343 frames stay byte-for-byte unchanged.
# Append the 9 x 10 user-supplied tile sheet as 90 JPEG frames.
MOOD_ADDED_DIR = ROOT / 'Resources' / 'mood_frame_additions'
MOOD_ADDED_SIZES = MOOD_ADDED_DIR / 'mood_append_sizes.txt'
MOOD_ADDED_PARTS = 16
MOOD_ADDED_COUNT = 90
MOOD_ADDED_BYTES = 568047
MOOD_ADDED_SHA256 = 'e22253104437247cb725be42872a75d91dd6b538f8de04332f3b6961047f1de5'
MOOD_FINAL_COUNT = MOOD_COUNT + MOOD_ADDED_COUNT
MOOD_FINAL_SIZE = 2785693
MOOD_FINAL_SHA256 = 'ab78e1ae79d3b7f812ded6e3f7c0295e617e2742fc22c315a9c80fe5273cdb77'



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


def jpeg_dimensions(data):
    # Lightweight SOF header check; no Pillow/ffmpeg CI dependency.
    if data[:2] != b'\xff\xd8':
        raise SystemExit('[FAIL] EUREKA replacement JPEG header invalid')
    index = 2
    while index + 4 <= len(data):
        if data[index] != 0xff:
            break
        while index < len(data) and data[index] == 0xff:
            index += 1
        if index >= len(data):
            break
        marker = data[index]
        index += 1
        if marker in (0xd8, 0x01) or 0xd0 <= marker <= 0xd7:
            continue
        if marker in (0xd9, 0xda):
            break
        segment_len = struct.unpack_from('>H', data, index)[0]
        if segment_len < 2 or index + segment_len > len(data):
            break
        if marker in (0xc0, 0xc1, 0xc2, 0xc3, 0xc5, 0xc6, 0xc7,
                      0xc9, 0xca, 0xcb, 0xcd, 0xce, 0xcf):
            h, w = struct.unpack_from('>HH', data, index + 3)
            return w, h
        index += segment_len
    raise SystemExit('[FAIL] EUREKA replacement JPEG size marker missing')


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

eureka_parts = sorted(EUREKA_CHUNK_DIR.glob('*.b64'))
if len(eureka_parts) != 21:
    raise SystemExit(
        f'[FAIL] expected 21 EUREKA frame chunks, got {len(eureka_parts)}')
eureka = decode_chunks(eureka_parts)
if len(eureka) != EUREKA_SIZE:
    raise SystemExit(
        f'[FAIL] reconstructed EUREKA pack size {len(eureka)} != {EUREKA_SIZE}')
eureka_sha = hashlib.sha256(eureka).hexdigest()
if eureka_sha != EUREKA_SHA256:
    raise SystemExit(
        f'[FAIL] reconstructed EUREKA pack sha256 {eureka_sha} != {EUREKA_SHA256}')
validate_crf1(eureka, EUREKA_COUNT, 'original EUREKA frame pack')

# Decode only newly supplied material; do not overwrite/re-encode any of the
# previously approved EUREKA images.
added_paths = sorted(EUREKA_ADDED_DIR.glob('part_*.b64'))
if len(added_paths) != EUREKA_ADDED_PARTS:
    raise SystemExit(
        f'[FAIL] expected {EUREKA_ADDED_PARTS} EUREKA added chunks, '
        f'got {len(added_paths)}')
added = decode_chunks(added_paths)
if len(added) != EUREKA_ADDED_SIZE:
    raise SystemExit(
        f'[FAIL] new EUREKA bytes {len(added)} != {EUREKA_ADDED_SIZE}')
added_sha = hashlib.sha256(added).hexdigest()
if added_sha != EUREKA_ADDED_SHA256:
    raise SystemExit(f'[FAIL] new EUREKA JPEG digest mismatch: {added_sha}')
sizes = [int(s) for s in EUREKA_ADDED_SIZES.read_text().splitlines() if s.strip()]
if len(sizes) != EUREKA_ADDED_COUNT or sum(sizes) != len(added):
    raise SystemExit('[FAIL] new EUREKA frame sizes or count mismatch')
source_lines = [
    x for x in EUREKA_ADDED_SOURCES.read_text(encoding='utf-8').splitlines()
    if x and not x.startswith('#')
]
if len(source_lines) != 7:
    raise SystemExit('[FAIL] new EUREKA source count mismatch')

# Validate 252 JPEG payload boundaries before constructing the CRF1 table.
position = 0
for size in sizes:
    frame = added[position:position + size]
    if size < 64 or frame[:2] != b'\xff\xd8' or frame[-2:] != b'\xff\xd9':
        raise SystemExit('[FAIL] new EUREKA JPEG frame boundary mismatch')
    position += size
if position != len(added):
    raise SystemExit('[FAIL] new EUREKA JPEG stream length mismatch')

old_header_size = 8 + EUREKA_COUNT * 12
new_header_size = 8 + EUREKA_FINAL_COUNT * 12
table_delta = new_header_size - old_header_size
new_entries = bytearray()

# Shift offsets only; keep the original 144 JPEG payloads untouched.
for index in range(EUREKA_COUNT):
    offset, flags, size = struct.unpack_from('<III', eureka, 8 + index * 12)
    if offset < old_header_size or offset + size > len(eureka):
        raise SystemExit(f'[FAIL] original EUREKA frame table {index} invalid')
    new_entries.extend(struct.pack('<III', offset + table_delta, flags, size))

cursor = new_header_size + len(eureka) - old_header_size
for size in sizes:
    new_entries.extend(struct.pack('<III', cursor, 0, size))
    cursor += size
eureka_merged = (
    b'CRF1' + struct.pack('<I', EUREKA_FINAL_COUNT)
    + new_entries + eureka[old_header_size:] + added
)
if len(eureka_merged) != EUREKA_FINAL_SIZE or cursor != len(eureka_merged):
    raise SystemExit('[FAIL] merged EUREKA frame pack size mismatch')
validate_crf1(eureka_merged, EUREKA_FINAL_COUNT, 'merged 11-clip EUREKA frame pack')

# Append five newly supplied clips to the exact previous 11-clip pack.
# This is an additive CRF1 index extension, not a new media pipeline.
extra_parts = sorted(EUREKA_EXTRA_DIR.glob('part_*.b64'))
if len(extra_parts) != EUREKA_EXTRA_PARTS:
    raise SystemExit(
        f'[FAIL] expected {EUREKA_EXTRA_PARTS} additional EUREKA chunks, '
        f'got {len(extra_parts)}')
extra = decode_chunks(extra_parts)
if len(extra) != EUREKA_EXTRA_SIZE:
    raise SystemExit(f'[FAIL] extra EUREKA data size mismatch: {len(extra)}')
if hashlib.sha256(extra).hexdigest() != EUREKA_EXTRA_SHA256:
    raise SystemExit('[FAIL] extra EUREKA JPEG digest mismatch')
extra_sizes = [int(x) for x in EUREKA_EXTRA_SIZES.read_text().splitlines() if x.strip()]
if len(extra_sizes) != EUREKA_EXTRA_COUNT or sum(extra_sizes) != len(extra):
    raise SystemExit('[FAIL] extra EUREKA frame index mismatch')
extra_sources = [
    x for x in EUREKA_EXTRA_SOURCES.read_text(encoding='utf-8').splitlines()
    if x and not x.startswith('#')
]
if len(extra_sources) != 5:
    raise SystemExit('[FAIL] extra EUREKA source count mismatch')
cursor = 0
for size in extra_sizes:
    frame = extra[cursor:cursor + size]
    if size < 64 or frame[:2] != b'\xff\xd8' or frame[-2:] != b'\xff\xd9':
        raise SystemExit('[FAIL] extra EUREKA JPEG frame boundary mismatch')
    cursor += size
if cursor != len(extra):
    raise SystemExit('[FAIL] extra EUREKA JPEG stream length mismatch')

# Both original 720x1280 portrait clips in the five-video addition are
# replaced with the user's newly supplied 1.mp4 and 2.mp4 (1080x1080).
# Assemble only at materialization: all other frames remain byte-identical.
swap_parts = sorted(EUREKA_SWAP_DIR.glob('part_*.b64'))
if len(swap_parts) != EUREKA_SWAP_PARTS:
    raise SystemExit(
        f'[FAIL] expected {EUREKA_SWAP_PARTS} portrait replacement parts, '
        f'got {len(swap_parts)}')
swapped = decode_chunks(swap_parts)
if len(swapped) != EUREKA_SWAP_BYTES:
    raise SystemExit('[FAIL] EUREKA replacement bytes incorrect')
if hashlib.sha256(swapped).hexdigest() != EUREKA_SWAP_SHA256:
    raise SystemExit('[FAIL] EUREKA replacement video SHA256 incorrect')
swap_sizes = [int(x) for x in EUREKA_SWAP_SIZES.read_text().splitlines() if x.strip()]
if len(swap_sizes) != EUREKA_SWAP_COUNT or sum(swap_sizes) != len(swapped):
    raise SystemExit('[FAIL] EUREKA replacement frame index invalid')
new_frames = []
new_sizes = []
read_offset = 0
swap_offset = 0
for frame_index, old_size in enumerate(extra_sizes):
    original_frame = extra[read_offset:read_offset + old_size]
    read_offset += old_size
    if 36 <= frame_index < 108:
        replacement_size = swap_sizes[frame_index - 36]
        frame = swapped[swap_offset:swap_offset + replacement_size]
        swap_offset += replacement_size
    else:
        frame = original_frame
    if len(frame) < 64 or frame[:2] != b'\xff\xd8' or frame[-2:] != b'\xff\xd9':
        raise SystemExit(f'[FAIL] EUREKA replacement JPEG invalid: {frame_index}')
    if 36 <= frame_index < 108 and jpeg_dimensions(frame) != (720, 720):
        raise SystemExit(
            f'[FAIL] EUREKA square slot has wrong frame aspect: {frame_index}')
    new_frames.append(frame)
    new_sizes.append(len(frame))
if read_offset != len(extra) or swap_offset != len(swapped):
    raise SystemExit('[FAIL] EUREKA replacement did not consume exact byte spans')
if sum(new_sizes) != EUREKA_SWAPPED_EXTRA_SIZE:
    raise SystemExit('[FAIL] EUREKA replacement total byte size incorrect')
extra = b''.join(new_frames)
extra_sizes = new_sizes

previous_header = 8 + EUREKA_FINAL_COUNT * 12
complete_header = 8 + EUREKA_COMPLETE_COUNT * 12
header_shift = complete_header - previous_header
complete_entries = bytearray()
for frame_index in range(EUREKA_FINAL_COUNT):
    offset, flags, size = struct.unpack_from(
        '<III', eureka_merged, 8 + frame_index * 12)
    if offset < previous_header or offset + size > len(eureka_merged):
        raise SystemExit(f'[FAIL] prior EUREKA frame index invalid: {frame_index}')
    complete_entries.extend(struct.pack('<III', offset + header_shift, flags, size))
cursor = complete_header + len(eureka_merged) - previous_header
for size in extra_sizes:
    complete_entries.extend(struct.pack('<III', cursor, 0, size))
    cursor += size
eureka_complete = (
    b'CRF1' + struct.pack('<I', EUREKA_COMPLETE_COUNT)
    + complete_entries + eureka_merged[previous_header:] + extra)
if len(eureka_complete) != EUREKA_COMPLETE_SIZE or cursor != len(eureka_complete):
    raise SystemExit('[FAIL] complete EUREKA frame pack size mismatch')
validate_crf1(eureka_complete, EUREKA_COMPLETE_COUNT, 'complete 16-clip EUREKA frame pack')
EUREKA_OUT.write_bytes(eureka_complete)

# Reconstruct and verify all 9 full-rate Vongon video clips before JUCER.
# Do not use the old 4.5-fps committed pack when building.
vongon_parts = sorted(VONGON_CHUNK_DIR.glob('part_*.bin'))
if len(vongon_parts) != VONGON_CHUNKS:
    raise SystemExit(
        f'[FAIL] expected {VONGON_CHUNKS} Vongon frame chunks, got {len(vongon_parts)}')
for part in vongon_parts[:-1]:
    if part.stat().st_size != VONGON_CHUNK_SIZE:
        raise SystemExit(f'[FAIL] Vongon chunk byte length mismatch: {part.name}')
vongon = b''.join(part.read_bytes() for part in vongon_parts)
if len(vongon) != VONGON_SIZE:
    raise SystemExit(
        f'[FAIL] reconstructed Vongon pack size {len(vongon)} != {VONGON_SIZE}')
vongon_sha = hashlib.sha256(vongon).hexdigest()
if vongon_sha != VONGON_SHA256:
    raise SystemExit(
        f'[FAIL] reconstructed Vongon pack sha256 {vongon_sha} != {VONGON_SHA256}')
validate_crf1(vongon, VONGON_COUNT, 'Vongon frame pack')
VONGON_OUT.write_bytes(vongon)


# Bundle MOOD source frames into the APK at build time.
mood_parts = sorted(MOOD_CHUNK_DIR.glob('mood_*.b64'))
if len(mood_parts) != MOOD_PARTS:
    raise SystemExit(f'[FAIL] expected {MOOD_PARTS} MOOD chunks, got {len(mood_parts)}')
mood = decode_chunks(mood_parts)
if len(mood) != MOOD_SIZE:
    raise SystemExit(f'[FAIL] MOOD pack size {len(mood)} != {MOOD_SIZE}')
mood_sha = hashlib.sha256(mood).hexdigest()
if mood_sha != MOOD_SHA256:
    raise SystemExit(f'[FAIL] MOOD pack sha256 mismatch: {mood_sha}')
validate_crf1(mood, MOOD_COUNT, 'MOOD frame pack')
# Verify the actual on-disk CRF1 table and the JPEG boundaries.
for frame_index in range(MOOD_COUNT):
    frame_offset, frame_flags, frame_size = struct.unpack_from(
        '<III', mood, 8 + frame_index * 12)
    if frame_offset < 8 + MOOD_COUNT * 12 or frame_offset + frame_size > len(mood):
        raise SystemExit(f'[FAIL] MOOD frame bounds invalid at {frame_index}')
    frame = mood[frame_offset:frame_offset + frame_size]
    if frame[:2] != bytes((0xff, 0xd8)) or frame[-2:] != bytes((0xff, 0xd9)):
        raise SystemExit(f'[FAIL] MOOD JPEG frame invalid at {frame_index}')
# Deterministically append the new JPEG frames, preserving all original
# JPEG data and correcting only the original CRF1 offset table.
added_paths = sorted(MOOD_ADDED_DIR.glob('mood_add_*.b64'))
expected_names = [f'mood_add_{index:02}.b64' for index in range(MOOD_ADDED_PARTS)]
if [p.name for p in added_paths] != expected_names:
    raise SystemExit('[FAIL] MOOD 433-frame asset chunks are missing or misordered')
added = decode_chunks(added_paths)
if len(added) != MOOD_ADDED_BYTES:
    raise SystemExit('[FAIL] MOOD appended JPEG data length mismatch')
if hashlib.sha256(added).hexdigest() != MOOD_ADDED_SHA256:
    raise SystemExit('[FAIL] MOOD appended JPEG data sha256 mismatch')
sizes = [int(x) for x in MOOD_ADDED_SIZES.read_text(encoding='ascii').split()]
if len(sizes) != MOOD_ADDED_COUNT or sum(sizes) != len(added):
    raise SystemExit('[FAIL] MOOD appended 90 JPEG sizes mismatch')
offset = 0
for index, size in enumerate(sizes):
    frame = added[offset:offset + size]
    if size < 64 or frame[:2] != bytes((255, 216)) or frame[-2:] != bytes((255, 217)):
        raise SystemExit(f'[FAIL] MOOD appended JPEG invalid at {index}')
    offset += size
old_header_size = 8 + MOOD_COUNT * 12
new_header_size = 8 + MOOD_FINAL_COUNT * 12
delta = new_header_size - old_header_size
entries = bytearray()
for index in range(MOOD_COUNT):
    old_offset, flags, size = struct.unpack_from('<III', mood, 8 + index * 12)
    if old_offset < old_header_size or old_offset + size > len(mood):
        raise SystemExit(f'[FAIL] original MOOD JPEG invalid at {index}')
    entries.extend(struct.pack('<III', old_offset + delta, flags, size))
cursor = len(mood) + delta
for size in sizes:
    entries.extend(struct.pack('<III', cursor, 0, size))
    cursor += size
mood_combined = (
    b'CRF1' + struct.pack('<I', MOOD_FINAL_COUNT)
    + entries + mood[old_header_size:] + added
)
if cursor != len(mood_combined) or len(mood_combined) != MOOD_FINAL_SIZE:
    raise SystemExit('[FAIL] MOOD 433-frame pack length mismatch')
combined_sha = hashlib.sha256(mood_combined).hexdigest()
if combined_sha != MOOD_FINAL_SHA256:
    raise SystemExit('[FAIL] MOOD 433-frame pack sha256 mismatch')
validate_crf1(mood_combined, MOOD_FINAL_COUNT, 'MOOD 433-frame pack')
for frame_index in range(MOOD_FINAL_COUNT):
    frame_offset, flags, frame_size = struct.unpack_from(
        '<III', mood_combined, 8 + frame_index * 12)
    if flags != 0 or frame_offset < new_header_size or frame_offset + frame_size > len(mood_combined):
        raise SystemExit(f'[FAIL] MOOD combined frame bounds invalid at {frame_index}')
    frame = mood_combined[frame_offset:frame_offset + frame_size]
    if frame[:2] != bytes((255, 216)) or frame[-2:] != bytes((255, 217)):
        raise SystemExit(f'[FAIL] MOOD combined JPEG invalid at {frame_index}')
MOOD_OUT.write_bytes(mood_combined)

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
    f'[PASS] EUREKA pack: {len(eureka_complete)} bytes / {EUREKA_COMPLETE_COUNT} frames sha256={hashlib.sha256(eureka_complete).hexdigest()}')
print(
    f'[PASS] launcher icon: {len(icon)} bytes sha256={icon_sha}')
print(
    f'[PASS] Vongon pack: {len(vongon)} bytes / {VONGON_COUNT} frames sha256={vongon_sha}')
print(f'[PASS] MOOD pack: {len(mood_combined)} bytes / {MOOD_FINAL_COUNT} frames sha256={combined_sha}')
