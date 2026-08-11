#!/usr/bin/env python3
"""Apollo 11 air-to-ground radio effect for voice clips (Houston <-> Eagle).

Takes a mono voice recording (wav, or mp3 via ffmpeg) and makes it sound like
a NASA Apollo 11 mission control transmission: Houston CAPCOM talking to the
Eagle crew, and optionally the crew answering back through the LM comms.

Effects per side:

  Houston (mission control):
    - voice-loop band-pass ~250-3000 Hz with soft edges
    - radio pre-emphasis soft-clip overdrive
    - band-limited hiss gated by the voice envelope (squelch)
    - low carrier hiss on the open loop

  Eagle (lunar module crew):
    - narrower, muffled band-pass ~350-2200 Hz
    - hollow "horn" resonances (LM comms colour)
    - heavier drive, more hiss -> clearly the other end of the radio

Both sides:
  - push-to-talk key-up/key-down fades around every transmission
  - optional earth-moon round-trip echo (~2.55 s, --echo)

With --two-way the script splits the recording at silences and alternates
Houston / Eagle on consecutive speech bursts, so a single file holding a
call-and-response becomes a dialogue (Houston speaks first; use --first-side
to swap).

Output: 8-bit unsigned mono 16 kHz wav (the format the ESP32 sketch uses).

Usage:
  python3 apollo11_radio.py input.wav|input.mp3 [output.wav]
      [--two-way] [--first-side houston|eagle]
      [--side houston|eagle] [--noise N] [--drive D]
      [--echo LVL] [--echo-delay S] [--seed N]
"""
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

SR = 16000

HOUSTON_BAND = (250.0, 3000.0, 120.0, 500.0)
EAGLE_BAND = (350.0, 2200.0, 150.0, 450.0)
EAGLE_RESONANCE = ((1200.0, 350.0, 0.9), (2400.0, 500.0, 0.5))

SIDES = {
    "houston": dict(band=HOUSTON_BAND, resonance=None, drive_k=1.0,
                    hiss_base=0.006, hiss_gate=0.60),
    "eagle": dict(band=EAGLE_BAND, resonance=EAGLE_RESONANCE, drive_k=1.45,
                  hiss_base=0.012, hiss_gate=0.75),
}


def read_wav_pcm(path):
    with wave.open(path, "rb") as w:
        ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw == 1:
        data = (np.frombuffer(raw, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
    elif sw == 2:
        data = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        v = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8)
             | (b[:, 2].astype(np.int32) << 16))
        v = np.where(v & 0x800000, v - 0x1000000, v)
        data = v.astype(np.float64) / 8388608.0
    elif sw == 4:
        data = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"unsupported sample width {sw}")
    if ch > 1:
        data = data.reshape(-1, ch).mean(axis=1)
    return data, sr


def read_via_ffmpeg(path):
    tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    tmp.close()
    try:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", path,
                        "-ac", "1", "-ar", str(SR), "-f", "wav", "-acodec", "pcm_s16le",
                        tmp.name], check=True)
        return read_wav_pcm(tmp.name)
    finally:
        os.unlink(tmp.name)


def load_input(path):
    try:
        return read_wav_pcm(path)
    except Exception:
        return read_via_ffmpeg(path)


def resample(sig, sr_in):
    if sr_in == SR:
        return sig
    n_out = int(round(len(sig) * SR / sr_in))
    x_old = np.linspace(0.0, 1.0, num=len(sig), endpoint=False)
    x_new = np.linspace(0.0, 1.0, num=n_out, endpoint=False)
    return np.interp(x_new, x_old, sig)


def trim_silence(sig, thr=0.015, margin=0.08):
    idx = np.where(np.abs(sig) > thr)[0]
    if len(idx) == 0:
        return sig
    m = int(margin * SR)
    return sig[max(0, idx[0] - m):min(len(sig), idx[-1] + m)]


def moving_rms(sig, win_ms=20.0):
    win = max(1, int(win_ms / 1000.0 * SR))
    kernel = np.ones(win) / win
    sq = np.concatenate([np.zeros(win // 2), sig ** 2, np.zeros(win - win // 2)])
    return np.sqrt(np.convolve(sq, kernel, mode="valid")[:len(sig)] + 1e-12)


def band_mask(n_sig, band, resonance=None):
    f_lo, f_hi, f_lo_edge, f_hi_edge = band
    freqs = np.fft.rfftfreq(n_sig, 1.0 / SR)
    mask = np.ones(len(freqs))
    lo = np.clip((freqs - (f_lo - f_lo_edge)) / f_lo_edge, 0.0, 1.0)
    mask *= 0.5 - 0.5 * np.cos(np.pi * lo)
    hi = np.clip(((f_hi + f_hi_edge) - freqs) / f_hi_edge, 0.0, 1.0)
    mask *= 0.5 - 0.5 * np.cos(np.pi * hi)
    if resonance:
        for f0, bw, gain in resonance:
            mask *= 1.0 + gain * np.exp(-0.5 * ((freqs - f0) / bw) ** 2)
    return mask


def spectral(sig, band, resonance=None, pad=0.1):
    n_pad = int(pad * SR)
    padded = np.concatenate([np.zeros(n_pad), sig, np.zeros(n_pad)])
    n = len(padded)
    spec = np.fft.rfft(padded)
    out = np.fft.irfft(spec * band_mask(n, band, resonance), n)
    return out[n_pad:n_pad + len(sig)]


def split_segments(sig, thr_rel=0.12, gap_ms=160, min_ms=70):
    env = moving_rms(sig, win_ms=15)
    thr = max(float(env.max()) * thr_rel, 1e-4)
    on = env > thr
    n = len(on)
    runs = []
    i = 0
    while i < n:
        if on[i]:
            j = i
            while j < n and on[j]:
                j += 1
            runs.append((i, j))
            i = j
        else:
            i += 1
    gap = int(gap_ms / 1000.0 * SR)
    minlen = int(min_ms / 1000.0 * SR)
    merged = []
    for s, e in runs:
        if merged and s - merged[-1][1] < gap:
            merged[-1] = (merged[-1][0], e)
        else:
            merged.append((s, e))
    return [(s, e) for s, e in merged if e - s >= minlen]


def chain(seg, side, noise_level, drive, rng):
    p = np.max(np.abs(seg))
    if p < 1e-6:
        return np.zeros(len(seg))
    spec = SIDES[side]
    voice = seg / p * 0.75
    voice = spectral(voice, spec["band"], spec["resonance"])
    drv = drive * spec["drive_k"]
    voice = np.tanh(voice * drv) / np.tanh(drv)
    hiss = spectral(rng.standard_normal(len(seg)), spec["band"], spec["resonance"])
    hiss /= max(float(np.max(np.abs(hiss))), 1e-9)
    env = moving_rms(voice)
    env = np.clip(env / (float(np.max(env)) + 1e-12), 0.0, 1.0) ** 0.6
    n = noise_level * (spec["hiss_base"] + spec["hiss_gate"] * env)
    return voice + hiss * n


def render(sig, two_way, side, first_side, noise_level, drive, seed):
    rng = np.random.default_rng(seed)
    if two_way:
        segs = split_segments(sig)
        if not segs:
            segs = [(0, len(sig))]
    else:
        segs = [(0, len(sig))]
    if two_way:
        print(f"two-way: {len(segs)} speech segment(s), first={first_side}")
    out = np.zeros(len(sig))
    for k, (s, e) in enumerate(segs):
        if two_way:
            cur = first_side if k % 2 == 0 else ("eagle" if first_side == "houston" else "houston")
        else:
            cur = side
        block = chain(sig[s:e], cur, noise_level, drive, rng)
        nf = min(len(block), int(0.012 * SR))
        if nf > 0:
            block[:nf] *= np.linspace(0.0, 1.0, nf)
        nr = min(len(block), int(0.030 * SR))
        if nr > 0:
            block[-nr:] *= np.linspace(1.0, 0.0, nr)
        out[s:e] += block
    loop = spectral(rng.standard_normal(len(sig)), HOUSTON_BAND)
    lp = float(np.max(np.abs(loop)))
    if lp > 1e-9:
        out += loop / lp * (0.002 + 0.004 * noise_level)
    return out


def apply_echo(out, lvl, delay):
    d = int(delay * SR)
    echo = np.zeros(len(out) + d)
    echo[:len(out)] = out
    echo[d:] += out * lvl
    return echo


def write_pcm8(path, samples):
    clipped = np.clip(samples, -1.0, 1.0)
    pcm8 = np.rint((clipped + 1.0) * 127.5).astype(np.uint8)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(SR)
        w.writeframes(pcm8.tobytes())
    print(f"wrote {path}: {len(pcm8)} samples ({len(pcm8)/SR:.2f}s)")


def main():
    raw = sys.argv[1:]
    pos = [a for a in raw if not a.startswith("--")]
    opts = {}
    value_flags = {"--first-side", "--side", "--noise", "--drive",
                   "--echo", "--echo-delay", "--seed"}
    i = 0
    while i < len(raw):
        a = raw[i]
        if not a.startswith("--"):
            i += 1
            continue
        if "=" in a:
            k, v = a.split("=", 1)
            opts[k] = v
        elif a in value_flags:
            if i + 1 < len(raw):
                opts[a] = raw[i + 1]
                i += 1
            else:
                opts[a] = True
        else:
            opts[a] = True
        i += 1
    if not pos:
        print(__doc__)
        sys.exit(1)
    src = pos[0]
    if len(pos) > 1:
        dst = pos[1]
    else:
        stem = os.path.splitext(os.path.basename(src))[0]
        dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), stem + "_apollo11.wav")

    two_way = "--two-way" in opts
    first_side = opts.get("--first-side", "houston")
    if first_side not in SIDES:
        print(f"error: --first-side must be houston or eagle, got '{first_side}'")
        sys.exit(1)
    side = opts.get("--side", "houston")
    if side not in SIDES:
        print(f"error: --side must be houston or eagle, got '{side}'")
        sys.exit(1)
    noise = float(opts.get("--noise", 0.05))
    drive = float(opts.get("--drive", 2.5))
    echo = float(opts.get("--echo", 0.0))
    echo_delay = float(opts.get("--echo-delay", 2.55))
    seed = int(opts.get("--seed", 11))

    try:
        sig, sr = load_input(src)
    except Exception as e:
        print(f"error: cannot read '{src}' ({e})")
        sys.exit(1)
    sig = resample(sig, sr)
    sig = trim_silence(sig)
    print(f"input: {src} ({len(sig)/SR:.2f}s @ {sr} Hz, mono)")
    out = render(sig, two_way, side, first_side, noise, drive, seed)
    if echo > 0.0:
        out = apply_echo(out, echo, echo_delay)
    out /= max(1.0, float(np.max(np.abs(out))) / 0.95)
    write_pcm8(dst, out)


if __name__ == "__main__":
    main()
