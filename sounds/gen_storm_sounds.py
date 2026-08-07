#!/usr/bin/env python3
"""Generate the storm/wind sounds for Lunar Lander.

Creates 8-bit unsigned mono WAV at 16 kHz:
  - wind.wav      : subtle lo-fied noise loop (seamless) for the wind
  - lightning.wav : one-shot crack + thunder rumble for lightning bolts

Does NOT touch rocket_thrust.wav / explosion.wav (real sounds).
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

def make_wind(duration=2.0, seed=2026, amp=0.55, cutoff=500.0):
    """Seamless loopable lowpassed noise, gentle amplitude wobble."""
    rng = random.Random(seed)
    n = int(SR * duration)
    out = []
    lp = 0.0
    alpha = cutoff / SR
    for i in range(n):
        t = i / SR
        white = rng.uniform(-1, 1)
        lp = lp + alpha * (white - lp)
        wob = 0.75 + 0.25 * math.sin(2 * math.pi * 0.6 * t + 0.5 * math.sin(2 * math.pi * 0.11 * t))
        out.append(128 + lp * amp * wob * 127)
    fade = int(SR * 0.05)
    for i in range(fade):
        a = i / fade
        head = out[i]
        tail = out[n - fade + i]
        out[i] = head * (1 - a) + tail * a
        out[n - fade + i] = head * a + tail * (1 - a)
    return out

def make_lightning(duration=1.2, seed=707):
    """Sharp crack (fast attack) + low thunder rumble tail."""
    rng = random.Random(seed)
    n = int(SR * duration)
    out = []
    for i in range(n):
        t = i / SR
        white = rng.uniform(-1, 1)
        white2 = rng.uniform(-1, 1)
        attack = min(t / 0.002, 1.0)
        crack = white * math.exp(-t * 20.0) * attack
        rumble = (math.sin(2 * math.pi * 52.0 * t) + 0.5 * math.sin(2 * math.pi * 88.0 * t)) * math.exp(-t * 2.8)
        v = (crack * 1.1 + rumble * 0.9 + white2 * 0.5 * math.exp(-t * 5.0)) / 1.3
        out.append(128 + v * 127)
    return out

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    write_wav(os.path.join(here, "wind.wav"), make_wind())
    write_wav(os.path.join(here, "lightning.wav"), make_lightning())
