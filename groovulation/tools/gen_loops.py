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
           "07 Melodic 01 Chords-Stabs", "08 Melodic 02 Pads", "09 Melodic 03 Synth", "10 Vocals", "11 FX"]
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
MAJOR = [0, 2, 4, 5, 7, 9, 11]
MINOR = [0, 2, 3, 5, 7, 8, 10]
manifest = {"version": 4, "folders": {}}


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


def drum_tom(seed, f0=200, decay=0.22):
    """Membrane tom: inharmonic modes (1, 1.59, 2.14), small pitch drop, noisy stick / skin attack."""
    n = int(SR * decay * 4)
    t = np.arange(n) / SR
    r = rng(seed)
    bend = 1 + 0.12 * np.exp(-t / 0.04)
    s = np.zeros(n)
    for ratio, amp, dk in ((1.0, 1.0, 1.0), (1.59, 0.55, 0.6), (2.14, 0.35, 0.4)):
        s += amp * np.sin(np.cumsum(2 * np.pi * f0 * ratio * bend / SR)) * np.exp(-t / (decay * dk))
    s += 0.5 * fft_filter(r.standard_normal(n), lo=1200, hi=5000) * np.exp(-t / 0.012)
    return s


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


# ------------------------------------------------------------------------------------------ genres
# name, bpm, swing, key (root, minor) for the tonal parts
GENRES = [
    ("House",       124, 0.00, "A",  True),
    ("Deep House",  121, 0.12, "F",  True),
    ("Micro House", 125, 0.20, "D",  True),
    ("Minimal",     127, 0.05, "G",  True),
    ("Techno",      132, 0.00, "E",  True),
]


def make_drums():
    for gi, (genre, bpm, sw, _, _) in enumerate(GENRES):
        seed = 100 * gi
        # 01 Kick
        kp = {"House": dict(tone=50, punch=0.05, decay=0.32, click=0.3), "Deep House": dict(tone=44, punch=0.07, decay=0.45, click=0.15),
              "Micro House": dict(tone=56, punch=0.025, decay=0.16, click=0.5), "Minimal": dict(tone=52, punch=0.035, decay=0.22, click=0.45),
              "Techno": dict(tone=47, punch=0.04, decay=0.38, click=0.6)}[genre]
        k = kick(seed + 1, **kp)
        if genre == "Techno":
            k = np.tanh(k * 2.2)
        hits = {"Micro House": [0, 4, 8, 12, 15], "Minimal": [0, 4, 8, 11, 12], "Deep House": [0, 4, 8, 12]}.get(genre, [0, 4, 8, 12])
        write(FOLDERS[0], f"{genre} Kick {bpm}bpm", pattern(bpm, 2, hits, lambda st: k), {"bpm": bpm, "bars": 2, "genre": genre})
        # 02 Snare / clap
        if genre in ("House", "Deep House"):
            snd = clap(seed + 2, 0.22 if genre == "Deep House" else 0.16); hits = [4, 12]; nm = "Clap"
        elif genre == "Micro House":
            snd = snare(seed + 2, 420, 0.035, 0.45); hits = [4, 7, 12, 14]; nm = "Snare Rim"
        elif genre == "Minimal":
            snd = clap(seed + 2, 0.07); hits = [4, 12, 15]; nm = "Clap Snap"
        else:
            snd = snare(seed + 2, 190, 0.18, 0.85); hits = [4, 12]; nm = "Snare"
        write(FOLDERS[1], f"{genre} {nm} {bpm}bpm", pattern(bpm, 2, hits, lambda st: snd, sw), {"bpm": bpm, "bars": 2, "genre": genre})
        # 03 Bottom: toms and mid-low percussion, NO kick
        tp = {"House": ([2, 7, 10, 14], [130, 165, 210]), "Deep House": ([3, 6, 11, 13], [115, 140, 175]),
              "Micro House": ([1, 5, 6, 9, 13, 14], [180, 220, 260]), "Minimal": ([3, 10, 14], [150, 190, 230]),
              "Techno": ([2, 3, 6, 10, 11, 14], [105, 125, 150])}[genre]
        # (no hits on the four kick positions 0/4/8/12, tuned above the kick range)
        tp = {"House": ([2, 7, 10, 14], [190, 240, 290]), "Deep House": ([3, 6, 11, 13], [170, 210, 255]),
              "Micro House": ([1, 5, 6, 9, 13, 14], [230, 280, 330]), "Minimal": ([3, 10, 14], [200, 250, 300]),
              "Techno": ([2, 3, 6, 10, 11, 14], [175, 215, 260])}[genre]
        x = pattern(bpm, 2, tp[0], lambda st: drum_tom(seed + 3 + st, tp[1][st % 3], 0.2), sw) \
            + 0.4 * pattern(bpm, 2, [1, 5, 9, 15] if genre != "Micro House" else [7, 15], lambda st: conga(seed + 4, 240, 0.08), sw)
        write(FOLDERS[2], f"{genre} Bottom Toms No Kick {bpm}bpm", x, {"bpm": bpm, "bars": 2, "genre": genre})
        # 04 Top
        if genre == "House":
            x = pattern(bpm, 2, [2, 6, 10, 14], lambda st: hat(seed + 4, 0.08, 7000), sw) + 0.4 * pattern(bpm, 2, list(range(16)), lambda st: shaker(seed + 5), sw, [1, .5, .8, .5])
        elif genre == "Deep House":
            x = pattern(bpm, 2, list(range(16)), lambda st: hat(seed + 4, 0.035, 8000), sw, [1, .45, .7, .45]) + 0.5 * pattern(bpm, 2, [2, 6, 10, 14], lambda st: hat(seed + 6, 0.2, 6000), sw)
        elif genre == "Micro House":
            x = pattern(bpm, 2, [1, 2, 5, 7, 9, 10, 13, 15], lambda st: shaker(seed + 4, 0.02), sw, [1, .6, .8, .5])
        elif genre == "Minimal":
            x = pattern(bpm, 2, [2, 6, 10, 14, 15], lambda st: hat(seed + 4, 0.02, 10000), sw)
        else:
            x = pattern(bpm, 2, list(range(16)), lambda st: hat(seed + 4, 0.03, 9000), 0.0, [1, .55, .85, .55]) + 0.6 * pattern(bpm, 2, [0, 2, 4, 6, 8, 10, 12, 14], lambda st: hat(seed + 6, 0.3, 4500))
        write(FOLDERS[3], f"{genre} Top Hats {bpm}bpm", x, {"bpm": bpm, "bars": 2, "genre": genre})
        # 05 Percussion
        if genre == "House":
            fn = lambda st: conga(seed + 7, [260, 340, 220][st % 3]); hits = [0, 3, 6, 10, 11, 14]
        elif genre == "Deep House":
            fn = lambda st: conga(seed + 7, [240, 320][st % 2], 0.15); hits = [0, 2, 3, 7, 8, 11, 14]
        elif genre == "Micro House":
            fn = lambda st: conga(seed + 7, 900 + 150 * (st % 3), 0.025); hits = [1, 5, 6, 9, 13, 14]
        elif genre == "Minimal":
            fn = lambda st: tom(seed + 7, 600 + 80 * (st % 4), 0.05); hits = [2, 5, 9, 12, 15]
        else:
            fn = lambda st: tom(seed + 7, [110, 140, 170][st % 3], 0.2); hits = [3, 7, 11, 14, 15]
        write(FOLDERS[4], f"{genre} Perc {bpm}bpm", pattern(bpm, 2, hits, fn, sw), {"bpm": bpm, "bars": 2, "genre": genre})


def make_bass():
    lines = {
        "House":       ("rubber", [(0, 0, 3), (3, 0, 2), (6, 7, 2), (8, 0, 3), (11, 3, 2), (14, 5, 2)]),
        "Deep House":  ("sub",    [(0, 0, 6), (8, 0, 4), (12, 5, 4)]),
        "Micro House": ("rubber", [(2, 0, 1), (6, 0, 1), (7, 7, 1), (10, 0, 1), (14, 3, 1)]),
        "Minimal":     ("sub",    [(2, 0, 2), (6, 0, 2), (10, 0, 2), (14, 10, 2)]),
        "Techno":      ("rubber", [(2, 0, 2), (6, 0, 2), (10, 0, 2), (14, 0, 2)]),
    }
    for genre, bpm, sw, rootn, minor in GENRES:
        kind, notes = lines[genre]
        L = bar_len(bpm, 2)
        buf = np.zeros(L)
        root = NOTE[rootn]
        base = 36 + root if root < 8 else 24 + root
        s16 = 60.0 / bpm / 4 * SR
        for b in range(2):
            for st, iv, ln in notes:
                place(buf, bass_note(base + iv, int(ln * s16), kind, b), step_pos(bpm, b * 16 + st, sw))
        write(FOLDERS[5], f"{genre} Bass {bpm}bpm {rootn}{'min' if minor else 'maj'}", buf, {"bpm": bpm, "bars": 2, "genre": genre})


def make_melodic():
    for gi, (genre, bpm, sw, rootn, minor) in enumerate(GENRES):
        root, sc = key_notes(rootn, minor)
        s16 = 60.0 / bpm / 4 * SR
        oct_ = 48 if root > 5 else 60
        kmin = "min" if minor else "maj"
        # 07 Chords & stabs
        L = bar_len(bpm, 2); buf = np.zeros(L)
        if genre in ("House", "Deep House"):
            kind = "keys" if genre == "Deep House" else "organ"
            degs = [0, 5, 3, 4]
            for j, dg in enumerate(degs):
                notes = chord_tones(root, sc, dg, 4, oct_)
                n = int(8 * s16)
                if genre == "House":   # offbeat organ stabs
                    for st in (2, 6):
                        m = int(1.6 * s16)
                        place(buf, poly(notes, m, kind, gi) * env_ad(m, 0.004, 0.15), step_pos(bpm, j * 8 + st, sw))
                else:
                    place(buf, poly(notes, n, kind, gi) * env_ad(n, 0.006, 0.9), step_pos(bpm, j * 8, sw))
            nm = "Chords"
        else:
            hits = {"Micro House": [0, 3, 10, 13, 22, 26], "Minimal": [3, 19], "Techno": [0, 6, 12, 16, 22, 28]}[genre]
            notes = chord_tones(root, sc, 0, 3, oct_)
            for st in hits:
                m = int(1.5 * s16)
                x = poly(notes, m, "stab", gi) * env_ad(m, 0.003, 0.07 if genre != "Minimal" else 0.25)
                if genre == "Techno":
                    x = fft_filter(np.tanh(2 * x), hi=2200)
                place(buf, x, step_pos(bpm, st, sw))
            nm = "Stabs"
        write(FOLDERS[6], f"{genre} {nm} {bpm}bpm {rootn}{kmin}", buf, {"bpm": bpm, "bars": 2, "genre": genre})
        # 08 Pads (4 bars)
        L = bar_len(bpm, 4); buf = np.zeros(L)
        degs = {"House": [0, 5, 3, 4], "Deep House": [0, 3, 5, 6], "Micro House": [0, 0, 5, 5], "Minimal": [0, 0, 0, 0], "Techno": [0, 0, 5, 0]}[genre]
        seg = L // 4
        for j, dg in enumerate(degs):
            notes = chord_tones(root, sc, dg, 4 if genre != "Minimal" else 3, 48 if root > 3 else 60)
            n = seg + int(0.4 * SR)
            t = np.arange(n) / SR
            e = np.minimum(1, t / (0.35 if genre != "Techno" else 0.8)) * np.where(t > seg / SR, np.exp(-(t - seg / SR) / 0.12), 1)
            x = poly(notes, n, "pad", 70 + gi) * e
            if genre in ("Micro House", "Minimal"):
                x = fft_filter(x, lo=300, hi=3000)
            if genre == "Techno":
                x = fft_filter(x, hi=900)
            place(buf, x, j * seg)
        write(FOLDERS[7], f"{genre} Pad {bpm}bpm {rootn}{kmin}", buf, {"bpm": bpm, "bars": 4, "genre": genre})
        # 09 Synth
        seq = {"House": ("pluck", [0, 2, 4, 7, 4, 2, 0, 4], 1), "Deep House": ("lead", [0, 0, 2, 4, 2, 0, 6, 4], 2),
               "Micro House": ("pluck", [7, 0, 9, 4, 0, 7, 2, 0], 1), "Minimal": ("pluck", [0, 7, 0, 0, 9, 0, 7, 0], 1),
               "Techno": ("pluck", [0, 0, 7, 0, 3, 0, 9, 7], 1)}[genre]
        kind, notes_seq, step = seq
        L = bar_len(bpm, 2); buf = np.zeros(L)
        for kk in range(0, 32, step):
            if genre == "Minimal" and kk % 3 == 1:
                continue
            deg = notes_seq[(kk // step) % len(notes_seq)]
            midi = 60 + root + sc[deg % 7] + 12 * (deg // 7) - (12 if genre == "Techno" else 0)
            n = int(s16 * (step * 0.95 if kind == "lead" else 2.0))
            t = np.arange(n) / SR
            f = mtof(midi)
            raw = saw(f, n) + (0.6 * saw(f * 2.003, n) if kind == "pluck" else 0.5 * np.sign(np.sin(2 * np.pi * f * t)))
            bright = fft_filter(raw, hi=5000 if genre != "Techno" else 2500)
            dark = fft_filter(raw, hi=700 if genre != "Techno" else 300)
            e = np.exp(-t / (0.08 if kind == "pluck" else 0.4))
            x = (dark + (bright - dark) * e) * env_ad(n, 0.002, 0.25 if kind == "pluck" else 1.2)
            if genre == "Techno":
                x = np.tanh(2.5 * x)
            place(buf, x, step_pos(bpm, kk, sw))
        write(FOLDERS[8], f"{genre} Synth {'Lead' if kind == 'lead' else 'Seq'} {bpm}bpm {rootn}{kmin}", buf, {"bpm": bpm, "bars": 2, "genre": genre})


# ------------------------------------------------------------------------------------------ vocals / fx
VOWELS = {"a": [(800, 80), (1150, 90), (2900, 120)], "o": [(450, 70), (800, 80), (2830, 100)],
          "e": [(400, 60), (2000, 100), (2550, 120)], "u": [(325, 50), (700, 60), (2530, 120)]}


def voice(midi, n, vowel, seed):
    """Glottal pulse train (with vibrato and jitter) through three formant resonators."""
    r = rng(seed)
    t = np.arange(n) / SR
    f = mtof(midi) * (1 + 0.012 * np.sin(2 * np.pi * 5.2 * t) * np.minimum(1, t / 0.25)) * (1 + 0.002 * r.standard_normal(n).cumsum() / np.sqrt(n))
    ph = np.cumsum(f / SR) % 1.0
    src = np.where(ph < 0.6, np.sin(np.pi * ph / 0.6) ** 2, 0.0) - 0.35
    src += 0.03 * r.standard_normal(n)
    out = np.zeros(n)
    N = 1 << int(np.ceil(np.log2(n + 1)))
    X = np.fft.rfft(src, N)
    fr = np.fft.rfftfreq(N, 1 / SR)
    H = np.zeros_like(fr)
    for k, (fc, bw) in enumerate(VOWELS[vowel]):
        H += (1.0 / (1 + k)) / (1 + ((fr - fc) / bw) ** 2)
    out = np.fft.irfft(X * H, N)[:n]
    return out * np.minimum(1, t / 0.02) * np.minimum(1, (n / SR - t) / 0.04)


def make_vocals():
    phrases = {"House": [(0, 3, "a"), (4, 3, "o"), (8, 2, "a"), (12, 4, "e")], "Deep House": [(0, 6, "o"), (8, 6, "u")],
               "Micro House": [(1, 1, "a"), (3, 1, "e"), (6, 1, "a"), (9, 1, "o"), (11, 1, "a"), (14, 1, "e")],
               "Minimal": [(4, 2, "o"), (20, 2, "o")], "Techno": [(0, 2, "a"), (8, 2, "a"), (16, 2, "a"), (24, 4, "e")]}
    degs = {"House": [4, 2, 0, 4], "Deep House": [2, 0], "Micro House": [4, 7, 4, 2, 4, 0], "Minimal": [0, 0], "Techno": [0, 0, 0, 2]}
    for gi, (genre, bpm, sw, rootn, minor) in enumerate(GENRES):
        root, sc = key_notes(rootn, minor)
        L = bar_len(bpm, 2); buf = np.zeros(L)
        s16 = 60.0 / bpm / 4 * SR
        for j, (st, ln, vw) in enumerate(phrases[genre]):
            dg = degs[genre][j % len(degs[genre])]
            midi = 60 + root + sc[dg % 7] + 12 * (dg // 7) - (12 if root > 6 else 0)
            place(buf, voice(midi, int(ln * s16 * 0.95), vw, 500 + gi * 10 + j), step_pos(bpm, st, sw))
        kind = "Vocal Chop" if genre in ("Micro House", "Techno") else "Vocal Phrase"
        write(FOLDERS[9], f"{genre} {kind} {bpm}bpm {rootn}{'min' if minor else 'maj'}", buf, {"bpm": bpm, "bars": 2, "genre": genre})


def make_fx():
    for gi, (genre, bpm, sw, _, _) in enumerate(GENRES):
        r = rng(900 + gi)
        bars = 2 if genre != "Deep House" else 4
        L = bar_len(bpm, bars); t = np.arange(L) / SR; T = L / SR
        if genre == "House":          # white-noise riser
            nz = r.standard_normal(L)
            x = np.zeros(L)
            for k in range(8):        # sweeping band-pass, built in segments
                a, b = k * L // 8, (k + 1) * L // 8
                x[a:b] = fft_filter(nz[a:b], lo=400 * 2 ** (k * 0.6), hi=1200 * 2 ** (k * 0.6))
            x *= (t / T) ** 2; nm = "FX Riser"
        elif genre == "Deep House":   # down-lifter: falling filtered noise + falling tone
            f = 1800 * np.exp(-3 * t / T) + 80
            x = 0.4 * np.sin(np.cumsum(2 * np.pi * f / SR)) + 0.5 * fft_filter(r.standard_normal(L), lo=300, hi=4000)
            x *= np.exp(-2.5 * t / T); nm = "FX Downlifter"
        elif genre == "Micro House":  # glitchy zaps
            x = np.zeros(L)
            for st in [3, 7, 10, 19, 22, 27]:
                n = int(0.08 * SR); tt = np.arange(n) / SR
                x[int(step_pos(bpm, st)):][:n] += np.sin(2 * np.pi * np.cumsum(3000 * np.exp(-tt / 0.01) + 200) / SR)[: len(x[int(step_pos(bpm, st)):][:n])] * np.exp(-tt / 0.03)[: len(x[int(step_pos(bpm, st)):][:n])]
            nm = "FX Zaps"
        elif genre == "Minimal":      # reversed swell into the downbeat
            sw_ = fft_filter(r.standard_normal(L), lo=2000, hi=9000) * np.exp(-6 * t / T)
            x = sw_[::-1]; nm = "FX Reverse Swell"
        else:                         # impact: sub boom + noise burst
            n = L; tt = t
            f = 60 + 140 * np.exp(-tt / 0.05)
            x = np.sin(np.cumsum(2 * np.pi * f / SR)) * np.exp(-tt / 0.9) + 0.6 * r.standard_normal(n) * np.exp(-tt / 0.08)
            x = np.tanh(1.5 * x); nm = "FX Impact"
        write(FOLDERS[10], f"{genre} {nm} {bpm}bpm", x, {"bpm": bpm, "bars": bars, "genre": genre})


if __name__ == "__main__":
    make_drums()
    make_bass()
    make_melodic()
    make_vocals()
    make_fx()
    with open(os.path.join(ROOT, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    total = sum(len(v) for v in manifest["folders"].values())
    print(f"{total} loops written to {ROOT}")
