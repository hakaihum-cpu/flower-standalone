#!/usr/bin/env python3
"""Read the actual FM-1 host audio capture and reject silent/clipped test runs."""
from array import array
from pathlib import Path
import argparse
import sys
import wave

def main():
    p = argparse.ArgumentParser()
    p.add_argument("wav", type=Path)
    a = p.parse_args()
    with wave.open(str(a.wav), "rb") as w:
        channels, width, rate, count = (
            w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes())
        assert (channels, width, rate) == (2, 2, 44100), (
            f"unexpected FM-1 host WAV format: {channels}, {width}, {rate}")
        assert 3.8 * rate <= count <= 10 * rate, f"bad audio duration: {count/rate:.2f}s"
        data = array("h")
        data.frombytes(w.readframes(count))
    if sys.byteorder != "little":
        data.byteswap()
    assert len(data) == count * channels
    peak = max(abs(v) for v in data)
    assert 300 <= peak < 32767, f"host audio silent or digital clipping: {peak}"
    for i in range(3):
        start = int((i * 1.3 + 0.08) * rate) * channels
        end = min(len(data), int((i * 1.3 + 1.25) * rate) * channels)
        segpeak = max((abs(v) for v in data[start:end]), default=0)
        assert segpeak >= 300, (
            f"MIYAKO FX segment {i+1}/3 did not produce sound: peak {segpeak}")
    print(f"PASS native FM-1 MIDI audio WAV: {count/rate:.2f}s, "
          f"16-bit stereo, non-silent Delay/Reverb/Chorus, peak {peak}/32767")
    print("NOT equivalent to original MIYAKO DSP; real FM-1 verification pending")

if __name__ == "__main__":
    main()
