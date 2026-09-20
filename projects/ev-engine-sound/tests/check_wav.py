#!/usr/bin/env python3
"""Validate rendered artifacts, including sample bounds and the stop tail."""
import array
import pathlib
import sys
import wave

root=pathlib.Path(sys.argv[1])
for name in ("single","twin270","vtwin","inline4"):
    with wave.open(str(root/(name+".wav")),"rb") as f:
        assert (f.getframerate(),f.getnchannels(),f.getsampwidth(),f.getnframes())==(32000,1,2,320000)
        pcm=array.array("h",f.readframes(f.getnframes()))
        if sys.byteorder!="little": pcm.byteswap()
    peak=max(abs(v) for v in pcm)
    assert 100<peak<30000, (name,peak)
    assert max(abs(v) for v in pcm[-8000:])==0, name
    rms=(sum(v*v for v in pcm)/len(pcm))**.5
    print(f"PASS {name}: 10s PCM16/32kHz/mono peak={peak} rms={rms:.1f}, last 250ms silent")
