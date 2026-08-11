#!/usr/bin/env python3
"""Apollo-era "mission control" radio effect for voice clips.

Takes a mono voice file (wav, or mp3 via ffmpeg) and makes it sound like a
NASA Apollo 11 air-to-ground transmission (Houston CAPCOM talking to the
Eagle crew):

  - band-limited to the voice loop bandwidth (~250-3200 Hz, soft edges)
  - soft-clip overdrive/compression (radio pre-emphasis character)
  - band-limited hiss that follows the voice envelope (squelch gate)
  - optional air-to-ground retransmission echo (--echo)

Output: 8-bit unsigned mono 16 kHz wav (same format as the other samples,
ready for sounds/convert_wav.py if it ever gets added to the sketch).

Usage:
  python3 mission_control_radio.py input.wav|input.mp3 [output.wav]
      [--noise N] [--drive D] [--echo LVL] [--echo-delay S]
"""
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

SR = 16000
BAND_LO = 250.0
BAND_HI = 3200.0
BAND_LO_EDGE = 120.0
BAND_HI_EDGE = 500.0


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


def bandlimit(sig, f_lo=BAND_LO, f_hi=BAND_HI, pad=0.1):
    """FFT bandpass with raised-cosine edges (zero-phase, no ringing)."""
    n_pad = int(pad * SR)
    padded = np.concatenate([np.zeros(n_pad), sig, np.zeros(n_pad)])
    n = len(padded)
    spec = np.fft.rfft(padded)
    freqs = np.fft.rfftfreq(n, 1.0 / SR)
    mask = np.ones(n // 2 + 1)
    lo = np.clip((freqs - (f_lo - BAND_LO_EDGE)) / BAND_LO_EDGE, 0.0, 1.0)
    mask *= 0.5 - 0.5 * np.cos(np.pi * lo)
    hi = np.clip(((f_hi + BAND_HI_EDGE) - freqs) / BAND_HI_EDGE, 0.0, 1.0)
    mask *= 0.5 - 0.5 * np.cos(np.pi * hi)
    out = np.fft.irfft(spec * mask, n)
    return out[n_pad:n_pad + len(sig)]


def moving_rms(sig, win_ms=20.0):
    win = max(1, int(win_ms / 1000.0 * SR))
    kernel = np.ones(win) / win
    sq = np.concatenate([np.zeros(win // 2), sig ** 2, np.zeros(win - win // 2)])
    return np.sqrt(np.convolve(sq, kernel, mode="valid")[:len(sig)] + 1e-12)


def mission_control_fx(voice, noise_level=0.05, drive=2.5,
                       echo_lvl=0.0, echo_delay=2.55, seed=11):
    peak = np.max(np.abs(voice))
    if peak < 1e-6:
        raise ValueError("input is silent")
    voice = voice / peak * 0.75

    voice = bandlimit(voice)
    voice = np.tanh(voice * drive) / np.tanh(drive)

    rng = np.random.default_rng(seed)
    hiss = bandlimit(rng.standard_normal(len(voice)))
    hiss /= np.max(np.abs(hiss))
    env = moving_rms(voice)
    env = np.clip(env / (np.max(env) + 1e-12), 0.0, 1.0) ** 0.6
    noise_gain = 0.006 + noise_level * env
    out = voice + hiss * noise_gain

    if echo_lvl > 0.0:
        d = int(echo_delay * SR)
        echo = np.zeros(len(out) + d)
        echo[:len(out)] = out
        echo[d:] += out * echo_lvl
        out = echo

    out /= max(1.0, np.max(np.abs(out)) / 0.95)
    return out


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
    value_flags = {"--noise", "--drive", "--echo", "--echo-delay"}
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
        dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), stem + "_radio.wav")

    noise = float(opts.get("--noise", 0.05))
    drive = float(opts.get("--drive", 2.5))
    echo = float(opts.get("--echo", 0.0))
    echo_delay = float(opts.get("--echo-delay", 2.55))

    try:
        sig, sr = load_input(src)
    except Exception as e:
        print(f"error: cannot read '{src}' ({e})")
        sys.exit(1)
    sig = resample(sig, sr)
    sig = trim_silence(sig)
    print(f"input: {src} ({len(sig)/SR:.2f}s @ {sr} Hz)")
    out = mission_control_fx(sig, noise_level=noise, drive=drive,
                             echo_lvl=echo, echo_delay=echo_delay)
    write_pcm8(dst, out)


if __name__ == "__main__":
    main()
