#!/usr/bin/env python3
"""Generate the storm/wind sounds for Lunar Lander.

Creates 8-bit unsigned mono WAV at 16 kHz:
  - wind.wav      : subtle lo-fied noise loop (seamless) for the wind
  - lightning.wav : one-shot crack + thunder rumble for lightning bolts
  - quake.wav     : one-shot tectonic rumble (crescendo) + ground-break thud

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

def make_quake(duration=2.6, seed=909, rumble_end=1.0):
    """Tectonic rumble for Io quakes.

    A deep lowpassed-noise rumble that crescendos through the warning phase
    (QUAKE_RUMBLE_TIME ~1.0 s) and lands on a heavy thud at the strike, then a
    short decaying aftershock tail.
    """
    rng = random.Random(seed)
    n = int(SR * duration)
    out = []
    # Deep rumble band plus a second lower band for the boom.
    alpha = 120.0 / SR
    boom_alpha = 90.0 / SR
    lp = 0.0
    lp2 = 0.0
    for i in range(n):
        t = i / SR
        white = rng.uniform(-1, 1)
        lp = lp + alpha * (white - lp)
        lp2 = lp2 + boom_alpha * (white - lp2)
        # Crescendo: quiet subsonic murmur -> full rumble by the strike.
        if t < rumble_end:
            a = t / rumble_end
            env = 0.25 + 0.75 * (a * a)
            # Low rumble with a slow tectonic pulse, boosted to be audible.
            pulse = 0.8 + 0.2 * math.sin(2 * math.pi * 1.7 * t)
            sub = (math.sin(2 * math.pi * 36.0 * t) +
                   0.6 * math.sin(2 * math.pi * 54.0 * t + 1.3)) * 0.5
            v = (lp * 4.0 + lp2 * 2.0 + sub) * env * pulse * 0.9
            out.append(128 + v * 127)
            continue
        # Strike thud: hard boom with a fast initial transient.
        dt = t - rumble_end
        env = math.exp(-dt * 4.0)
        boom = math.sin(2 * math.pi * 48.0 * dt) * math.exp(-dt * 3.0) * 0.9
        thud = (lp2 * 3.0 - lp * 2.0) * math.exp(-dt * 12.0) * 0.9
        v = boom + thud + lp * 3.0 * env
        out.append(128 + v * 127)
    fade = int(SR * 0.02)
    for i in range(fade):
        tail = out[n - fade + i]
        a = (fade - i) / fade
        out[n - fade + i] = 128 + (tail - 128) * a
    return out

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    write_wav(os.path.join(here, "wind.wav"), make_wind())
    write_wav(os.path.join(here, "lightning.wav"), make_lightning())
    write_wav(os.path.join(here, "quake.wav"), make_quake())
