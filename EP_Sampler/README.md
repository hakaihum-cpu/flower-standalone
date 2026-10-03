# EP Sampler Android — WAV-ready v0.1

This is the implementation stage immediately before inserting the recorded electric-piano material.
No Git/GitHub operation is part of this package.

## What is implemented

- Android native audio engine using AAudio (API 26+), low-latency/exclusive requested with shared fallback.
- 88-key bank range: MIDI 21–108.
- 8 velocity layers: 16, 32, 48, 64, 80, 96, 112, 127.
- 3 round robins per note.
- Velocity-layer interpolation.
- Separate sustain and release portions.
- CC64 sustain pedal.
- CC7 Volume and CC11 Expression.
- Poly Pressure and Channel Pressure. The first 250 ms attack remains on the original Note-On velocity; pressure then moves the BODY toward stronger velocity layers.
- Pitch Bend ±2 semitones.
- 64-voice pool with simple voice stealing.
- DREAMY is ON by default. It uses the established FLOWER/MIYAKO-style 2-voice micro-loop core: +5 and +12 semitone voices, ~2.5 s history, minimum ~0.75 s history, X drift and Y loop-length/wet control. Tap the DREAMY indicator to toggle it.
- MIDI input discovery through Android MidiManager, including running-status parsing. Dreamy X/Y also accept CC103/CC104, with the established initial value 0.28/0.28.
- UI based directly on the supplied piano image, with a deliberately small indicator strip instead of knobs/panels.
- Pressed-key overlay following MIDI Note.
- Two lightweight five-finger visual rigs. Their fingertip targets follow active MIDI notes; this is a visual overlay rather than an attempt to deform the photographed hands themselves.
- Runtime `epbank.bin` import. Tap `BANK —` to choose it. MIDI visualization still works when the bank is absent.

## UI behavior

The supplied image is shown without destructive cropping: it is fit inside the screen with black letterbox space where necessary. On a square 720×720 display this leaves useful quiet space for the tiny status indicators while keeping the full keyboard visible.

Top indicator examples:

`MIDI ●   BANK READY`

`VEL ━━━  AT ━  VOL ━  EXP ━  SUS ●`

`DREAMY ●` is at the upper right. There are no normal-size parameter knobs on the performance screen.

## WAV -> bank

Keep the 24 rendered WAVs from the capture MIDI exactly named. Put them in a folder, then run:

```bash
python tools/build_bank.py <wav-folder> -o epbank.bin --target-bits 24
```

The builder reads `tools/capture_manifest.csv`, splits every capture at the exact Note-On/Note-Off positions and creates both sustain and release entries. It does not normalize the audio.

`--target-bits 24` preserves the 24-bit capture. `--target-bits 16` is also supported if bank size becomes more important than retaining the original word length.

After `epbank.bin` exists, no code change is required: launch the app, tap the BANK indicator, and select the bank file.

## Important bank size note

The capture specification intentionally prioritizes realism: 88 keys × 8 velocities × 3 RR with 6 s sustain + 3 s release. With completely untrimmed 48 kHz stereo PCM the bank is large. The current runtime is deliberately mmap-based so the whole bank is not copied into RAM.

Once the real WAVs arrive, the next useful optimization is data-driven: measure the actual decay/noise floor per note and trim only inaudible tails. That should be done from the real renders rather than guessed now.

## Dreamy

Dreamy is enabled at startup and is implemented as the established FLOWER/MIYAKO-style micro-loop effect rather than a generic delay: two overlapping pitch-up voices at +5 semitones (`1.3348398x`) and +12 semitones (`2.0x`), ~2.5 seconds of history, and a ~0.75 second minimum history before wet audio appears. X controls the `0.90..1.00` drift region; Y shortens the micro-loop and raises wet amount using `clamp(0.24 + y*0.34, 0.20..0.58)`. X/Y start at `0.28 / 0.28` and accept MIDI CC103/104.

For the known Dreamy noise problem, the new standalone implementation keeps history continuously, smooths X/Y changes, overlaps grains with Hann windows instead of hard fragment switches, and low-passes the wet high end. No CHORD/PSOLA path is present in this app.

## Build status

The source structure is complete, but this environment does not contain an Android SDK/Gradle installation, so no APK build was executed here. No CI or Git service was invoked.

## 2026-10-03 FLAC processing update (v0.2)

The supplied captures are FLAC rather than WAV. `tools/build_bank_flac_dedup.py` builds EPBANK1 directly from them.
It also detects byte-identical capture files and lets multiple RR index entries share one PCM region, which is valid with the existing `SampleBank` runtime and avoids storing identical audio three times.

For this capture set, RR1/RR2/RR3 are byte-identical at every velocity. The resulting 24-bit bank is therefore 1.700 GiB instead of roughly three times that size while preserving the existing 3-RR lookup contract.

Build command:

```bash
python tools/build_bank_flac_dedup.py <flac-folder> -o epbank_24bit_dedup.bin
```

See `docs/FLAC_PROCESSING_REPORT.txt` for the measured timing and validation results.
