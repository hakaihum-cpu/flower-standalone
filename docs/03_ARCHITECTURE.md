# Architecture

## Signal path
Audio input -> mono analysis tap -> YIN pitch estimate -> stability gate -> TheoryEngine -> target MIDI pitches -> pitch ratios -> GranularPitchBank -> stereo output.

Stopped state bypasses dry input. Running state outputs generated harmony only in the MVP.

## Pitch detection
1024-sample YIN window, 256-sample hop. Range approximately 75..1100 Hz. RMS and confidence gates reject silence/unstable estimates. Two matching note estimates are required before a note change is accepted.

## Harmony generation
The first note establishes a provisional tonal centre. Because one note cannot establish major/minor, the initial chord deliberately avoids the third (`5(add9)`). Following input notes add major/minor evidence. Progression movement uses a weighted degree transition matrix rather than equiprobable random chords.

COMPLEX layers additional harmonic vocabulary; it does not simply increase random chance uniformly.

## Pitch shifting
A low-latency overlapping-grain pitch bank reuses the input audio. Each target chord tone receives a pitch ratio from target MIDI note vs current detected pitch. No oscillator/synth voice is present.

## Timing
Internal: BAR factor × four beats × 60/BPM.
MIDI: 24 PPQN, therefore 24/48/96/192 clocks for 1/4, 1/2, 1, 2 bars.
LENGTH=MAX bypasses timed progression and latches until new input.

## Visuals
The 300 source JPEG byte streams are packed without image regeneration into `classroom_frames.pack`. Runtime decodes the selected frame and stretches it to the screen. The sequence index changes only on chord events.

## Audio-device access
The standalone editor uses JUCE `StandalonePluginHolder::deviceManager` so CONFIG operates on the actual standalone host device manager rather than a separate unused manager.