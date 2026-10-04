# EP Sampler automatic capture workflow

This branch does not change the Android app or the EPBANK1 format. It only automates the source-audio capture stage and then calls the existing bank builder.

## Goal

Old workflow:
- import 24 MIDI files
- render 24 audio files manually
- build the BIN

New workflow:
- import one MASTER MIDI once when creating the reusable FL Studio template
- for each new instrument/preset, replace/select the VST sound in that template
- run one BAT
- FL Studio renders one long 24-bit FLAC
- the helper splits it back into the existing 24 logical captures
- byte-identical capture blocks are copied identically so the existing FLAC dedup builder can share their PCM region
- the existing build_bank_flac_dedup.py produces the normal EPBANK1 BIN

The Android reader and bank format are unchanged.

## First-time setup only

1. Install Python 3 and FFmpeg so python/py, ffmpeg and ffprobe are available in PATH.
2. Run:
   PREPARE_SAMPLING_TEMPLATE.bat
3. This creates:
   - MASTER_CAPTURE.mid
   - MASTER_CAPTURE.layout.json
4. In FL Studio, create a project with the target instrument channel.
5. Import MASTER_CAPTURE.mid into that instrument's Piano roll.
6. Put the resulting pattern at time 0 in the Playlist and save in SONG mode.
7. Set FL Studio render/audio settings to 48 kHz and FLAC 24-bit.
8. Save this project as SAMPLING_MASTER.flp.

The MASTER sequence contains all 8 velocity layers x 3 RR blocks. Each original 794-second capture block is preserved, with a 5-second gap between blocks to reduce cross-block tail contamination.

## Every later sample instrument

1. Duplicate/open SAMPLING_MASTER.flp.
2. Load the desired VST/preset.
3. Save the FLP under the instrument name.
4. Close FL Studio.
5. Run:
   AUTO_SAMPLE.bat "C:\path\to\InstrumentName.flp"

AUTO_SAMPLE.bat:
1. uses FL Studio's documented command-line render path to render FLAC;
2. validates 48 kHz / stereo / 24-bit;
3. splits the long render according to MASTER_CAPTURE.layout.json;
4. reconstructs the normal 24 capture filenames;
5. runs build_bank_flac_dedup.py;
6. prints the finished bank path and SHA-256.

FL_EXE can be set as an environment variable if FL Studio is installed outside the usual Image-Line path.

## Important limitation

FL Studio exposes command-line project rendering, but it does not expose a supported command-line API for injecting arbitrary MIDI into an already configured VST channel. For that reason the one-time creation of SAMPLING_MASTER.flp still needs one manual MIDI import. After that, new instruments do not need repeated MIDI import or manual audio export.
