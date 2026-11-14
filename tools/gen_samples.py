#!/usr/bin/env python3
"""
Twinbox factory sample generator.

Renders the default sample folders for Section B ("Twinbox Kick", "Twinbox Snare", "Twinbox Hat",
"Twinbox Tie", "Twinbox Pod") as 44.1 kHz / 16-bit mono WAV files, plus a manifest with genre tags
that the preset generator uses to build genre-coherent kits.

Hats follow the Limelite convention: every closed hat "X.wav" may have an open partner in "open/X.wav".

Usage: python3 tools/gen_samples.py resources/samples
"""
import json, os, sys, wave
import numpy as np

SR = 44100
OUT = sys.argv[1] if len(sys.argv) > 1 else "resources/samples"
GENRES = ["techno", "minimal", "microhouse", "rominimal", "chicago", "techhouse", "deephouse", "breakbeat"]
manifest = {}


# ----------------------------------------------------------------------------------------- helpers
def t_axis(sec):
    return np.arange(int(sec * SR)) / SR

def rng(seed):
    return np.random.default_rng(seed)

def env_exp(t, tau):
    return np.exp(-t / max(tau, 1e-4))

def env_ad(t, a, d):
    e = np.where(t < a, t / max(a, 1e-5), np.exp(-(t - a) / max(d, 1e-4)))
    return e

def osc_sine(freq_curve):
    ph = np.cumsum(2 * np.pi * freq_curve / SR)
    return np.sin(ph)

def osc_saw(freq_curve):
    ph = np.cumsum(freq_curve / SR) % 1.0
    return 2 * ph - 1

def osc_square(freq_curve, pw=0.5):
    ph = np.cumsum(freq_curve / SR) % 1.0
    return np.where(ph < pw, 1.0, -1.0)

def svf(x, fc, q=0.707, mode="lp"):
    """Time-varying TPT state-variable filter. fc may be scalar or array."""
    n = len(x)
    fc = np.broadcast_to(np.asarray(fc, dtype=float), (n,))
    g = np.tan(np.pi * np.clip(fc, 10, SR * 0.45) / SR)
    k = 1.0 / q
    ic1 = ic2 = 0.0
    y = np.empty(n)
    for i in range(n):
        gi = g[i]
        a1 = 1 / (1 + gi * (gi + k)); a2 = gi * a1; a3 = gi * a2
        v3 = x[i] - ic2
        v1 = a1 * ic1 + a2 * v3
        v2 = ic2 + a2 * ic1 + a3 * v3
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2
        y[i] = v2 if mode == "lp" else (k * v1 if mode == "bp" else x[i] - k * v1 - v2)
    return y

def onepole_hp(x, fc):
    a = np.exp(-2 * np.pi * fc / SR)
    y = np.empty_like(x); prev_x = prev_y = 0.0
    for i in range(len(x)):
        prev_y = a * (prev_y + x[i] - prev_x); prev_x = x[i]; y[i] = prev_y
    return y

def sat(x, drive):
    return np.tanh(x * drive) / np.tanh(drive)

def fade_tail(x, ms=4):
    n = min(len(x), int(SR * ms / 1000))
    if n > 0:
        x[-n:] *= np.linspace(1, 0, n)
    return x

def declick_head(x, ms=0.3):
    n = max(1, int(SR * ms / 1000))
    x[:n] *= np.linspace(0, 1, n)
    return x

def normalize(x, peak=0.89):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x

def trim(x, thresh=1e-4):
    idx = np.where(np.abs(x) > thresh)[0]
    return x[: idx[-1] + 1] if len(idx) else x

def write(module, name, x, tags, subdir=None):
    x = fade_tail(declick_head(normalize(trim(np.asarray(x, dtype=float)))))
    folder = os.path.join(OUT, f"Twinbox {module}", *( [subdir] if subdir else [] ))
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name + ".wav")
    data = (np.clip(x, -1, 1) * 32767).astype("<i2").tobytes()
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(data)
    if subdir is None:
        manifest.setdefault(module, []).append({"file": name + ".wav", "tags": tags})


# ----------------------------------------------------------------------------------------- KICKS
def kick(f_end, f_start, p_tau, a_tau, length, click=0.3, drive=1.5, noise=0.0, sub=0.0, body_hp=0, seed=1, bend=0.0):
    t = t_axis(length); r = rng(seed)
    f = f_end + (f_start - f_end) * env_exp(t, p_tau) + bend * env_exp(t, p_tau * 6)
    x = osc_sine(f) * env_exp(t, a_tau)
    if sub > 0:
        x += sub * osc_sine(np.full_like(t, f_end * 0.5)) * env_exp(t, a_tau * 1.4)
    clk = r.standard_normal(len(t)) * env_exp(t, 0.0012)
    clk = svf(clk, 3500, 0.8, "bp") if click > 0 else clk * 0
    x += click * clk * 3
    if noise > 0:
        x += noise * svf(r.standard_normal(len(t)), 1800, 0.7, "bp") * env_exp(t, 0.025)
    x = sat(x, drive)
    if body_hp:
        x = onepole_hp(x, body_hp)
    return x

KICKS = [
    # name, tags, params
    ("Rumble Core",        ["techno"],                dict(f_end=46, f_start=210, p_tau=0.035, a_tau=0.42, length=1.2, click=0.35, drive=3.0, sub=0.2)),
    ("Industrial Stomp",   ["techno"],                dict(f_end=52, f_start=320, p_tau=0.02, a_tau=0.22, length=0.7, click=0.8, drive=6.0, noise=0.3)),
    ("Warehouse 9",        ["techno", "techhouse"],   dict(f_end=50, f_start=260, p_tau=0.028, a_tau=0.28, length=0.8, click=0.55, drive=2.6)),
    ("Berghain Floor",     ["techno"],                dict(f_end=44, f_start=180, p_tau=0.045, a_tau=0.55, length=1.4, click=0.25, drive=4.0, sub=0.3)),
    ("Hard Lane",          ["techno"],                dict(f_end=55, f_start=400, p_tau=0.015, a_tau=0.18, length=0.6, click=1.0, drive=7.0)),
    ("Minimal Thud",       ["minimal", "techhouse"],  dict(f_end=54, f_start=170, p_tau=0.03, a_tau=0.2, length=0.6, click=0.2, drive=1.4)),
    ("Dry Point",          ["minimal"],               dict(f_end=58, f_start=230, p_tau=0.018, a_tau=0.12, length=0.4, click=0.5, drive=1.6)),
    ("Plastic Kick",       ["minimal", "microhouse"], dict(f_end=62, f_start=300, p_tau=0.012, a_tau=0.1, length=0.35, click=0.7, drive=2.0, body_hp=30)),
    ("Micro Click",        ["microhouse"],            dict(f_end=70, f_start=500, p_tau=0.006, a_tau=0.07, length=0.25, click=1.0, drive=1.8, body_hp=45)),
    ("Felt Bump",          ["microhouse", "rominimal"], dict(f_end=56, f_start=140, p_tau=0.03, a_tau=0.14, length=0.45, click=0.05, drive=1.1)),
    ("Wood Body",          ["microhouse"],            dict(f_end=64, f_start=190, p_tau=0.02, a_tau=0.09, length=0.3, click=0.4, drive=1.3, noise=0.15)),
    ("Bucharest Round",    ["rominimal"],             dict(f_end=50, f_start=120, p_tau=0.04, a_tau=0.26, length=0.8, click=0.08, drive=1.15, sub=0.15)),
    ("Soft Rubber",        ["rominimal", "minimal"],  dict(f_end=53, f_start=110, p_tau=0.05, a_tau=0.2, length=0.6, click=0.02, drive=1.05)),
    ("Deep Pulse Ro",      ["rominimal"],             dict(f_end=47, f_start=100, p_tau=0.06, a_tau=0.34, length=0.9, click=0.04, drive=1.2, sub=0.25)),
    ("Velvet Knock",       ["rominimal", "deephouse"], dict(f_end=60, f_start=150, p_tau=0.025, a_tau=0.15, length=0.45, click=0.12, drive=1.25)),
    ("Long Sub Ro",        ["rominimal", "minimal"],  dict(f_end=42, f_start=95, p_tau=0.08, a_tau=0.6, length=1.5, click=0.03, drive=1.1, sub=0.3)),
    ("Jack 909",           ["chicago"],               dict(f_end=52, f_start=280, p_tau=0.022, a_tau=0.3, length=0.8, click=0.6, drive=2.2, noise=0.2)),
    ("Box 707",            ["chicago"],               dict(f_end=60, f_start=240, p_tau=0.02, a_tau=0.16, length=0.5, click=0.45, drive=1.6)),
    ("Warehouse Jack",     ["chicago", "techhouse"],  dict(f_end=55, f_start=300, p_tau=0.025, a_tau=0.24, length=0.7, click=0.7, drive=3.0)),
    ("Grit Machine",       ["chicago"],               dict(f_end=50, f_start=260, p_tau=0.03, a_tau=0.35, length=0.9, click=0.5, drive=5.0, noise=0.25)),
    ("Tech Punch",         ["techhouse"],             dict(f_end=53, f_start=250, p_tau=0.025, a_tau=0.25, length=0.7, click=0.5, drive=2.4, sub=0.1)),
    ("Tight Groove",       ["techhouse", "minimal"],  dict(f_end=57, f_start=280, p_tau=0.018, a_tau=0.16, length=0.5, click=0.6, drive=2.0)),
    ("Round 808",          ["deephouse", "chicago"],  dict(f_end=45, f_start=110, p_tau=0.05, a_tau=0.5, length=1.4, click=0.1, drive=1.1, sub=0.2)),
    ("Deep Soft",          ["deephouse"],             dict(f_end=50, f_start=140, p_tau=0.035, a_tau=0.3, length=0.9, click=0.12, drive=1.2)),
    ("Warm Felt",          ["deephouse"],             dict(f_end=55, f_start=160, p_tau=0.03, a_tau=0.22, length=0.7, click=0.15, drive=1.35, noise=0.08)),
    ("Lofi Bump",          ["deephouse", "microhouse"], dict(f_end=58, f_start=180, p_tau=0.025, a_tau=0.18, length=0.55, click=0.2, drive=2.0, body_hp=35)),
    ("Break Beater",       ["breakbeat"],             dict(f_end=62, f_start=200, p_tau=0.02, a_tau=0.14, length=0.5, click=0.9, drive=2.5, noise=0.45)),
    ("Funk Kick",          ["breakbeat"],             dict(f_end=58, f_start=180, p_tau=0.025, a_tau=0.2, length=0.55, click=0.7, drive=3.0, noise=0.35)),
    ("Amen Low",           ["breakbeat"],             dict(f_end=66, f_start=220, p_tau=0.015, a_tau=0.12, length=0.45, click=1.0, drive=4.0, noise=0.6)),
    ("Electro Boom",       ["breakbeat", "techno"],   dict(f_end=40, f_start=260, p_tau=0.06, a_tau=0.7, length=1.6, click=0.4, drive=2.0, sub=0.2)),
    ("Bent Tom Kick",      ["minimal", "techno"],     dict(f_end=70, f_start=180, p_tau=0.06, a_tau=0.25, length=0.7, click=0.2, drive=1.6, bend=30)),
    ("Ghost Kick",         ["minimal", "rominimal", "microhouse"], dict(f_end=60, f_start=90, p_tau=0.04, a_tau=0.1, length=0.3, click=0.0, drive=1.0)),
]
for i, (name, tags, p) in enumerate(KICKS):
    write("Kick", f"K{i+1:02d} {name}", kick(seed=100 + i, **p), tags)


# ----------------------------------------------------------------------------------------- SNARES / CLAPS / RIMS
def snare(tone, t_tau, n_tau, n_fc, n_mode="bp", mix=0.6, length=0.5, drive=1.3, snap=0.0, seed=1, q=0.9):
    t = t_axis(length); r = rng(seed)
    f = tone * (1 + 0.6 * env_exp(t, 0.008))
    body = (osc_sine(f) + 0.45 * osc_sine(f * 1.62)) * env_exp(t, t_tau)
    nz = svf(r.standard_normal(len(t)), n_fc, q, n_mode) * env_exp(t, n_tau)
    x = body * (1 - mix) + nz * mix * 1.6
    if snap:
        x += snap * svf(r.standard_normal(len(t)), 6000, 1.2, "hp") * env_exp(t, 0.004)
    return sat(x, drive)

def clap(fc, spacing, bursts, tail, length=0.6, q=1.6, seed=1, room=0.0):
    t = t_axis(length); r = rng(seed)
    e = np.zeros_like(t)
    for b in range(bursts):
        tb = t - b * spacing * (1 + 0.15 * r.standard_normal())
        e = np.maximum(e, np.where(tb >= 0, env_exp(np.maximum(tb, 0), spacing * 0.35), 0))
    tl = t - (bursts - 1) * spacing
    e = np.maximum(e, np.where(tl >= 0, env_exp(np.maximum(tl, 0), tail), 0))
    x = svf(r.standard_normal(len(t)), fc, q, "bp") * e
    if room:
        x += room * svf(r.standard_normal(len(t)), fc * 0.7, 0.6, "bp") * env_exp(t, tail * 4) * 0.3
    return x

def rim(freq, decay, length=0.25, noise=0.3, seed=1):
    t = t_axis(length); r = rng(seed)
    x = (osc_sine(np.full_like(t, freq)) + 0.6 * osc_sine(np.full_like(t, freq * 2.37))) * env_exp(t, decay)
    x += noise * svf(r.standard_normal(len(t)), freq * 3, 1.5, "bp") * env_exp(t, decay * 0.4)
    return sat(x, 1.8)

SNARES = [
    ("Tek Snare",       ["techno"],                lambda s: snare(190, 0.04, 0.12, 3500, mix=0.7, drive=2.5, seed=s)),
    ("Metal Crack",     ["techno"],                lambda s: snare(240, 0.03, 0.09, 5000, "hp", mix=0.8, drive=4.0, snap=0.5, seed=s)),
    ("Noise Gate",      ["techno", "breakbeat"],   lambda s: snare(200, 0.02, 0.2, 2500, mix=0.95, drive=3.0, length=0.45, seed=s)),
    ("Warehouse Clap",  ["techno", "techhouse"],   lambda s: clap(1300, 0.011, 4, 0.16, seed=s, room=0.6)),
    ("Min Rim",         ["minimal"],               lambda s: rim(850, 0.03, seed=s)),
    ("Dry Clap",        ["minimal", "techhouse"],  lambda s: clap(1600, 0.008, 3, 0.07, length=0.35, seed=s)),
    ("Snap Click",      ["minimal", "microhouse"], lambda s: snare(320, 0.008, 0.035, 7000, "hp", mix=0.85, length=0.2, snap=0.8, seed=s)),
    ("Paper Snare",     ["minimal"],               lambda s: snare(210, 0.02, 0.06, 4200, mix=0.75, length=0.3, seed=s)),
    ("Micro Tick",      ["microhouse"],            lambda s: rim(2200, 0.008, length=0.12, noise=0.6, seed=s)),
    ("Glass Rim",       ["microhouse", "rominimal"], lambda s: rim(1450, 0.02, length=0.2, noise=0.2, seed=s)),
    ("Finger Snap",     ["microhouse", "deephouse"], lambda s: clap(2600, 0.004, 2, 0.03, length=0.2, q=1.2, seed=s)),
    ("Bucharest Rim",   ["rominimal"],             lambda s: rim(620, 0.045, noise=0.15, seed=s)),
    ("Wood Clap Ro",    ["rominimal"],             lambda s: clap(900, 0.014, 3, 0.05, length=0.3, q=2.2, seed=s)),
    ("Soft Shuffle",    ["rominimal", "minimal"],  lambda s: snare(170, 0.02, 0.08, 2000, mix=0.6, drive=1.0, length=0.35, seed=s)),
    ("Dust Snare",      ["rominimal", "microhouse"], lambda s: snare(260, 0.015, 0.05, 6000, "hp", mix=0.9, drive=1.2, length=0.25, seed=s)),
    ("Hollow Knock",    ["rominimal"],             lambda s: rim(410, 0.06, length=0.3, noise=0.1, seed=s)),
    ("Jack Clap",       ["chicago"],               lambda s: clap(1150, 0.012, 4, 0.14, seed=s, room=0.4)),
    ("909 Snare",       ["chicago", "techhouse"],  lambda s: snare(180, 0.07, 0.15, 3500, mix=0.65, drive=1.6, seed=s)),
    ("Box Snare",       ["chicago"],               lambda s: snare(220, 0.05, 0.1, 2800, mix=0.55, drive=2.0, length=0.4, seed=s)),
    ("Clap Stack",      ["chicago", "deephouse"],  lambda s: clap(1050, 0.013, 5, 0.22, length=0.8, seed=s, room=0.8)),
    ("Tech Clap",       ["techhouse"],             lambda s: clap(1400, 0.01, 4, 0.1, length=0.45, seed=s, room=0.3)),
    ("Tight Snare",     ["techhouse", "minimal"],  lambda s: snare(200, 0.04, 0.08, 4000, mix=0.7, drive=1.8, length=0.35, seed=s)),
    ("Deep Clap",       ["deephouse"],             lambda s: clap(950, 0.015, 4, 0.25, length=0.9, q=1.3, seed=s, room=1.0)),
    ("Soft Rim",        ["deephouse"],             lambda s: rim(700, 0.04, noise=0.08, seed=s)),
    ("Velvet Snare",    ["deephouse"],             lambda s: snare(165, 0.06, 0.18, 2200, mix=0.6, drive=1.1, length=0.6, seed=s)),
    ("Break Snare",     ["breakbeat"],             lambda s: snare(230, 0.06, 0.16, 3000, mix=0.6, drive=3.5, snap=0.4, seed=s)),
    ("Ghost Snare",     ["breakbeat", "minimal"],  lambda s: snare(240, 0.02, 0.05, 3800, mix=0.65, drive=1.5, length=0.2, seed=s)),
    ("Crack Break",     ["breakbeat"],             lambda s: snare(260, 0.04, 0.22, 4500, mix=0.75, drive=5.0, snap=0.7, length=0.6, seed=s)),
    ("Electro Snare",   ["breakbeat", "techno"],   lambda s: snare(300, 0.05, 0.1, 5000, "hp", mix=0.6, drive=2.2, seed=s)),
    ("Lofi Clap",       ["breakbeat", "deephouse"], lambda s: sat(clap(1200, 0.012, 3, 0.12, seed=s) * 2, 2.5)),
    ("Room Rim",        ["techhouse", "rominimal"], lambda s: rim(900, 0.03, length=0.4, noise=0.4, seed=s)),
    ("Flam Clap",       ["minimal", "microhouse"], lambda s: clap(1800, 0.022, 2, 0.06, length=0.3, seed=s)),
]
for i, (name, tags, fn) in enumerate(SNARES):
    write("Snare", f"S{i+1:02d} {name}", fn(200 + i), tags)


# ----------------------------------------------------------------------------------------- HATS (closed + open pair)
def hat(tau, hp, metal, base, length, seed=1, bp=None, shaker=False):
    t = t_axis(length); r = rng(seed)
    ratios = [2.0, 3.0, 4.16, 5.43, 6.79, 8.21]
    m = sum(osc_square(np.full_like(t, base * k)) for k in ratios) / 6
    n = r.standard_normal(len(t))
    x = metal * m + (1 - metal) * n
    x = svf(x, hp, 0.8, "hp")
    if bp:
        x = svf(x, bp, 1.2, "bp") * 1.5 + x * 0.3
    e = env_exp(t, tau)
    if shaker:
        e = env_ad(t, 0.012, tau)
    return x * e

HATS = [
    # name, tags, closed(tau,hp,metal,base,len), open tau, extra
    ("808 Metal",     ["chicago", "techno"],       (0.035, 7000, 0.85, 360), 0.32, {}),
    ("909 Noise",     ["chicago", "techhouse"],    (0.045, 8000, 0.3, 420), 0.40, {}),
    ("Tick",          ["minimal", "microhouse"],   (0.012, 9500, 0.5, 500), 0.12, {}),
    ("Dark Hat",      ["techno", "minimal"],       (0.05, 4500, 0.6, 300), 0.35, {}),
    ("Shaker",        ["deephouse", "rominimal"],  (0.07, 5000, 0.0, 400), 0.22, dict(shaker=True, bp=7000)),
    ("Steel",         ["techno"],                  (0.04, 6500, 1.0, 470), 0.5, {}),
    ("Micro Dust",    ["microhouse", "rominimal"], (0.01, 11000, 0.2, 520), 0.09, {}),
    ("Ro Shuffle",    ["rominimal"],               (0.03, 6500, 0.1, 410), 0.16, dict(bp=8500)),
    ("Ro Tsk",        ["rominimal", "minimal"],    (0.02, 8500, 0.35, 450), 0.2, {}),
    ("Paper Hat",     ["minimal"],                 (0.025, 7500, 0.2, 380), 0.25, {}),
    ("Wire",          ["techno", "techhouse"],     (0.03, 9000, 0.7, 610), 0.45, {}),
    ("House Tsss",    ["chicago", "deephouse"],    (0.05, 7000, 0.4, 390), 0.6, {}),
    ("Ride Bell",     ["deephouse", "techhouse"],  (0.12, 4000, 0.95, 520), 1.2, dict(bp=5200)),
    ("Tamb",          ["chicago", "deephouse", "breakbeat"], (0.06, 6000, 0.45, 700), 0.28, dict(shaker=True, bp=9000)),
    ("Break Hat",     ["breakbeat"],               (0.04, 6000, 0.25, 350), 0.35, {}),
    ("Brushy",        ["breakbeat", "deephouse"],  (0.08, 3500, 0.0, 300), 0.3, dict(shaker=True)),
    ("Clicky",        ["microhouse"],              (0.006, 12000, 0.6, 700), 0.06, {}),
    ("Grain Hat",     ["minimal", "microhouse"],   (0.02, 7000, 0.15, 300), 0.14, dict(bp=6500)),
    ("Crash Cut",     ["techno", "breakbeat"],     (0.15, 3000, 0.5, 330), 1.4, {}),
    ("Soft Open",     ["deephouse", "rominimal"],  (0.04, 5500, 0.3, 360), 0.45, {}),
    ("Hiss",          ["techno", "minimal"],       (0.06, 10000, 0.0, 400), 0.5, {}),
    ("Rattle",        ["rominimal", "microhouse"], (0.05, 4800, 0.1, 340), 0.18, dict(shaker=True, bp=5600)),
    ("Jack Hat",      ["chicago"],                 (0.03, 8000, 0.6, 450), 0.38, {}),
    ("Tech Shuffle",  ["techhouse"],               (0.035, 7500, 0.5, 430), 0.3, dict(bp=10000)),
]
for i, (name, tags, (tau, hp, metal, base), otau, extra) in enumerate(HATS):
    fname = f"H{i+1:02d} {name}"
    write("Hat", fname, hat(tau, hp, metal, base, max(0.12, tau * 7), seed=300 + i, **extra), tags)
    write("Hat", fname, hat(otau, hp, metal, base, max(0.3, otau * 6), seed=300 + i, **extra), tags, subdir="open")


# ----------------------------------------------------------------------------------------- TIE (tonal, root C)
C2, C3, C4 = 65.41, 130.81, 261.63

def tonal(freqs, amps, length, a=0.003, tau=0.4, kind="sine", cutoff=None, cut_env=0.0, q=0.9, drive=1.2,
          detune=0.0, vib=0.0, seed=1, noise=0.0):
    t = t_axis(length); r = rng(seed)
    x = np.zeros_like(t)
    for f, amp in zip(freqs, amps):
        for d in ([0.0] if detune == 0 else [-detune, detune]):
            fc = np.full_like(t, f * (1 + d)) * (1 + vib * np.sin(2 * np.pi * 5.2 * t))
            o = {"sine": osc_sine, "saw": osc_saw, "square": osc_square}[kind](fc)
            x += amp * o
    if noise:
        x += noise * r.standard_normal(len(t))
    if cutoff:
        fcurve = cutoff * (1 + cut_env * env_exp(t, 0.08))
        x = svf(x, fcurve, q, "lp")
    return sat(x * env_ad(t, a, tau), drive)

def fm_tone(f, ratio, index, length, tau, idx_tau=0.1, seed=1):
    t = t_axis(length)
    mod = np.sin(np.cumsum(2 * np.pi * f * ratio / SR)) * index * env_exp(t, idx_tau)
    car = np.sin(np.cumsum(2 * np.pi * f / SR) + mod)
    return car * env_ad(t, 0.002, tau)

def formant(f0, vowel, length, tau, seed=1):
    F = {"a": (800, 1150, 2900), "o": (450, 800, 2830), "e": (400, 1600, 2700), "u": (325, 700, 2530)}[vowel]
    src = osc_saw(np.full(int(length * SR), f0))
    x = sum(svf(src, fc, 8, "bp") * g for fc, g in zip(F, (1, 0.6, 0.3)))
    return x * env_ad(t_axis(length), 0.02, tau)

TIES = [
    ("Sub Bass C",       ["deephouse", "techhouse", "minimal"], lambda s: tonal([C2], [1], 1.0, tau=0.5, drive=1.3)),
    ("Acid Pluck C",     ["chicago", "techno"],     lambda s: tonal([C2], [1], 0.6, kind="saw", cutoff=500, cut_env=6, q=6, tau=0.25, drive=2.5)),
    ("Rubber Bass C",    ["minimal", "rominimal"],  lambda s: tonal([C2, C3], [1, 0.3], 0.7, kind="square", cutoff=700, cut_env=2, q=1.5, tau=0.25)),
    ("Ro Bass Pluck",    ["rominimal"],             lambda s: tonal([C2], [1], 0.5, kind="saw", cutoff=350, cut_env=3, q=2, tau=0.18, drive=1.4)),
    ("Dub Chord m7",     ["minimal", "deephouse"],  lambda s: tonal([C3, C3*1.189, C3*1.498, C3*1.782], [1]*4, 1.5, kind="saw", cutoff=900, cut_env=1, q=1.2, tau=0.6, detune=0.004)),
    ("Stab Minor",       ["chicago", "techhouse"],  lambda s: tonal([C3, C3*1.189, C3*1.498], [1]*3, 0.6, kind="saw", cutoff=2500, cut_env=1.5, tau=0.18, detune=0.006)),
    ("Organ Hit",        ["chicago", "deephouse"],  lambda s: tonal([C3, C4, C4*1.5, C4*2], [1, 0.7, 0.4, 0.3], 0.7, tau=0.3)),
    ("Rhodes C",         ["deephouse"],             lambda s: fm_tone(C3, 1.0, 1.8, 1.6, 0.9, 0.3)),
    ("Rhodes Cm9",       ["deephouse"],             lambda s: sum(fm_tone(f, 1.0, 1.4, 1.8, 1.0, 0.3) for f in (C3, C3*1.189, C3*1.498, C3*1.782, C4*1.122)) * 0.4),
    ("Marimba C",        ["rominimal", "microhouse"], lambda s: fm_tone(C4, 4.0, 1.2, 0.6, 0.18, 0.02)),
    ("Kalimba C",        ["rominimal", "microhouse"], lambda s: fm_tone(C4, 5.4, 0.9, 0.7, 0.25, 0.01)),
    ("Glass Bell",       ["minimal", "microhouse"], lambda s: fm_tone(C4 * 2, 3.5, 2.5, 1.4, 0.7, 0.4)),
    ("Ro Wood Tone",     ["rominimal"],             lambda s: fm_tone(C3 * 1.5, 2.0, 1.5, 0.4, 0.12, 0.03)),
    ("Vox Ah",           ["microhouse", "deephouse"], lambda s: formant(C3, "a", 0.9, 0.35)),
    ("Vox Oh",           ["microhouse", "rominimal"], lambda s: formant(C3, "o", 0.9, 0.3)),
    ("Vox Ee Blip",      ["microhouse"],            lambda s: formant(C4, "e", 0.3, 0.08)),
    ("Hoover Stab",      ["techno", "breakbeat"],   lambda s: tonal([C2, C3], [1, 0.8], 0.9, kind="saw", detune=0.02, cutoff=3000, tau=0.4, drive=2.0)),
    ("Tek Drone C",      ["techno"],                lambda s: tonal([C2, C2*1.5], [1, 0.5], 2.0, kind="saw", cutoff=400, q=3, a=0.05, tau=1.2, drive=3.0)),
    ("Reese C",          ["breakbeat", "techno"],   lambda s: tonal([C2], [1], 1.2, kind="saw", detune=0.012, cutoff=800, tau=0.6, drive=2.0)),
    ("Electro Zap Bass", ["breakbeat"],             lambda s: tonal([C2], [1], 0.5, kind="square", cutoff=300, cut_env=12, q=4, tau=0.2, drive=2.5)),
    ("Funk Bass C",      ["breakbeat", "chicago"],  lambda s: tonal([C2, C3], [1, 0.4], 0.5, kind="saw", cutoff=900, cut_env=2, q=1.8, tau=0.22, drive=1.8)),
    ("Tech Bass Stab",   ["techhouse"],             lambda s: tonal([C2], [1], 0.4, kind="saw", cutoff=600, cut_env=4, q=3, tau=0.14, drive=2.0)),
    ("Pad Swell Cm",     ["deephouse", "minimal"],  lambda s: tonal([C3, C3*1.189, C3*1.498], [1]*3, 2.5, kind="saw", cutoff=1200, a=0.5, tau=1.6, detune=0.008, drive=1.0)),
    ("Piano Chord",      ["chicago", "deephouse"],  lambda s: sum(fm_tone(f, 2.0, 1.1, 1.5, 0.7, 0.05) for f in (C3, C3*1.26, C3*1.498, C4)) * 0.5),
    ("Mallet Low",       ["minimal", "rominimal"],  lambda s: fm_tone(C3, 3.0, 1.6, 0.6, 0.2, 0.02)),
    ("Bleep C",          ["techno", "minimal"],     lambda s: tonal([C4 * 2], [1], 0.25, kind="square", tau=0.06)),
    ("Sine Ping",        ["microhouse", "minimal"], lambda s: tonal([C4], [1], 0.4, tau=0.12)),
    ("Guitar Pluck",     ["deephouse", "rominimal"], lambda s: tonal([C3, C4, C4*1.5], [1, 0.4, 0.2], 1.0, kind="saw", cutoff=1800, cut_env=1, tau=0.35, drive=1.1)),
    ("Ro Dub Tone",      ["rominimal"],             lambda s: tonal([C2, C3*1.498], [1, 0.25], 0.9, kind="sine", tau=0.45, drive=1.6, vib=0.003)),
    ("Siren Tone",       ["breakbeat", "techno"],   lambda s: tonal([C4], [1], 0.8, kind="square", tau=0.5, vib=0.04)),
    ("Hollow Stab",      ["techhouse", "minimal"],  lambda s: tonal([C3, C4*1.498], [1, 0.6], 0.5, kind="square", cutoff=1500, cut_env=1, tau=0.15)),
    ("Clav C",           ["chicago", "breakbeat"],  lambda s: tonal([C3], [1], 0.4, kind="square", cutoff=2500, cut_env=1.5, q=2, tau=0.12, drive=1.5)),
]
for i, (name, tags, fn) in enumerate(TIES):
    write("Tie", f"T{i+1:02d} {name}", fn(400 + i), tags)


# ----------------------------------------------------------------------------------------- POD (percussion / fx)
def perc(f, length, tau, drop=0.3, partials=((1, 1),), noise=0.0, seed=1, n_fc=3000):
    t = t_axis(length); r = rng(seed)
    fc = f * (1 + drop * env_exp(t, 0.02))
    x = sum(a * osc_sine(fc * k) for k, a in partials)
    if noise:
        x += noise * svf(r.standard_normal(len(t)), n_fc, 1.0, "bp") * env_exp(t, 0.01)
    return x * env_ad(t, 0.0005, tau)

def noise_fx(length, kind, seed=1):
    t = t_axis(length); r = rng(seed); n = r.standard_normal(len(t))
    if kind == "sweep":
        return svf(n, 200 + 8000 * (t / length) ** 2, 4, "bp") * np.sin(np.pi * t / length)
    if kind == "burst":
        return svf(n, 2500, 0.8, "bp") * env_exp(t, 0.03)
    if kind == "crackle":
        c = (r.random(len(t)) > 0.9985) * r.standard_normal(len(t)) * 3
        return svf(c + 0.05 * n, 3000, 0.5, "hp") * env_ad(t, 0.01, length * 0.5)
    if kind == "zip":
        return svf(n, 1000 + 6000 * env_exp(t, 0.05), 6, "bp") * env_exp(t, 0.06)
    if kind == "breath":
        return svf(n, 1200, 0.7, "bp") * env_ad(t, 0.1, 0.25)
    return n * env_exp(t, 0.05)

PODS = [
    ("Conga Hi",      ["rominimal", "deephouse", "chicago"], lambda s: perc(340, 0.4, 0.12, 0.35, ((1, 1), (2.1, 0.15)), 0.2, s)),
    ("Conga Lo",      ["rominimal", "deephouse"],            lambda s: perc(220, 0.5, 0.16, 0.35, ((1, 1), (2.1, 0.15)), 0.2, s)),
    ("Bongo",         ["rominimal", "microhouse"],           lambda s: perc(420, 0.3, 0.08, 0.4, ((1, 1), (2.4, 0.2)), 0.3, s)),
    ("Tom Low",       ["chicago", "techno"],                 lambda s: perc(95, 0.6, 0.25, 0.6, ((1, 1),), 0.2, s)),
    ("Tom Hi",        ["chicago", "breakbeat"],              lambda s: perc(180, 0.45, 0.18, 0.6, ((1, 1),), 0.2, s)),
    ("Cowbell",       ["chicago", "techhouse"],              lambda s: perc(560, 0.4, 0.12, 0.0, ((1, 0.7), (1.48, 0.6)), 0.0, s)),
    ("Clave",         ["rominimal", "microhouse", "minimal"], lambda s: perc(2500, 0.15, 0.03, 0.0, ((1, 1),), 0.1, s)),
    ("Woodblock",     ["rominimal", "microhouse"],           lambda s: perc(1150, 0.2, 0.04, 0.05, ((1, 1), (2.6, 0.3)), 0.15, s)),
    ("Log Drum",      ["rominimal", "minimal"],              lambda s: perc(160, 0.5, 0.2, 0.15, ((1, 1), (2.76, 0.4)), 0.0, s)),
    ("Agogo",         ["rominimal", "deephouse"],            lambda s: perc(950, 0.5, 0.18, 0.0, ((1, 0.8), (2.67, 0.4)), 0.0, s)),
    ("Shaker Long",   ["deephouse", "rominimal", "microhouse"], lambda s: noise_fx(0.4, "breath", s)),
    ("Tabla Na",      ["rominimal", "microhouse"],           lambda s: perc(520, 0.4, 0.15, 0.1, ((1, 1), (2.0, 0.5), (3.01, 0.2)), 0.2, s)),
    ("Ro Click Perc", ["rominimal"],                         lambda s: perc(1800, 0.08, 0.012, 0.5, ((1, 1), (1.7, 0.5)), 0.4, s, 6000)),
    ("Ro Water Drop", ["rominimal", "microhouse"],           lambda s: perc(900, 0.2, 0.05, -0.5, ((1, 1),), 0.0, s)),
    ("Ro Metal Tic",  ["rominimal", "minimal"],              lambda s: perc(3300, 0.12, 0.02, 0.0, ((1, 1), (1.41, 0.7), (2.3, 0.5)), 0.1, s)),
    ("Glitch Blip",   ["microhouse", "minimal"],             lambda s: tonal([2200], [1], 0.05, kind="square", tau=0.012)),
    ("Bit Click",     ["microhouse"],                        lambda s: np.round(perc(1200, 0.06, 0.01, 0.0, ((1, 1),), 0.5, s) * 4) / 4),
    ("Vinyl Crackle", ["microhouse", "deephouse"],           lambda s: noise_fx(1.2, "crackle", s)),
    ("Zip",           ["microhouse", "minimal"],             lambda s: noise_fx(0.25, "zip", s)),
    ("Noise Burst",   ["techno", "breakbeat"],               lambda s: noise_fx(0.2, "burst", s)),
    ("Sweep Up",      ["techno", "techhouse"],               lambda s: noise_fx(1.0, "sweep", s)),
    ("Metal Hit",     ["techno", "industrial", "minimal"],   lambda s: perc(410, 0.6, 0.3, 0.0, ((1, 1), (2.76, 0.6), (5.4, 0.4), (8.9, 0.3)), 0.2, s)),
    ("Laser",         ["breakbeat", "techno"],               lambda s: osc_square(200 + 2000 * env_exp(t_axis(0.4), 0.05)) * env_exp(t_axis(0.4), 0.12) * 0.6),
    ("Zap Down",      ["breakbeat", "chicago"],              lambda s: osc_sine(60 + 3000 * env_exp(t_axis(0.4), 0.03)) * env_exp(t_axis(0.4), 0.1)),
    ("Tech Perc",     ["techhouse"],                         lambda s: perc(720, 0.25, 0.06, 0.3, ((1, 1), (1.6, 0.4)), 0.3, s)),
    ("Rim Perc",      ["techhouse", "minimal"],              lambda s: rim(1250, 0.025, noise=0.3, seed=s)),
    ("Vox Hey",       ["chicago", "techhouse"],              lambda s: formant(220, "e", 0.3, 0.12)),
    ("Vox Uh",        ["deephouse", "microhouse"],           lambda s: formant(160, "u", 0.35, 0.15)),
    ("Snap Perc",     ["deephouse", "breakbeat"],            lambda s: clap(2200, 0.005, 2, 0.04, length=0.25, seed=s)),
    ("Steel Pan",     ["deephouse", "rominimal"],            lambda s: fm_tone(523, 2.0, 1.0, 0.8, 0.35, 0.1)),
    ("Cabasa",        ["chicago", "deephouse", "breakbeat"], lambda s: noise_fx(0.15, "burst", s) * 0.5 + svf(rng(s).standard_normal(int(0.15*SR)), 9000, 1, "hp") * env_exp(t_axis(0.15), 0.03)),
    ("Timbale",       ["breakbeat", "chicago"],              lambda s: perc(380, 0.5, 0.2, 0.2, ((1, 1), (1.58, 0.5), (2.1, 0.3)), 0.4, s)),
]
for i, (name, tags, fn) in enumerate(PODS):
    write("Pod", f"P{i+1:02d} {name}", fn(500 + i), tags)

with open(os.path.join(OUT, "manifest.json"), "w") as f:
    json.dump({"genres": GENRES, "modules": manifest}, f, indent=1)
print({k: len(v) for k, v in manifest.items()})

# pack everything (folders + manifest) into the archive that is embedded in the plug-in
import zipfile
zpath = os.path.join(os.path.dirname(os.path.abspath(OUT)), "factory_samples.zip")
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk(OUT):
        for fn in sorted(files):
            full = os.path.join(root, fn)
            z.write(full, os.path.relpath(full, OUT))
import shutil
shutil.copy(os.path.join(OUT, "manifest.json"), os.path.join(os.path.dirname(os.path.abspath(OUT)), "samples_manifest.json"))
print("archive:", zpath, os.path.getsize(zpath) // 1024, "KB")
