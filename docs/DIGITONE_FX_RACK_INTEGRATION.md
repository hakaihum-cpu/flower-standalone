# Digitone Four-FX rack — implementation and build gate (2026-11-03)

## Baseline and delivery status

- Protected user-tested firmware: PG79, Digitone mk1 OS 1.44.
- Implementation branch: feature/digitone-fx-dedicated-pages-20261103.
- Four-FX module: ports/digitone_fx_rack.
- Source implementation is committed. Native GCC/UBSan, cross-compiler, elekloader lint, SYX packaging, and physical-instrument checks have NOT been executed.
- No firmware has been built and the existing approved PG79 binary has not been changed.
- The workflow remains workflow_dispatch only, restricted to this branch. Build requires a new explicit authorization.

## Audio ownership and menus

The built-in core-dn1 TRACK-hold Mod Menu supplies five choices: GRANULAR, EUREKA FX, REEL, BITCRUSH and FX OFF. The rack contributes one CORE_AUDIO_INSERT owner so original FM synthesis, input mixer, stock chorus/delay/reverb, sequencer and MUTE remain in the path. The previous standalone GRANULAR module is intentionally not linked in the next candidate firmware: it is integrated in the rack from its 0.5 source. Only one rack audio engine is selected; no second render owner is enabled.

When changing modes, the selected wet signal is faded to stock audio over 720 samples (~15 ms at 48 kHz), then the insert owner is disabled and the new source is reset in the UI context after a short guard. On entry, the new insert fades back in. Timing is a software design; click-free operation requires a hardware test.

The EUREKA 576-frame user video remains a separate module, with MIDI+YES and its own six video parameter pages. The rack's EUREKA entry is a distinct AUDIO processor. The rack's native audio parameter pages overlay video only while open; NO closes the page while leaving the chosen effect active. TRACK holds still go to core-dn1 Mod Menu; all sixteen TRIG steps and original track MUTE chords pass stock.

## Individual native 128x64 pages

- GRANULAR: one 5-control page: PITA, PITB, SIZE, POS, MIX; pitch groups -12, -7, -5, 0, +5, +7, +12, +24; 120..360 ms grains. Original CC 33,34,36,37,60 continue to update the same audio and display state.
- EUREKA AUDIO: SOUND page MIX, TIME, HAZE, FILT, REPT, MOD, VERB, SPED; PLAY page LOOP, WARB, TRNS, ECHO, OG, LOCK, GAIN, STEO. These names correspond to the existing MIYAKO HazeProcessor params. Two pages are navigable by PAGE or the appropriate page keys while visible.
- REEL: one page DRIV, WOW, FLUT, NOIS, WET. This port references Source/MiyakoMasterFx.h from the MIYAKO master bus.
- BITCRUSH: one page BITS 4..16, RATE hold 1..32, MIX, OUT. OUT 100 corresponds to no output gain compensation.

The panel renderer uses the Digitone native fonts, 2x4 knob geometry, top tempo/title band and left gutter; unset controls are truly blank, not fake editable knobs.

## DSP implementation limits

- Granular is the PG79 v0.5 Hann/grain algorithm, moved into the exclusive rack and wired to its dedicated panel. Its already-verified source remains preserved as a separate file under ports/digitone_dreamy_trial on the baseline branch.
- Reel: 18-ms moving buffer, 0.55-Hz wow, 8-Hz flutter, saturation, low/high filtering, optional hiss and wet blend. Fixed-point response curve is NOT byte-for-byte equal to MIYAKO's floating-point tanh. Sonic parity has not been established.
- Eureka: one stereo 2MiB bulk history area at 0x47900000..0x47B00000 (10.92s at 48k), opposite stereo TIME windows, repeat, phase playback, 0.5/1/2 speed, modulation, transposition, warble, OG degradation, lock/echo, filters and a lightweight stereo reverb. Based on MIYAKO Haze control behavior, but not an exact JUCE algorithm recompile. Memory reads are guarded until fresh recording has filled a usable phrase. Sonic parity has not been established.
- Bitcrusher: independent signed quantization and sample-hold implementation with 16-bit/1x neutral path, wet mixing and output compensation.

## Required tests before any build authorization is used

1. Confirm source callback symbols, the five Mod Menu entries, exactly one core_audio owner, no conflicting bulk memory region, valid fixed-point math bounds and no hidden changes to PG79 visual/MUTE logic.
2. Run old Granular native/UBSan tests and the new rack_audio.c + test_rack_audio.c host native/UBSan suite; then m68k cross-compiler, elemod link/lint/patch check and official OS 1.44 SHA verification.
3. Confirm physical unit: Global and Pattern MUTE while internal sequencer plays, TRIG 1..16, all stock pages, MIDI+YES video toggle, visual six pages, each four-FX sound, encoder A-H, NO/track-hold menus and rapid switching; compare Reel and EUREKA against MIYAKO output recordings.

The source is a development candidate, not yet a verified firmware image.
