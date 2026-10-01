# Audio Algorithm MVP

## Known design risk
Pitch shifting quality/latency is the highest-risk area. The bootstrap granular bank is intentionally simple and must be judged on the target device before it is treated as production quality.

## Expected latency contributors
- pitch detector window/hop;
- note-stability acceptance;
- granular window;
- Android device buffer.

No latency claim is made until measured with the actual Android+iRig setup.

## Harmonic vocabulary by COMPLEX
- 0.00..0.22: mainly diatonic triads.
- >0.22: seventh colours may appear.
- >0.42: add9 colours may appear.
- >0.62: borrowed iv / bVI-style colour may appear.
- >0.82: altered dominant or tritone-substitute behaviour may appear in dominant function.

These are weighted probabilities, not hard mode switches.