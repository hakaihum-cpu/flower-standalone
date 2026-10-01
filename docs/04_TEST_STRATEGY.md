# Test Strategy

## Static
- Exactly 300 supplied frame records.
- Frame pack embedded in JUCER.
- `pluginIsSynth=0` and no synthesiser classes/tokens.
- No FLOWER/MIYAKO source dependency.
- Manual CircleCI gate retained.
- Native compile/link parallelism capped as in FLOWER.

## Native core test
Pure-C++ test validates:
- 1000 theory transitions always produce notes in C3..C7 safety range used by MVP.
- YIN recognises a 220 Hz sine within 3 Hz.
- Granular pitch bank produces non-zero output.

## Android build
Only after repository/branch state is fixed and static audit passes. One justified CircleCI build, no exploratory rerun.

## Device verification
With iRig Stream attached:
1. app opens without ANR;
2. CONFIG input list updates;
3. external input causes INPUT LEVEL movement;
4. iRig exact-name detection is checked (or recorded as generic-name limitation);
5. single notes produce stable detected-note text;
6. REC starts harmony and red VHS overlay;
7. second background tap stops, clears chord and returns frame 1;
8. COMPLEX/BAR/WIDTH/LENGTH behave independently;
9. LENGTH MAX holds until next accepted input note;
10. MIDI clock changes progression at the selected BAR interval.

## Audio quality checks
Record latency, octave-down artifacts, chord-transition clicks, low-note tracking, and CPU load. Do not call the algorithm final-quality before these are measured on device.