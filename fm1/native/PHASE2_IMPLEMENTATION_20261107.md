# MIYAKO FM-1 Phase 2 (from the actually installed/booted Phase 1)

Date: 2026-11-07
Branch: feature/fm1-miyako-visual-arp-fx-20261107
Parent/baseline: feature/fm1-miyako-no-eureka-20261106
Status: **source implemented, CircleCI build and physical FM-1 tests NOT yet run**.

## User-directed changes

1. **SEL → VIDEO:** true media-only 240×240 two-pass LCD screen.
   The entire original 128×78 image is shown at 240×146 (aspect ratio
   preserved), centered vertically (y=47..192). The former
   `MIYAKO FM-1 / NATIVE PREVIEW`, `NOT FULL MIYAKO DSP`, and all
   unrelated parameter text are deleted from the application. The
   media-only page intentionally ignores K1–K4. HOME/FX restores controls.
2. **FX:** DREAMY now runs native finite-memory dual-grain DSP with
   GRAIN and SPACE knobs, in addition to REEL, DELAY, REVERB and CHORUS.
   Approximately 64 KiB additional `.pool` RAM for the history buffer;
   target memory size MUST be verified by the actual JieLi linker.
   DREAMY is *not* a byte/sample-identical JUCE/Android DSP reimplementation.
   All-source-fidelity multi-FX routing and additional MIYAKO effects remain
   separate future work. EUREKA is still deferred by the user's instruction.
3. **OSC:** SINE / SAW / SQUARE / PULSE names replace numerical wave IDs
   on the OSC and HOME parameter pages, using only existing platform fonts.
4. **ENV:** ADSR line graph plus ATK/DEC/SUS/REL parameters. This is
   **not just UI**: sample engine now applies attack, decay, sustain and
   release stages for all four synthesizer voices.
5. **LFO:** waveform line graph and four parameters:
   RATE (0–127), WAVE (SINE/TRI/SAW/SQUARE), DEPTH (0–127),
   DEST (PITCH/PWM/AMP). A sample-clocked LFO now modulates oscillator
   pitch, pulse width or amplitude.
6. **ARP:** sample-clocked held-note arpeggiator from both local touch
   keys and USB/TRS MIDI: MODE OFF/UP/DOWN/UPDN/RND,
   RATE 1/4/1/8/1/16/1/32, GATE 10..100%, OCT 1..4.
   The current CONFIG BPM remains the tempo source (40–240 BPM).
   It does not depend on the unimplemented SEQ/REC.
   MIDI/key note-off stops held notes; overlapping inputs have reference
   counts so one note-off cannot silence another source's held note.

## Compatibility and build contract

- Do not modify the approved Phase 1 branch or main.
- Version label 0.2.0-DEV, firmware ID FM-1_932; Phase 1 was FM-1_931.
  Both on-device compiled identity and .fwsc use the same builder patch.
  The previous successful installer/loader path stays unchanged.
- CircleCI: fm1_manual → fm1_native_build, run_build=true, manual ONLY.
  Do not trigger without user's explicit build permission.
- Inspect the host-generated MIYAKO_EFFECTS_VIDEO.png, MIYAKO_ENV.png,
  MIYAKO_LFO.png and MIYAKO_ARP.png, along with four-segment
  MIYAKO_FX_PREVIEW.wav (DREAMY added).
- Keep original 144 EFFECTS source frames and independent CRC.
  PSNR remains informational ONLY: never reintroduce a 26 dB build gate.
- All C host unit tests and the REAL JieLi target link, MMIO checks, pool
  memory and update package identity MUST pass before providing a FWSC.
- Device verification still required: boot, audio, controls, ADSR/LFO/ARP,
  FX tails, screen and flashing; do not imply it has occurred.
