#!/usr/bin/env python3
"""Generate retro arcade sounds for Lunar Lander.

Creates 8-bit unsigned PCM mono WAV files at 16 kHz:
  - rocket_thrust.wav : looping low-frequency rumble (noise, band-limited)
  - explosion.wav     : one-shot noise burst with fast decay
"""
import wave
import random
import math
import os

SR = 16000

def clamp8(v):
    return max(0, min(255, int(round(v))))

def write_wav(path, samples):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(SR)
        w.writeframes(bytes(clamp8(s) for s in samples))
    print(f"wrote {path}: {len(samples)} samples ({len(samples)/SR:.2f}s)")

def make_thrust(duration=1.0, seed=1234):
    """Loopable lowpassed noise rumble. Gain shaped to avoid loop click."""
    rng = random.Random(seed)
    n = int(SR * duration)
    out = []
    lp = 0.0
    amp = 0.85
    # one-pole lowpass ~400 Hz to keep it as a rumble
    alpha = 400.0 / SR
    # slow amplitude wobble (turbine beat)
    for i in range(n):
        t = i / SR
        white = rng.uniform(-1, 1)
        lp = lp + alpha * (white - lp)
        wob = 0.7 + 0.3 * math.sin(2 * math.pi * 3.0 * t)
        v = lp * amp * wob
        out.append(128 + v * 127)
    # crossfade last 5% into first 5% to make loop seamless
    fade = int(SR * 0.05)
    for i in range(fade):
        a = i / fade
        head = out[i]
        tail = out[n - fade + i]
        out[i] = head * (1 - a) + tail * a
        out[n - fade + i] = head * a + tail * (1 - a)
    return out

def make_explosion(duration=1.2, seed=99):
    """Noise burst, fast decay + a low boom tail."""
    rng = random.Random(seed)
    n = int(SR * duration)
    out = []
    # low boom: 55 Hz decaying
    boom_amp = 0.9
    boom_alpha = 3.5  # decay rate (exp)
    noise_amp = 1.0
    noise_alpha = 8.0
    for i in range(n):
        t = i / SR
        white = rng.uniform(-1, 1)
        boom = math.sin(2 * math.pi * 55.0 * t)
        e_boom = math.exp(-boom_alpha * t)
        e_noise = math.exp(-noise_alpha * t)
        v = boom * boom_amp * e_boom + white * noise_amp * e_noise
        v *= 0.9
        out.append(128 + v * 127)
    return out

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    write_wav(os.path.join(here, "rocket_thrust.wav"), make_thrust())
    write_wav(os.path.join(here, "explosion.wav"), make_explosion())
