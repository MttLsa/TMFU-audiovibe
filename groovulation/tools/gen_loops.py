#!/usr/bin/env python3
"""
Groovulation factory loop generator.

Renders the default loop folders (Loops/01 Kick ... Loops/09 Melodic 03 Synth) as 44.1 kHz / 16-bit WAV files,
all synthesised here (no third-party audio). Names carry tempo and key ("Bass Rubber 124bpm Emin") so both the
audio analysis and the "File Name" key mode can be tested. Two timbre "families" per type, so the
similar-loops database has something to group.

Usage: python3 tools/gen_loops.py out_dir      (then zip -r resources/factory_loops.zip Loops)
"""
import json, os, sys, wave
import numpy as np

SR = 44100
OUT = sys.argv[1] if len(sys.argv) > 1 else "build/factory"
ROOT = os.path.join(OUT, "Loops")
FOLDERS = ["01 Kick", "02 Snare-Clap", "03 Bottom", "04 Top", "05 Percussion", "06 Bass",
           "07 Melodic 01 Chords-Stabs", "08 Melodic 02 Pads", "09 Melodic 03 Synth"]
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
MAJOR = [0, 2, 4, 5, 7, 9, 11]
MINOR = [0, 2, 3, 5, 7, 8, 10]
manifest = {"version": 1, "folders": {}}


def rng(seed):
    return np.random.default_rng(seed)


def mtof(m):
    return 440.0 * 2 ** ((m - 69) / 12.0)


def fft_filter(x, lo=None, hi=None, order=2):
    """Static zero-phase-ish filter in the frequency domain (Butterworth-like magnitude)."""
    n = len(x)
    N = 1 << int(np.ceil(np.log2(n + 1)))
    X = np.fft.rfft(x, N)
    f = np.fft.rfftfreq(N, 1 / SR)
    H = np.ones_like(f)
    if hi is not None:
        H *= 1 / np.sqrt(1 + (f / hi) ** (2 * order))
    if lo is not None:
        H *= 1 / np.sqrt(1 + (lo / np.maximum(f, 1e-3)) ** (2 * order))
    return np.fft.irfft(X * H, N)[:n]


def saw(freq, n, phase=0.0):
    ph = (phase + np.cumsum(np.full(n, freq / SR))) % 1.0
    # mild band-limiting: blend with a 3-harmonic version at high pitch
    return 2 * ph - 1


def env_ad(n, a, d):
    t = np.arange(n) / SR
    return np.where(t < a, t / max(a, 1e-5), np.exp(-(t - a) / max(d, 1e-4)))


def place(buf, sig, start):
    start = int(start)
    if start >= len(buf):
        return
    end = min(len(buf), start + len(sig))
    buf[start:end] += sig[: end - start]
    # wrap tails around (seamless loops)
    rest = len(sig) - (end - start)
    if rest > 0:
        k = min(rest, len(buf))
        buf[:k] += sig[end - start: end - start + k]


def bar_len(bpm, bars):
    return int(round(bars * 4 * 60.0 / bpm * SR))


def step_pos(bpm, step, swing=0.0):
    s16 = 60.0 / bpm / 4 * SR
    return step * s16 + (s16 * swing if step % 2 == 1 else 0)


def write(folder, name, x, meta):
    os.makedirs(os.path.join(ROOT, folder), exist_ok=True)
    x = np.asarray(x, dtype=np.float64)
    peak = np.max(np.abs(x)) + 1e-9
    x = x / peak * 0.89
    data = (np.clip(x, -1, 1) * 32767).astype("<i2")
    path = os.path.join(ROOT, folder, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1 if x.ndim == 1 else 2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())
    manifest["folders"].setdefault(folder, []).append({"file": name + ".wav", **meta})


# ------------------------------------------------------------------------------------------ drums
def kick(seed, tone=50, punch=0.08, decay=0.35, click=0.3):
    n = int(SR * (decay * 2.5))
    t = np.arange(n) / SR
    f = tone + tone * 3 * np.exp(-t / punch)
    ph = np.cumsum(2 * np.pi * f / SR)
    s = np.sin(ph) * np.exp(-t / decay)
    s += click * rng(seed).standard_normal(n) * np.exp(-t / 0.004)
    return np.tanh(s * 1.5)


def snare(seed, tone=190, decay=0.16, noise=0.8):
    n = int(SR * 0.5)
    t = np.arange(n) / SR
    r = rng(seed)
    body = np.sin(2 * np.pi * tone * t) * np.exp(-t / 0.05)
    nz = fft_filter(r.standard_normal(n), lo=1500, hi=9000) * np.exp(-t / decay)
    return body * (1 - noise) + nz * noise * 1.6


def clap(seed, decay=0.18):
    n = int(SR * 0.5)
    t = np.arange(n) / SR
    r = rng(seed)
    nz = fft_filter(r.standard_normal(n), lo=900, hi=5000)
    e = np.zeros(n)
    for k, off in enumerate([0, 0.011, 0.022, 0.031]):
        tt = np.clip(t - off, 0, None)
        e += np.where(t >= off, np.exp(-tt / (0.007 if k < 3 else decay)), 0)
    return nz * e


def hat(seed, decay=0.05, bright=8000):
    n = int(SR * (decay * 6))
    t = np.arange(n) / SR
    nz = fft_filter(rng(seed).standard_normal(n), lo=bright, order=3)
    return nz * np.exp(-t / decay)


def tom(seed, f0=180, decay=0.18):
    n = int(SR * decay * 4)
    t = np.arange(n) / SR
    f = f0 * (1 + 0.5 * np.exp(-t / 0.03))
    return np.sin(np.cumsum(2 * np.pi * f / SR)) * np.exp(-t / decay)


def conga(seed, f0=260, decay=0.12):
    n = int(SR * decay * 4)
    t = np.arange(n) / SR
    s = np.sin(2 * np.pi * f0 * t) * np.exp(-t / decay) + 0.3 * np.sin(2 * np.pi * f0 * 1.52 * t) * np.exp(-t / (decay * 0.5))
    s += 0.2 * rng(seed).standard_normal(n) * np.exp(-t / 0.003)
    return s


def shaker(seed, decay=0.03):
    n = int(SR * 0.12)
    t = np.arange(n) / SR
    nz = fft_filter(rng(seed).standard_normal(n), lo=5000, hi=14000)
    a = np.minimum(1, t / 0.008)
    return nz * a * np.exp(-t / decay)


def pattern(bpm, bars, hits, sound_fn, swing=0.0, vel=None):
    L = bar_len(bpm, bars)
    buf = np.zeros(L)
    for b in range(bars):
        for i, st in enumerate(hits):
            v = 1.0 if vel is None else vel[i % len(vel)]
            place(buf, sound_fn(b * 16 + st) * v, step_pos(bpm, b * 16 + st, swing))
    return buf


def make_drums():
    # 01 Kick: family A deep/long, family B punchy/short
    specs = [("Kick Deep 4x4 124bpm", 124, 2, [0, 4, 8, 12], dict(tone=45, punch=0.06, decay=0.45, click=0.2)),
             ("Kick Deep Offbeat 122bpm", 122, 2, [0, 4, 8, 12, 14], dict(tone=43, punch=0.07, decay=0.5, click=0.15)),
             ("Kick Rolling 126bpm", 126, 1, [0, 3, 4, 8, 11, 12], dict(tone=48, punch=0.05, decay=0.35, click=0.2)),
             ("Kick Punch 4x4 128bpm", 128, 2, [0, 4, 8, 12], dict(tone=58, punch=0.03, decay=0.18, click=0.6)),
             ("Kick Punch Broken 120bpm", 120, 2, [0, 6, 10, 13], dict(tone=60, punch=0.025, decay=0.15, click=0.7)),
             ("Kick Punch Garage 130bpm", 130, 1, [0, 7, 10], dict(tone=62, punch=0.03, decay=0.16, click=0.6))]
    for i, (nm, bpm, bars, hits, kw) in enumerate(specs):
        k = kick(10 + i, **kw)
        write(FOLDERS[0], nm, pattern(bpm, bars, hits, lambda s: k), {"bpm": bpm, "bars": bars})
    # 02 Snare / clap
    specs = [("Snare Tight 124bpm", 124, 2, [4, 12], lambda s, i: snare(20 + i, 200, 0.12, 0.75)),
             ("Snare Fat 120bpm", 120, 2, [4, 12, 15], lambda s, i: snare(21 + i, 170, 0.22, 0.7)),
             ("Snare Rim 126bpm", 126, 1, [4, 10, 12], lambda s, i: snare(22 + i, 420, 0.04, 0.4)),
             ("Clap Classic 124bpm", 124, 2, [4, 12], lambda s, i: clap(23 + i, 0.16)),
             ("Clap Wide 128bpm", 128, 2, [4, 12, 14], lambda s, i: clap(24 + i, 0.3)),
             ("Clap Snap 122bpm", 122, 1, [4, 11, 12], lambda s, i: clap(25 + i, 0.08))]
    for i, (nm, bpm, bars, hits, fn) in enumerate(specs):
        write(FOLDERS[1], nm, pattern(bpm, bars, hits, lambda s: fn(s, i)), {"bpm": bpm, "bars": bars})
    # 03 Bottom: full grooves (kick + snare + hats)
    for i, (nm, bpm, sw) in enumerate([("Bottom Groove House 124bpm", 124, 0.0), ("Bottom Groove Swung 120bpm", 120, 0.18),
                                       ("Bottom Groove Breaks 128bpm", 128, 0.05), ("Bottom Groove Garage 130bpm", 130, 0.22),
                                       ("Bottom Groove Tech 126bpm", 126, 0.0)]):
        bars = 2
        k = kick(30 + i, tone=50 + i * 3, decay=0.3)
        s = clap(31 + i) if i % 2 else snare(31 + i)
        h = hat(32 + i, 0.03)
        kh = [0, 4, 8, 12] if i in (0, 4) else ([0, 6, 10] if i == 2 else [0, 7, 10])
        x = pattern(bpm, bars, kh, lambda st: k) + 0.7 * pattern(bpm, bars, [4, 12], lambda st: s, sw) \
            + 0.35 * pattern(bpm, bars, [2, 6, 10, 14] if i != 2 else list(range(0, 16, 2)), lambda st: h, sw)
        write(FOLDERS[2], nm, x, {"bpm": bpm, "bars": bars})
    # 04 Top: hats / rides / shakers
    for i, (nm, bpm, hits, fn, sw) in enumerate([
            ("Top Hats Offbeat 124bpm", 124, [2, 6, 10, 14], lambda: hat(40, 0.06, 7000), 0.0),
            ("Top Hats 16th 126bpm", 126, list(range(16)), lambda: hat(41, 0.025, 9000), 0.0),
            ("Top Hats Swing 120bpm", 120, list(range(16)), lambda: hat(42, 0.03, 8500), 0.2),
            ("Top Shaker 122bpm", 122, list(range(16)), lambda: shaker(43, 0.03), 0.12),
            ("Top Ride 128bpm", 128, [0, 2, 4, 6, 8, 10, 12, 14], lambda: hat(44, 0.25, 5000), 0.0)]):
        vel = [1.0, 0.55, 0.8, 0.55]
        write(FOLDERS[3], nm, pattern(bpm, 2, hits, lambda st: fn(), sw, vel), {"bpm": bpm, "bars": 2})
    # 05 Percussion
    for i, (nm, bpm, hits, fn) in enumerate([
            ("Perc Congas 124bpm", 124, [0, 3, 6, 10, 11, 14], lambda st: conga(50, [260, 340, 220][st % 3])),
            ("Perc Congas Latin 120bpm", 120, [0, 2, 3, 7, 8, 11, 14], lambda st: conga(51, [240, 320][st % 2], 0.15)),
            ("Perc Toms 126bpm", 126, [3, 7, 11, 14, 15], lambda st: tom(52, [140, 180, 220][st % 3])),
            ("Perc Wood 128bpm", 128, [1, 5, 6, 9, 13], lambda st: conga(53, 900, 0.03)),
            ("Perc Blips 122bpm", 122, [2, 5, 9, 12, 15], lambda st: tom(54, 600 + 80 * (st % 4), 0.05))]):
        write(FOLDERS[4], nm, pattern(bpm, 2, hits, fn), {"bpm": bpm, "bars": 2})


# ------------------------------------------------------------------------------------------ tonal
def key_notes(root_name, minor):
    root = NOTE[root_name]
    return root, (MINOR if minor else MAJOR)


def bass_note(midi, n, kind, seed):
    f = mtof(midi)
    t = np.arange(n) / SR
    if kind == "sub":
        s = np.sin(2 * np.pi * f * t) + 0.15 * np.sin(4 * np.pi * f * t)
        e = np.minimum(1, t / 0.005) * np.exp(-t / 0.6)
        return s * e
    s = saw(f, n) + 0.5 * saw(f * 1.006, n, 0.3)
    bright = fft_filter(s, hi=1400 if kind == "rubber" else 900)
    dark = fft_filter(s, hi=260)
    e1 = np.exp(-t / (0.06 if kind == "rubber" else 0.12))
    sig = dark + (bright - dark) * e1
    return np.tanh(1.4 * sig) * np.minimum(1, t / 0.004) * np.exp(-t / 0.5)


def make_bass():
    specs = [("Bass Rubber 124bpm Emin", 124, "E", True, "rubber", [(0, 0, 3), (3, 0, 2), (6, 7, 2), (8, 0, 3), (11, 3, 2), (14, 5, 2)]),
             ("Bass Rubber 126bpm Gmin", 126, "G", True, "rubber", [(0, 0, 2), (2, 0, 2), (6, 10, 2), (8, 0, 2), (10, 3, 2), (14, 7, 2)]),
             ("Bass Rubber 120bpm Cmaj", 120, "C", False, "rubber", [(0, 0, 3), (4, 7, 2), (7, 0, 2), (10, 4, 2), (12, 5, 3)]),
             ("Bass Sub 122bpm Dmin", 122, "D", True, "sub", [(0, 0, 6), (8, 0, 4), (12, 5, 4)]),
             ("Bass Sub 128bpm Amin", 128, "A", True, "sub", [(0, 0, 4), (6, 0, 2), (8, 7, 4), (14, 3, 2)]),
             ("Bass Sub 124bpm Fmaj", 124, "F", False, "sub", [(0, 0, 4), (4, 0, 2), (8, 4, 4), (12, 7, 4)])]
    for i, (nm, bpm, rootn, minor, kind, notes) in enumerate(specs):
        bars = 2
        L = bar_len(bpm, bars)
        buf = np.zeros(L)
        root = NOTE[rootn]
        base = 36 + root if root < 8 else 24 + root     # octave 1-2
        s16 = 60.0 / bpm / 4 * SR
        for b in range(bars):
            for st, iv, ln in notes:
                n = int(ln * s16)
                place(buf, bass_note(base + iv + (12 if (b == 1 and st == 14) else 0), n, kind, i), step_pos(bpm, b * 16 + st))
        write(FOLDERS[5], nm, buf, {"bpm": bpm, "bars": bars, "key": rootn + ("m" if minor else "")})


def chord_tones(root, scale, degree, size=3, octave=60):
    out = []
    for k in range(size):
        idx = degree + 2 * k
        out.append(octave + root + scale[idx % 7] + 12 * (idx // 7))
    return out


def poly(notes, n, kind, seed):
    t = np.arange(n) / SR
    r = rng(seed)
    acc = np.zeros(n)
    for m in notes:
        f = mtof(m)
        if kind == "pad":
            s = sum(saw(f * (1 + d), n, r.random()) for d in (-0.007, 0.0, 0.006))
            s = fft_filter(s, hi=1800)
        elif kind == "keys":
            s = np.sin(2 * np.pi * f * t) + 0.35 * np.sin(4 * np.pi * f * t) * np.exp(-t / 0.3) + 0.12 * np.sin(6 * np.pi * f * t) * np.exp(-t / 0.15)
        elif kind == "stab":
            s = fft_filter(saw(f, n) + saw(f * 1.004, n, 0.5), hi=3000)
        elif kind == "organ":
            s = np.sin(2 * np.pi * f * t) + 0.5 * np.sin(4 * np.pi * f * t) + 0.3 * np.sin(6 * np.pi * f * t)
        else:
            s = np.sin(2 * np.pi * f * t)
        acc += s
    return acc / max(1, len(notes))


def make_melodic():
    # 07 Chords & stabs
    specs = [("Chords Keys 124bpm Amin", 124, "A", True, "keys", [0, 5, 3, 4], 2),
             ("Chords Keys 120bpm Dmaj", 120, "D", False, "keys", [0, 4, 5, 3], 2),
             ("Chords Organ 126bpm Fmin", 126, "F", True, "organ", [0, 3, 6, 4], 2),
             ("Stabs House 124bpm Cmin", 124, "C", True, "stab", [0, 0, 5, 3], 1),
             ("Stabs Deep 122bpm Ebmaj", 122, "D#", False, "stab", [0, 3, 4, 0], 1)]
    for i, (nm, bpm, rootn, minor, kind, degs, bars) in enumerate(specs):
        root, sc = key_notes(rootn, minor)
        L = bar_len(bpm, bars)
        buf = np.zeros(L)
        s16 = 60.0 / bpm / 4 * SR
        steps_per = 16 * bars // len(degs)
        for j, dg in enumerate(degs):
            notes = chord_tones(root, sc, dg, 4 if kind != "stab" else 3, 48 if root > 5 else 60)
            if kind == "stab":
                for st in (0, 3, 6):
                    n = int(1.5 * s16)
                    x = poly(notes, n, kind, i) * env_ad(n, 0.003, 0.09)
                    place(buf, x, step_pos(bpm, j * steps_per + st))
            else:
                n = int(steps_per * s16)
                x = poly(notes, n, kind, i) * env_ad(n, 0.006, 0.9 if kind == "keys" else 5.0)
                place(buf, x, step_pos(bpm, j * steps_per))
        write(FOLDERS[6], nm, buf, {"bpm": bpm, "bars": bars, "key": rootn + ("m" if minor else "")})
    # 08 Pads
    specs = [("Pad Warm 124bpm Emin", 124, "E", True, [0, 5, 3, 6]),
             ("Pad Warm 120bpm Gmaj", 120, "G", False, [0, 3, 4, 5]),
             ("Pad Air 126bpm Bmin", 126, "B", True, [0, 0, 5, 5]),
             ("Pad Air 122bpm Cmaj", 122, "C", False, [0, 5, 3, 4])]
    for i, (nm, bpm, rootn, minor, degs) in enumerate(specs):
        root, sc = key_notes(rootn, minor)
        bars = 4
        L = bar_len(bpm, bars)
        buf = np.zeros(L)
        seg = L // len(degs)
        for j, dg in enumerate(degs):
            notes = chord_tones(root, sc, dg, 4, 48 if root > 3 else 60)
            n = seg + int(0.4 * SR)
            t = np.arange(n) / SR
            e = np.minimum(1, t / 0.35) * np.where(t > seg / SR, np.exp(-(t - seg / SR) / 0.12), 1)
            x = poly(notes, n, "pad", 70 + i) * e
            if "Air" in nm:
                x = fft_filter(x, lo=300)
            place(buf, x, j * seg)
        write(FOLDERS[7], nm, buf, {"bpm": bpm, "bars": bars, "key": rootn + ("m" if minor else "")})
    # 09 Synth (arps / plucks / leads)
    specs = [("Synth Arp 124bpm Amin", 124, "A", True, "pluck", [0, 2, 4, 7, 4, 2, 0, 4]),
             ("Synth Arp 128bpm Dmin", 128, "D", True, "pluck", [0, 4, 2, 7, 0, 4, 2, 9]),
             ("Synth Pluck 120bpm Emaj", 120, "E", False, "pluck", [0, 4, 7, 4, 9, 7, 4, 2]),
             ("Synth Lead 126bpm Gmin", 126, "G", True, "lead", [0, 0, 3, 5, 7, 5, 3, 2]),
             ("Synth Lead 122bpm Fmaj", 122, "F", False, "lead", [0, 2, 4, 5, 4, 2, 0, 7])]
    for i, (nm, bpm, rootn, minor, kind, seq) in enumerate(specs):
        root, sc = key_notes(rootn, minor)
        bars = 2
        L = bar_len(bpm, bars)
        buf = np.zeros(L)
        s16 = 60.0 / bpm / 4 * SR
        step = 2 if kind == "lead" else 1
        for k in range(0, 16 * bars, step):
            iv = seq[(k // step) % len(seq)]
            midi = 60 + root + (iv if iv in range(12) else iv)  # interval in semitones from root (scale-safe values)
            # map scale degrees: values are scale steps (0..9)
            deg = iv
            midi = 60 + root + sc[deg % 7] + 12 * (deg // 7)
            n = int(s16 * (step * 0.95 if kind == "lead" else 2.0))
            t = np.arange(n) / SR
            f = mtof(midi)
            raw = saw(f, n) + (0.6 * saw(f * 2.003, n) if kind == "pluck" else 0.5 * np.sign(np.sin(2 * np.pi * f * t)))
            bright = fft_filter(raw, hi=5000)
            dark = fft_filter(raw, hi=700)
            e = np.exp(-t / (0.08 if kind == "pluck" else 0.4))
            x = (dark + (bright - dark) * e) * env_ad(n, 0.002, 0.25 if kind == "pluck" else 1.2)
            if kind == "pluck" and (k % 3 == 2):
                x *= 0.6
            place(buf, x, step_pos(bpm, k))
        write(FOLDERS[8], nm, buf, {"bpm": bpm, "bars": bars, "key": rootn + ("m" if minor else "")})


if __name__ == "__main__":
    make_drums()
    make_bass()
    make_melodic()
    with open(os.path.join(ROOT, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    total = sum(len(v) for v in manifest["folders"].values())
    print(f"{total} loops written to {ROOT}")
