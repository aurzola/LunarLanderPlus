#!/usr/bin/env python3
"""Post-process the real moonlander sounds for the ESP32.

Reads tmp 16 kHz mono 16-bit PCM wavs and writes 8-bit unsigned mono wavs:
  - rocket_thrust.wav : most steady ~2 s segment of rocket.mp3, crossfaded for a
    seamless loop
  - explosion.wav     : crash.mp3, trim leading/trailing silence
"""
import os
import sys
import wave

import numpy as np

SR = 16000
LOOP_LEN = int(2.0 * SR)  # 2 s loop
CROSSFADE = int(0.05 * SR)  # 50 ms

def read_pcm16(path):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 1
        assert w.getsampwidth() == 2
        assert w.getframerate() == SR
        n = w.getnframes()
        data = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float64) / 32768.0
    return data

def write_pcm8(path, samples):
    clipped = np.clip(samples, -1.0, 1.0)
    pcm8 = np.rint((clipped + 1.0) * 127.5).astype(np.uint8)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(SR)
        w.writeframes(pcm8.tobytes())
    print(f"wrote {path}: {len(pcm8)} samples ({len(pcm8)/SR:.2f}s)")

def loudness(sig):
    return float(np.sqrt(np.mean(sig**2)))

def pick_steady_loop(sig, lo, hi):
    win = 4096
    step = 256
    rms = []
    for i in range(0, len(sig) - win + 1, step):
        rms.append(loudness(sig[i:i + win]))
    rms = np.array(rms)
    # candidate start offsets in samples (keep room for the continuation)
    starts = range(lo, hi - LOOP_LEN - CROSSFADE + 1, step)
    best = None
    best_score = None
    for s0 in starts:
        w0 = s0 // step
        seg = rms[w0:w0 + LOOP_LEN // step]
        if len(seg) < 2:
            continue
        # want low relative stddev (steady) but not silence
        mean = seg.mean()
        if mean < 0.05:
            continue
        score = seg.std() / mean
        if best_score is None or score < best_score:
            best_score = score
            best = s0
    return best

def make_loop(src, s0, n, f):
    """Seamless loop: first f samples crossfade from the natural continuation
    (src[s0+n .. s0+n+f)) into the head (src[s0 .. s0+f)).  The wrap and the
    head/middle boundary are both smooth because they join adjacent source
    samples."""
    T = src[s0:s0 + n + f].copy()
    L = T[:n].copy()
    for i in range(f):
        a = i / f
        L[i] = T[n + i] * (1 - a) + T[i] * a
    return L

def trim_silence(sig, thr=0.02):
    n = len(sig)
    s, e = 0, n
    while s < n and abs(sig[s]) < thr:
        s += 1
    while e > s and abs(sig[e - 1]) < thr:
        e -= 1
    # keep a little attack headroom
    s = max(0, s - int(0.01 * SR))
    return sig[s:e]

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    tmp = sys.argv[1] if len(sys.argv) > 1 else "/tmp/opencode"
    rocket = read_pcm16(os.path.join(tmp, "rocket16.wav"))
    crash = read_pcm16(os.path.join(tmp, "crash16.wav"))

    print(f"rocket: {len(rocket)/SR:.2f}s  crash: {len(crash)/SR:.2f}s")

    # skip the first 10% (possible intro/fade-in)
    s0 = pick_steady_loop(rocket, int(0.10 * len(rocket)), int(0.90 * len(rocket)))
    if s0 is None:
        print("no steady loop found, using middle")
        s0 = (len(rocket) - LOOP_LEN - CROSSFADE) // 2
    print(f"loop start at {s0} ({s0/SR:.2f}s)")
    loop = make_loop(rocket, s0, LOOP_LEN, CROSSFADE)
    loop = np.tanh(loop * 3.0) * 0.85
    write_pcm8(os.path.join(here, "rocket_thrust.wav"), loop)

    crash = trim_silence(crash)
    crash = np.tanh(crash * 4.0) * 0.9
    write_pcm8(os.path.join(here, "explosion.wav"), crash)

if __name__ == "__main__":
    main()
