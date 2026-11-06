#!/usr/bin/env python3
"""Inspect the real host FM-1 EUREKA audio, not a synthetic status flag."""
from array import array
import sys
import wave

def main(path):
    with wave.open(path,"rb") as w:
        rate=w.getframerate()
        ch=w.getnchannels()
        width=w.getsampwidth()
        count=w.getnframes()
        assert (rate,ch,width)==(44100,2,2), (
            f"wrong EUREKA audio capture format: {rate,ch,width}")
        assert count>=rate*2, "EUREKA audio capture too short"
        data=array("h")
        data.frombytes(w.readframes(count))
    if sys.byteorder!="little":
        data.byteswap()
    assert len(data)==count*ch
    first=max(abs(v) for v in data[:rate*ch])
    second=max(abs(v) for v in data[rate*ch:rate*ch*2])
    assert first>250, "EUREKA dry/initial audio path is silent"
    assert second>250, "EUREKA processed audio path is silent after buffer fill"
    assert max(abs(v) for v in data)<32767, "clipped EUREKA test audio"
    print(f"PASS: FM-1 2-second EUREKA host audio, peaks {first}/{second}")

if __name__=="__main__":
    assert len(sys.argv)==2
    main(sys.argv[1])
