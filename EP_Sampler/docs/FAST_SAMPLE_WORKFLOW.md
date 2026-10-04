# EP Sampler FAST capture workflow (C2-C8)

This helper keeps the existing EPBANK1 reader unchanged and only shortens source capture.

## Capture specification

- Playable bank range: C2-C8 (MIDI 36-108)
- Physically rendered source notes: every 3 semitones, 25 notes total
- Captured velocities: 16 / 48 / 80 / 127
- Captured round robin: RR1 only
- Per captured note: 0.25 s pre-gap + 3.0 s sustain + 1.0 s release
- MASTER render length: 425 seconds = 7 min 05 sec

## Expansion performed by build_fast_bank.py

- Missing semitones are generated offline from the nearest captured source note.
- The nearest source is always at most 1 semitone away because the capture grid is every 3 semitones.
- Pitch conversion uses ffmpeg asetrate + aresample + atempo so the 4.0-second active segment keeps approximately the same duration.
- The app's existing 8 velocity slots are populated from the nearest of the four captured velocities.
- RR1/RR2/RR3 index entries share the same PCM. No fake audio variation is added.
- A0-B1 entries are intentionally absent. Existing EP-SAMPLE therefore plays C2-C8 only.

## First-time preparation

1. Put the tools in one folder.
2. Run PREPARE_FAST_TEMPLATE.bat.
3. Import FAST_CAPTURE.mid once into the desired instrument channel in FL Studio.
4. Put the pattern at the start of the Playlist and use SONG mode.
5. Set FL Studio to 48 kHz and FLAC 24-bit stereo export.
6. Save the project as a reusable template FLP.

## Each new instrument/preset

1. Open a copy of the prepared FLP.
2. Select the VST/preset to sample.
3. Save the FLP and close FL Studio.
4. Drag the FLP onto AUTO_SAMPLE_FAST.bat.
5. FL Studio renders the 7:05 MASTER sequence offline.
6. build_fast_bank.py expands it to the existing EPBANK1 format and writes the final BIN.

The Android EP-SAMPLE source is not modified by this workflow.
