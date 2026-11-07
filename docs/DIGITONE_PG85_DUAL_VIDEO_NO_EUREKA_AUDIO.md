# PG85 Digitone OS 1.44 — 1152 VIDEO / 3 AUDIO FX (trial)

## User-requested change

Remove the EUREKA **audio** processor completely; keep EUREKA **video** and all its approved MIDI/scene/page behavior. Preserve GRANULAR v0.5, REEL, BITCRUSHER, stock FM, stock MUTE/transport, stock chorus/delay/reverb. Do not create new visual art.

## Visual content and codec

- Frames 0..575 = **original approved PG79** EUREKA user video material, rebuilt from `Resources/eureka_frames.pack` with `prepare.py` and SHA-256 `43cfac794ccd53d4f174806412773b7933537cc2928a21809dbd3fba983d64c2` for its 576-frame stream.
- Frames 576..1151 = **PG84 user-supplied tiles**, 574 original cropped tiles plus two exact repeats, yellow letterbox padding retained (no stretching). Its 576-frame stream SHA-256 `bd7577db4d789f3a6b5bbae819cd6f5d5604eb4851d0dcf0d7f6a2ac67d4a411`.
- Exactly 32 scenes x 36 frames. Existing non-sequential random cuts, note triggering, video CC40/65/67/68/69, MIDI+YES toggle, MIDI+FUNC pages and 16 stock TRIG key events are unchanged except index range.
- Same original #04 HYBRID 128x64 monochrome conversion and existing XOR-delta/RLE codec. The two image sets are decoded, concatenated without any pixel edits, then recompressed as one stream; this is required to preserve XOR state at the 575/576 boundary.
- 192 seek checkpoints (one for every 6 frames), same max of five sequential delta records per random seek. 1152 decoded frames x 1024 bytes = 1,179,648 bytes, but **only one 1024-byte framebuffer in RAM**, with packed stream and 197,376 bytes of seek metadata.
- `prepare_1152.py`, `verify_1152.py` and native full-pixel decoder regression verify all 1152 frames and boundary positions 575,576,1151; source frames remain byte-for-byte identical.

## Audio removal

- Delete all EUREKA Haze DSP, 2MiB stereo capture region claim, ~16KiB local reverb and Haze tables.
- One CORE_AUDIO_INSERT owner remains for GRANULAR / REEL / BITCRUSHER. TRACK long-hold menu: GRANULAR, REEL, BITCRUSH, FX OFF. Native dedicated parameter screens for the three are unchanged (except removal of now-unused EUREKA SOUND/PLAY).
- CPU overrun guard stays active for REEL/BITCRUSHER, not for video-only EUREKA (video has never been part of audio callback).
- The original EUREKA visual engine still uses the exact same single-frame video decoder, scene randomization and supported event routing.
- Source branch `feature/digitone-1152-video-no-eureka-audio-20261107`, parent PG84. No changes to approved PG79, PG81, PG82, PG84, or main.

## Acceptance

1. Pin both source stream SHA256 values. Validate merged stream round-trip, each source bank's 576 full 1-bit frames, 192 snapshots, all 32 scenes, and all 1152 LCD renders. Inspect the four explicit 575/576 boundary directions.
2. GCC -Wconversion -Werror, native UBSan/ASan for three-FX menu/stock keys, DSP extremes, Reel LUT, granular original. Zero symbols of EUREKA AUDIO or 2MiB bulk allocation; one audio owner.
3. Link OS1.44 with pinned elekloader/core; enforce positive RAM/flash margins, no overlapping patches, firmware stock sections byte-exact, sysex checksum. New syx = `Digitone_OS1.44_1152_VIDEO_3FX_PG85_TRIAL.syx`.
4. Before making it the baseline, hardware test: image both banks, random cuts including new 16 scenes, MUTE/TRIG1-16/MIDI+YES, 3 FX, long-play stability.

## Status

This document describes the code and checks, not a passed on-device acceptance. Do not claim PG85 hardware stability from link success. Audio EUREKA has been removed at the binary source level.
