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



# ---------------------------------------------------------------- base instruments
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


# ---------------------------------------------------------------- extra instruments (v2.5 library)
def reverb_ir(seconds, seed=1, damp=0.0):
    t = t_axis(seconds); n = rng(seed).standard_normal(len(t)) * np.exp(-t * 6.9 / seconds)
    if damp: n = svf(n, 4000 * (1 - damp) + 400, 0.7, "lp")
    return n / np.sqrt(np.sum(n * n) + 1e-9)

def convolve(x, ir):
    n = len(x) + len(ir) - 1
    N = 1 << (n - 1).bit_length()
    return np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(ir, N), N)[:n]

def rumble_kick(f_end, f_start, p_tau, a_tau, length, rumble=0.5, rumble_lp=160, rumble_drive=3.0, click=0.3, drive=2.0, seed=1):
    """Techno 'rumble' kick: kick + saturated, low-passed reverb tail (CVO / Alarico style)."""
    k = kick(f_end, f_start, p_tau, a_tau, length, click=click, drive=drive, seed=seed)
    tail = convolve(k, reverb_ir(1.2, seed + 7))[:len(k)]
    tail = svf(sat(tail * 4.0, rumble_drive), rumble_lp, 0.9, "lp")
    t = t_axis(length)
    tail *= np.clip((t - 0.02) / 0.06, 0, 1) * env_exp(t, length * 0.45)
    return k + rumble * tail / (np.max(np.abs(tail)) + 1e-9) * np.max(np.abs(k))

def lofi(x, bits=10, hold=2, lp=7000, hiss=0.0, seed=1):
    y = np.repeat(x[::hold], hold)[:len(x)]
    q = 2 ** (bits - 1)
    y = np.round(y * q) / q
    y = svf(y, lp, 0.7, "lp")
    if hiss: y = y + hiss * rng(seed).standard_normal(len(y)) * 0.02
    return y

def vinyl(x, crackle=0.3, seed=1):
    r = rng(seed); n = len(x)
    c = (r.random(n) > 0.9992) * r.standard_normal(n) * 0.8
    c = svf(c, 2500, 0.6, "hp")
    return x + crackle * c + 0.004 * crackle * svf(r.standard_normal(n), 3000, 0.5, "bp")

def karplus(f, length, decay=0.996, bright=0.5, seed=1, pick=0.5):
    """Plucked string (Karplus-Strong) - double bass / guitar like."""
    r = rng(seed); n = int(length * SR); p = max(2, int(SR / f))
    buf = r.standard_normal(p) * 0.8
    buf = svf(buf, 800 + 6000 * bright, 0.7, "lp")
    out = np.zeros(n); idx = 0
    for i in range(n):
        out[i] = buf[idx]
        nxt = (idx + 1) % p
        buf[idx] = decay * (pick * buf[idx] + (1 - pick) * buf[nxt])
        idx = nxt
    return out * env_ad(t_axis(length), 0.002, length * 0.6)

def acid(f, length, cutoff=600, env=4.0, res=0.9, decay=0.18, wave="saw", accent=False, slide=0.0, drive=2.0):
    """303-style note: saw/square through a resonant low-pass with envelope."""
    t = t_axis(length)
    fc = np.full_like(t, f) * (1 + slide * env_exp(t, 0.03))
    o = osc_saw(fc) if wave == "saw" else osc_square(fc)
    cf = cutoff * (1 + env * env_exp(t, decay * (0.6 if accent else 1.0)))
    y = svf(o, cf, 1.0 / max(0.05, 1.0 - res * 0.97), "lp")
    return sat(y * env_ad(t, 0.002, decay * 2.2) * (1.3 if accent else 1.0), drive)

def strings(freqs, length, attack=0.25, cutoff=2500, detune=0.008, seed=1):
    """Detroit string / pad: detuned saws, slow attack."""
    t = t_axis(length); x = np.zeros_like(t)
    for f in freqs:
        for d in (-detune, 0, detune):
            x += osc_saw(np.full_like(t, f * (1 + d)) * (1 + 0.002 * np.sin(2 * np.pi * (4.7 + d * 100) * t)))
    x = svf(x / (3 * len(freqs)), cutoff, 0.8, "lp")
    return x * env_ad(t, attack, length * 0.5)

def organ(freqs, length, drawbars=(1, 0.8, 0.5, 0.35, 0.25, 0.2), click=0.3, tau=0.6, seed=1):
    """House organ (M1 / drawbar style) with key click."""
    t = t_axis(length); x = np.zeros_like(t)
    harm = (1, 2, 3, 4, 6, 8)
    for f in freqs:
        for h, a in zip(harm, drawbars):
            x += a * osc_sine(np.full_like(t, f * h))
    x /= len(freqs)
    x += click * svf(rng(seed).standard_normal(len(t)), 3000, 1.0, "bp") * env_exp(t, 0.004)
    return x * env_ad(t, 0.003, tau)

def cowbell808(length=0.4, tau=0.09):
    t = t_axis(length)
    x = osc_square(np.full_like(t, 540)) + osc_square(np.full_like(t, 800))
    x = svf(x, 2600, 1.6, "bp")
    return x * (0.6 * env_exp(t, 0.012) + 0.4 * env_exp(t, tau))

def snare808(tone=180, snappy=0.6, tau=0.12, length=0.45, seed=1):
    t = t_axis(length)
    body = (osc_sine(np.full_like(t, tone)) + 0.6 * osc_sine(np.full_like(t, tone * 1.85))) * env_exp(t, 0.045)
    nz = svf(rng(seed).standard_normal(len(t)), 5500, 0.7, "hp") * env_exp(t, tau)
    return sat(body * (1 - snappy) + nz * snappy * 1.4, 1.4)

def chord(root, kind, octave_up=0):
    iv = {"m7": (0, 3, 7, 10), "m9": (0, 3, 7, 10, 14), "min": (0, 3, 7), "maj7": (0, 4, 7, 11), "maj9": (0, 4, 7, 11, 14),
          "sus": (0, 5, 7, 10), "dom": (0, 4, 7, 10), "m11": (0, 3, 7, 10, 17)}[kind]
    return [root * 2 ** ((i + 12 * octave_up) / 12) for i in iv]

def delay_fx(x, beats=0.375, bpm=124, fb=0.45, mix=0.35, lp=2500):
    d = int(beats * 60 / bpm * SR); y = np.concatenate([x, np.zeros(d * 4)])
    out = y.copy(); buf = y.copy()
    for k in range(1, 5):
        buf = np.concatenate([np.zeros(d), buf[:-d]]) * fb
        buf = svf(buf, lp, 0.7, "lp")
        out += buf * mix
    return out

C1, C2, C3, C4 = 32.70, 65.41, 130.81, 261.63

# ---------------------------------------------------------------- the library: 6 per genre and module (hats: 4 pairs)
G = {}   # module -> list of (name, genre, fn)
def add(module, name, genre, fn): G.setdefault(module, []).append((name, genre, fn))

# ===== KICKS
add("Kick", "Tek Rumble Deep", "techno",   lambda s: rumble_kick(46, 190, 0.035, 0.38, 1.6, rumble=0.7, rumble_lp=140, click=0.3, drive=2.6, seed=s))
add("Kick", "Tek Rumble Dist", "techno",   lambda s: rumble_kick(48, 240, 0.028, 0.30, 1.4, rumble=0.8, rumble_lp=190, rumble_drive=6, click=0.45, drive=4.0, seed=s))
add("Kick", "Tek Hypno", "techno",         lambda s: kick(50, 210, 0.03, 0.40, 1.0, click=0.35, drive=2.4, sub=0.2, seed=s))
add("Kick", "Tek Rolling", "techno",       lambda s: kick(53, 260, 0.022, 0.22, 0.7, click=0.55, drive=3.0, seed=s))
add("Kick", "Tek Warehouse", "techno",     lambda s: rumble_kick(50, 280, 0.025, 0.26, 1.2, rumble=0.45, click=0.6, drive=3.5, seed=s))
add("Kick", "Tek Low Thud", "techno",      lambda s: rumble_kick(43, 150, 0.05, 0.5, 1.6, rumble=0.5, rumble_lp=120, click=0.15, drive=2.0, seed=s))
add("Kick", "Min Dry", "minimal",          lambda s: kick(58, 200, 0.018, 0.12, 0.4, click=0.4, drive=1.4, seed=s))
add("Kick", "Min Click", "minimal",        lambda s: kick(66, 420, 0.006, 0.07, 0.25, click=0.9, drive=1.6, body_hp=40, seed=s))
add("Kick", "Min Lofi", "minimal",         lambda s: lofi(kick(56, 180, 0.02, 0.15, 0.45, click=0.3, drive=1.6, seed=s), bits=8, hold=3, lp=5000, hiss=0.5, seed=s))
add("Kick", "Min Soft Thump", "minimal",   lambda s: kick(54, 120, 0.035, 0.16, 0.5, click=0.05, drive=1.1, seed=s))
add("Kick", "Min Jazz Kick", "minimal",    lambda s: convolve(kick(62, 140, 0.02, 0.1, 0.35, click=0.2, noise=0.4, drive=1.2, seed=s), reverb_ir(0.25, s, 0.6))[:int(0.6 * SR)])
add("Kick", "Min Micro", "minimal",        lambda s: kick(72, 300, 0.008, 0.05, 0.2, click=0.6, drive=1.3, body_hp=50, seed=s))
add("Kick", "Mic Round Sub", "microhouse", lambda s: kick(48, 110, 0.05, 0.45, 1.2, click=0.08, drive=1.2, sub=0.3, seed=s))
add("Kick", "Mic Rubber", "microhouse",    lambda s: kick(56, 160, 0.03, 0.2, 0.6, click=0.15, drive=1.5, bend=25, seed=s))
add("Kick", "Mic Soft", "microhouse",      lambda s: kick(52, 130, 0.03, 0.22, 0.6, click=0.05, drive=1.1, seed=s))
add("Kick", "Mic Deep Long", "microhouse", lambda s: kick(45, 100, 0.06, 0.7, 1.8, click=0.05, drive=1.15, sub=0.3, seed=s))
add("Kick", "Mic Wood", "microhouse",      lambda s: kick(64, 190, 0.02, 0.09, 0.3, click=0.4, noise=0.2, drive=1.3, seed=s))
add("Kick", "Mic Dusty", "microhouse",     lambda s: vinyl(lofi(kick(54, 150, 0.03, 0.25, 0.7, click=0.1, drive=1.5, seed=s), 11, 2, 6000), 0.4, s))
add("Kick", "Ro Punch", "rominimal",       lambda s: kick(55, 170, 0.025, 0.17, 0.5, click=0.2, drive=1.4, seed=s))
add("Kick", "Ro Round", "rominimal",       lambda s: kick(50, 120, 0.04, 0.26, 0.8, click=0.06, drive=1.15, sub=0.15, seed=s))
add("Kick", "Ro Tight", "rominimal",       lambda s: kick(60, 200, 0.015, 0.1, 0.35, click=0.3, drive=1.4, seed=s))
add("Kick", "Ro Sub", "rominimal",         lambda s: kick(46, 95, 0.06, 0.55, 1.5, click=0.03, drive=1.1, sub=0.35, seed=s))
add("Kick", "Ro Clean 909", "rominimal",   lambda s: kick(52, 220, 0.022, 0.2, 0.6, click=0.35, noise=0.1, drive=1.6, seed=s))
add("Kick", "Ro Felt", "rominimal",        lambda s: kick(57, 130, 0.03, 0.14, 0.45, click=0.02, drive=1.05, seed=s))
add("Kick", "House 909 Warm", "house",     lambda s: kick(52, 260, 0.024, 0.32, 0.9, click=0.5, noise=0.2, drive=2.0, seed=s))
add("Kick", "House 909 Drive", "house",    lambda s: kick(54, 300, 0.02, 0.28, 0.8, click=0.7, noise=0.25, drive=3.0, seed=s))
add("Kick", "House Garage", "house",       lambda s: kick(50, 180, 0.035, 0.35, 1.0, click=0.25, drive=1.8, sub=0.2, seed=s))
add("Kick", "House Deep", "house",         lambda s: kick(47, 140, 0.04, 0.45, 1.2, click=0.15, drive=1.5, sub=0.25, seed=s))
add("Kick", "House Box", "house",          lambda s: kick(60, 230, 0.018, 0.16, 0.5, click=0.45, drive=1.8, seed=s))
add("Kick", "House Sat", "house",          lambda s: kick(52, 250, 0.025, 0.3, 0.9, click=0.5, drive=4.0, seed=s))
add("Kick", "TH 808 Raw", "techhouse",     lambda s: kick(47, 110, 0.05, 0.42, 1.2, click=0.1, drive=2.2, seed=s))
add("Kick", "TH 707", "techhouse",         lambda s: kick(60, 240, 0.017, 0.14, 0.45, click=0.5, noise=0.2, drive=1.7, seed=s))
add("Kick", "TH 909 Raw", "techhouse",     lambda s: kick(53, 290, 0.022, 0.27, 0.8, click=0.65, noise=0.25, drive=3.2, seed=s))
add("Kick", "TH Jack", "techhouse",        lambda s: kick(56, 260, 0.02, 0.2, 0.6, click=0.6, drive=2.6, seed=s))
add("Kick", "TH UK Punch", "techhouse",    lambda s: kick(54, 230, 0.022, 0.22, 0.7, click=0.4, drive=2.0, sub=0.1, seed=s))
add("Kick", "TH Detroit", "techhouse",     lambda s: kick(50, 200, 0.03, 0.34, 0.9, click=0.35, drive=2.4, seed=s))
add("Kick", "Deep Dusty", "deephouse",     lambda s: vinyl(lofi(kick(52, 160, 0.03, 0.3, 0.9, click=0.15, drive=1.8, seed=s), 10, 2, 5000, 0.5, s), 0.5, s))
add("Kick", "Deep Lofi Thump", "deephouse",lambda s: lofi(kick(55, 140, 0.03, 0.2, 0.6, click=0.1, drive=2.2, seed=s), 9, 3, 4200, 0.6, s))
add("Kick", "Deep Soft 909", "deephouse",  lambda s: kick(52, 220, 0.026, 0.3, 0.9, click=0.3, noise=0.1, drive=1.6, seed=s))
add("Kick", "Deep Dub", "deephouse",       lambda s: kick(48, 120, 0.045, 0.4, 1.1, click=0.08, drive=1.4, sub=0.25, seed=s))
add("Kick", "Deep Tape", "deephouse",      lambda s: sat(svf(kick(54, 200, 0.025, 0.28, 0.8, click=0.25, drive=1.6, seed=s), 3500, 0.7, "lp") * 1.6, 2.2))
add("Kick", "Deep Round", "deephouse",     lambda s: kick(50, 130, 0.04, 0.32, 0.9, click=0.05, drive=1.2, seed=s))
add("Kick", "Electro 808 Boom", "breakbeat", lambda s: kick(42, 150, 0.06, 0.9, 2.0, click=0.15, drive=1.5, seed=s))
add("Kick", "Electro 808 Long", "breakbeat", lambda s: kick(45, 120, 0.05, 0.7, 1.8, click=0.1, drive=1.3, seed=s))
add("Kick", "Electro Punch", "breakbeat",  lambda s: kick(58, 300, 0.018, 0.18, 0.5, click=0.7, drive=2.6, seed=s))
add("Kick", "Electro Short 808", "breakbeat", lambda s: kick(50, 140, 0.04, 0.25, 0.7, click=0.15, drive=1.8, seed=s))
add("Kick", "Electro Click", "breakbeat",  lambda s: kick(65, 450, 0.01, 0.1, 0.35, click=1.0, drive=2.0, seed=s))
add("Kick", "Electro Dist 808", "breakbeat", lambda s: kick(44, 160, 0.05, 0.6, 1.5, click=0.2, drive=5.0, seed=s))

# ===== SNARES / CLAPS / RIMS
add("Snare", "Tek Noise Snare", "techno",  lambda s: convolve(snare(200, 0.03, 0.14, 3500, mix=0.85, drive=2.5, seed=s), reverb_ir(0.5, s, 0.3))[:int(0.8 * SR)])
add("Snare", "Tek Metal Clap", "techno",   lambda s: convolve(clap(1500, 0.011, 4, 0.12, seed=s, room=0.3), reverb_ir(0.6, s, 0.2))[:int(0.9 * SR)])
add("Snare", "Tek Industrial", "techno",   lambda s: sat(snare(240, 0.03, 0.09, 5000, "hp", mix=0.8, drive=4.0, snap=0.5, seed=s) * 1.5, 3))
add("Snare", "Tek Rim Echo", "techno",     lambda s: delay_fx(rim(780, 0.03, noise=0.3, seed=s), 0.375, 130, 0.5, 0.4, 2000))
add("Snare", "Tek Clap Dry", "techno",     lambda s: clap(1300, 0.01, 4, 0.08, length=0.4, seed=s))
add("Snare", "Tek Gated", "techno",        lambda s: snare(190, 0.02, 0.25, 2600, mix=0.95, drive=3.0, length=0.35, seed=s))
add("Snare", "Min Click Snare", "minimal", lambda s: snare(320, 0.006, 0.03, 7500, "hp", mix=0.85, length=0.15, snap=0.8, seed=s))
add("Snare", "Min Micro Rim", "minimal",   lambda s: rim(1900, 0.008, length=0.1, noise=0.5, seed=s))
add("Snare", "Min Brush", "minimal",       lambda s: vinyl(svf(rng(s).standard_normal(int(0.4 * SR)), 4000, 0.6, "bp") * env_ad(t_axis(0.4), 0.02, 0.09), 0.3, s))
add("Snare", "Min Snap Glitch", "minimal", lambda s: lofi(clap(2600, 0.004, 2, 0.03, length=0.2, seed=s), 6, 4, 8000))
add("Snare", "Min Lofi Snare", "minimal",  lambda s: lofi(snare(210, 0.02, 0.06, 3500, mix=0.7, length=0.3, seed=s), 8, 3, 5500, 0.4, s))
add("Snare", "Min Paper", "minimal",       lambda s: snare(240, 0.012, 0.045, 5200, mix=0.8, length=0.2, seed=s))
add("Snare", "Mic Finger Snap", "microhouse", lambda s: clap(2600, 0.004, 2, 0.035, length=0.25, q=1.2, seed=s))
add("Snare", "Mic Rim Wood", "microhouse", lambda s: rim(1350, 0.02, length=0.2, noise=0.2, seed=s))
add("Snare", "Mic Organic Clap", "microhouse", lambda s: convolve(clap(1100, 0.014, 3, 0.07, length=0.35, q=1.8, seed=s), reverb_ir(0.3, s, 0.5))[:int(0.5 * SR)])
add("Snare", "Mic Shaker Snare", "microhouse", lambda s: snare(260, 0.01, 0.07, 6000, "hp", mix=0.9, length=0.3, seed=s))
add("Snare", "Mic Dust Snare", "microhouse", lambda s: vinyl(snare(230, 0.015, 0.05, 5500, "hp", mix=0.85, length=0.25, seed=s), 0.4, s))
add("Snare", "Mic Knock", "microhouse",    lambda s: rim(480, 0.05, length=0.3, noise=0.1, seed=s))
add("Snare", "Ro Rimshot", "rominimal",    lambda s: rim(620, 0.04, noise=0.15, seed=s))
add("Snare", "Ro Tight Rim", "rominimal",  lambda s: rim(900, 0.02, length=0.15, noise=0.25, seed=s))
add("Snare", "Ro Short Clap", "rominimal", lambda s: clap(1400, 0.008, 3, 0.05, length=0.25, q=2.0, seed=s))
add("Snare", "Ro Wood Click", "rominimal", lambda s: rim(2400, 0.01, length=0.12, noise=0.3, seed=s))
add("Snare", "Ro Snap Room", "rominimal",  lambda s: convolve(clap(2200, 0.005, 2, 0.04, length=0.2, seed=s), reverb_ir(0.4, s, 0.4))[:int(0.6 * SR)])
add("Snare", "Ro Soft Snare", "rominimal", lambda s: snare(170, 0.02, 0.07, 2200, mix=0.6, drive=1.0, length=0.3, seed=s))
add("Snare", "House 909 Clap", "house",    lambda s: clap(1150, 0.012, 4, 0.15, seed=s, room=0.4))
add("Snare", "House Garage Snare", "house", lambda s: snare(185, 0.05, 0.12, 3800, mix=0.7, drive=1.8, snap=0.3, seed=s))
add("Snare", "House Clap Stack", "house",  lambda s: clap(1050, 0.013, 5, 0.22, length=0.8, seed=s, room=0.8))
add("Snare", "House Snap", "house",        lambda s: clap(2400, 0.005, 2, 0.05, length=0.25, seed=s))
add("Snare", "House Rim", "house",         lambda s: rim(820, 0.03, noise=0.2, seed=s))
add("Snare", "House 909 Snare", "house",   lambda s: snare(180, 0.07, 0.15, 3500, mix=0.65, drive=1.6, seed=s))
add("Snare", "TH 707 Clap", "techhouse",   lambda s: clap(1250, 0.009, 4, 0.09, length=0.4, q=1.4, seed=s))
add("Snare", "TH 808 Clap", "techhouse",   lambda s: clap(1050, 0.012, 4, 0.13, length=0.5, seed=s))
add("Snare", "TH 707 Snare", "techhouse",  lambda s: snare(220, 0.04, 0.09, 3000, mix=0.6, drive=2.0, length=0.35, seed=s))
add("Snare", "TH Rimshot", "techhouse",    lambda s: rim(1700, 0.012, length=0.15, noise=0.35, seed=s))
add("Snare", "TH Raw Snare", "techhouse",  lambda s: sat(snare(200, 0.05, 0.11, 4200, mix=0.75, drive=2.5, seed=s), 2.5))
add("Snare", "TH Tight Clap", "techhouse", lambda s: clap(1600, 0.008, 3, 0.06, length=0.3, seed=s))
add("Snare", "Deep Dusty Clap", "deephouse", lambda s: vinyl(lofi(clap(1000, 0.014, 4, 0.18, length=0.7, seed=s, room=0.6), 10, 2, 6000), 0.4, s))
add("Snare", "Deep Lofi Snare", "deephouse", lambda s: lofi(snare(175, 0.05, 0.14, 2600, mix=0.6, drive=1.5, seed=s), 9, 3, 4500, 0.5, s))
add("Snare", "Deep Snap", "deephouse",     lambda s: clap(2100, 0.005, 2, 0.06, length=0.3, seed=s))
add("Snare", "Deep Dub Rim", "deephouse",  lambda s: delay_fx(rim(700, 0.04, noise=0.1, seed=s), 0.75, 120, 0.55, 0.45, 1500))
add("Snare", "Deep Soft Clap", "deephouse", lambda s: clap(950, 0.015, 4, 0.25, length=0.9, q=1.3, seed=s, room=1.0))
add("Snare", "Deep Tape Snare", "deephouse", lambda s: sat(svf(snare(190, 0.06, 0.13, 3000, mix=0.65, seed=s), 5000, 0.7, "lp") * 1.4, 2.0))
add("Snare", "Electro 808 Snare", "breakbeat", lambda s: snare808(185, 0.65, 0.12, seed=s))
add("Snare", "Electro Snappy", "breakbeat", lambda s: snare808(230, 0.85, 0.09, 0.35, seed=s))
add("Snare", "Electro Clap", "breakbeat",  lambda s: clap(1200, 0.011, 4, 0.1, length=0.45, seed=s))
add("Snare", "Electro Gated", "breakbeat", lambda s: snare808(200, 0.75, 0.3, 0.45, seed=s) * (t_axis(0.45) < 0.18))
add("Snare", "Electro Zap Snare", "breakbeat", lambda s: snare808(200, 0.5, 0.1, seed=s) + 0.4 * osc_sine(300 + 2500 * env_exp(t_axis(0.45), 0.015)) * env_exp(t_axis(0.45), 0.05))
add("Snare", "Electro Rim", "breakbeat",   lambda s: rim(1600, 0.015, length=0.15, noise=0.3, seed=s))

# ===== HATS (closed + open pairs): (tau, hp, metal, base, open_tau, extra)
HATS = {
 "techno":     [("Tek Metal 909", (0.04, 8000, 0.5, 430), 0.5, {}), ("Tek Steel", (0.035, 7000, 0.95, 480), 0.55, {}),
                ("Tek Hiss Ride", (0.06, 9000, 0.2, 520), 1.4, dict(bp=6000)), ("Tek Dark", (0.05, 4500, 0.6, 300), 0.4, {})],
 "minimal":    [("Min Tick", (0.012, 9500, 0.5, 500), 0.12, {}), ("Min Crackle", (0.02, 7000, 0.0, 400), 0.15, dict(bp=8000)),
                ("Min Grain", (0.018, 7000, 0.15, 300), 0.14, dict(bp=6500)), ("Min Micro", (0.006, 12000, 0.6, 700), 0.06, {})],
 "microhouse": [("Mic Shaker", (0.07, 5000, 0.0, 400), 0.22, dict(shaker=True, bp=7000)), ("Mic Soft Hat", (0.03, 6000, 0.3, 360), 0.3, {}),
                ("Mic Rattle", (0.05, 4800, 0.1, 340), 0.18, dict(shaker=True, bp=5600)), ("Mic Dust", (0.01, 11000, 0.2, 520), 0.09, {})],
 "rominimal":  [("Ro Crisp", (0.02, 8500, 0.35, 450), 0.2, {}), ("Ro Shuffle", (0.03, 6500, 0.1, 410), 0.16, dict(bp=8500)),
                ("Ro Tsk", (0.025, 9000, 0.4, 470), 0.25, {}), ("Ro Soft Shaker", (0.05, 5500, 0.0, 380), 0.2, dict(shaker=True))],
 "house":      [("House 909 Hat", (0.045, 8000, 0.3, 420), 0.42, {}), ("House Open Tss", (0.05, 7000, 0.4, 390), 0.6, {}),
                ("House Ride", (0.12, 4000, 0.95, 520), 1.2, dict(bp=5200)), ("House Tamb", (0.06, 6000, 0.45, 700), 0.28, dict(shaker=True, bp=9000))],
 "techhouse":  [("TH 808 Hat", (0.035, 7000, 0.85, 360), 0.32, {}), ("TH 707 Hat", (0.03, 8500, 0.6, 450), 0.35, {}),
                ("TH Jack Hat", (0.03, 8000, 0.6, 450), 0.38, {}), ("TH Shuffle", (0.035, 7500, 0.5, 430), 0.3, dict(bp=10000))],
 "deephouse":  [("Deep Lofi Hat", (0.04, 5500, 0.3, 360), 0.45, {}), ("Deep Brushy", (0.08, 3500, 0.0, 300), 0.3, dict(shaker=True)),
                ("Deep Dusty Ride", (0.1, 4200, 0.8, 500), 1.0, dict(bp=5000)), ("Deep Soft Tsss", (0.05, 7000, 0.4, 390), 0.55, {})],
 "breakbeat":  [("Electro 808 Hat", (0.03, 7500, 0.9, 360), 0.3, {}), ("Electro Tick", (0.015, 10000, 0.8, 520), 0.15, {}),
                ("Electro Cymbal", (0.15, 3000, 0.5, 330), 1.4, {}), ("Electro Metal", (0.035, 6500, 1.0, 470), 0.45, {})],
}

# ===== TIE (tonal, rooted on C unless the name says otherwise)
add("Tie", "Tek Dub Stab m7", "techno",    lambda s: delay_fx(tonal(chord(C3, "m7"), [1] * 4, 0.5, kind="saw", cutoff=900, cut_env=1.5, q=1.5, tau=0.15, detune=0.006), 0.75, 130, 0.5, 0.5, 1800))
add("Tie", "Tek Rumble Bass C", "techno",  lambda s: sat(svf(tonal([C2], [1], 1.0, kind="saw", tau=0.5), 220, 1.5, "lp") * 2, 3.0))
add("Tie", "Tek Hypno Pluck", "techno",    lambda s: acid(C3, 0.4, cutoff=500, env=6, res=0.7, decay=0.08, wave="square", drive=1.6))
add("Tie", "Tek Dark Pad Cm", "techno",    lambda s: strings(chord(C3, "min"), 2.0, attack=0.4, cutoff=1200))
add("Tie", "Tek Sci-Fi Stab", "techno",    lambda s: fm_tone(C3 * 2, 1.414, 3.0, 0.8, 0.3, 0.08))
add("Tie", "Tek Drone C", "techno",        lambda s: tonal([C2, C2 * 1.5], [1, 0.5], 2.0, kind="saw", cutoff=400, q=3, a=0.05, tau=1.2, drive=3.0))
add("Tie", "Min Upright Bass C", "minimal", lambda s: karplus(C2, 1.0, 0.994, 0.3, s, 0.55))
add("Tie", "Min Jazz Rhodes Cm9", "minimal", lambda s: lofi(sum(fm_tone(f, 1.0, 1.4, 1.4, 0.8, 0.3) for f in chord(C3, "m9")) * 0.4, 10, 2, 6000, 0.4, s))
add("Tie", "Min Grain Organ C", "minimal", lambda s: lofi(organ([C3], 0.5, click=0.4, tau=0.25), 8, 3, 5000) * (0.6 + 0.4 * (np.sin(2 * np.pi * 18 * t_axis(0.5)) > 0)))
add("Tie", "Min Muted Guitar C", "minimal", lambda s: karplus(C3, 0.4, 0.985, 0.6, s, 0.5))
add("Tie", "Min Vibes C", "minimal",       lambda s: fm_tone(C4, 4.0, 0.8, 1.2, 0.6, 0.05) * (1 + 0.3 * np.sin(2 * np.pi * 5 * t_axis(1.2))))
add("Tie", "Min Click Bass C", "minimal",  lambda s: tonal([C2], [1], 0.35, kind="square", cutoff=500, cut_env=3, q=1.5, tau=0.12))
add("Tie", "Mic Sub Bass C", "microhouse", lambda s: tonal([C2], [1], 0.9, tau=0.45, drive=1.4))
add("Tie", "Mic Vox Ah C", "microhouse",   lambda s: formant(C3, "a", 0.9, 0.35))
add("Tie", "Mic Vox Oh C", "microhouse",   lambda s: formant(C3, "o", 0.7, 0.25))
add("Tie", "Mic Marimba C", "microhouse",  lambda s: fm_tone(C4, 4.0, 1.2, 0.6, 0.18, 0.02))
add("Tie", "Mic Celestial Pad", "microhouse", lambda s: strings(chord(C3, "maj9"), 2.5, attack=0.6, cutoff=2500, detune=0.01))
add("Tie", "Mic Rubber Bass C", "microhouse", lambda s: tonal([C2, C3], [1, 0.3], 0.6, kind="square", cutoff=650, cut_env=2, q=1.5, tau=0.22))
add("Tie", "Ro Rolling Bass C", "rominimal", lambda s: sat(tonal([C2], [1], 0.5, tau=0.2, drive=1.5) + 0.2 * tonal([C3], [1], 0.5, kind="saw", cutoff=600, tau=0.1), 1.5))
add("Tie", "Ro Pluck Bass C", "rominimal", lambda s: tonal([C2], [1], 0.5, kind="saw", cutoff=350, cut_env=3, q=2, tau=0.18, drive=1.4))
add("Tie", "Ro Soft Pad Cm", "rominimal",  lambda s: strings(chord(C3, "m9"), 2.0, attack=0.3, cutoff=1600, detune=0.006))
add("Tie", "Ro Kalimba C", "rominimal",    lambda s: fm_tone(C4, 5.4, 0.9, 0.7, 0.25, 0.01))
add("Tie", "Ro Bell C", "rominimal",       lambda s: fm_tone(C4 * 2, 3.5, 2.0, 1.2, 0.5, 0.3))
add("Tie", "Ro Dub Tone C", "rominimal",   lambda s: tonal([C2, C3 * 1.498], [1, 0.25], 0.9, kind="sine", tau=0.45, drive=1.6, vib=0.003))
add("Tie", "House Organ Bass C", "house",  lambda s: organ([C2], 0.5, (1, 0.9, 0.2, 0.3, 0.0, 0.0), click=0.5, tau=0.25))
add("Tie", "House Organ Chord Cm7", "house", lambda s: organ(chord(C3, "m7"), 0.7, click=0.3, tau=0.35))
add("Tie", "House Rhodes Cm9", "house",    lambda s: sum(fm_tone(f, 1.0, 1.6, 1.6, 1.0, 0.3) for f in chord(C3, "m9")) * 0.4)
add("Tie", "House Piano Chord Cm", "house", lambda s: sum(fm_tone(f, 2.0, 1.1, 1.5, 0.7, 0.05) for f in chord(C3, "m7")) * 0.5)
add("Tie", "House Deep Bass C", "house",   lambda s: tonal([C2, C3], [1, 0.15], 0.6, tau=0.3, drive=1.3))
add("Tie", "House Garage Stab", "house",   lambda s: tonal(chord(C3, "m9"), [1] * 5, 0.4, kind="saw", cutoff=2200, cut_env=1, tau=0.12, detune=0.005))
add("Tie", "TH Acid C", "techhouse",       lambda s: acid(C2, 0.35, cutoff=400, env=5, res=0.92, decay=0.12, drive=2.5))
add("Tie", "TH Acid Accent C", "techhouse", lambda s: acid(C2, 0.35, cutoff=600, env=7, res=0.95, decay=0.1, accent=True, drive=3.0))
add("Tie", "TH Acid Square C", "techhouse", lambda s: acid(C2, 0.4, cutoff=500, env=5, res=0.88, decay=0.15, wave="square", drive=2.2))
add("Tie", "TH Detroit Strings Cm", "techhouse", lambda s: strings(chord(C3, "m7"), 2.2, attack=0.35, cutoff=3000, detune=0.01))
add("Tie", "TH Chord Stab Cm", "techhouse", lambda s: tonal(chord(C3, "min"), [1] * 3, 0.5, kind="saw", cutoff=2500, cut_env=1.5, tau=0.15, detune=0.006))
add("Tie", "TH Raw Bass C", "techhouse",   lambda s: tonal([C2], [1], 0.4, kind="saw", cutoff=600, cut_env=4, q=3, tau=0.14, drive=2.0))
add("Tie", "Deep Dub Chord Cm7", "deephouse", lambda s: delay_fx(tonal(chord(C3, "m7"), [1] * 4, 0.6, kind="saw", cutoff=800, cut_env=1, q=1.2, tau=0.2, detune=0.004), 0.75, 120, 0.5, 0.5, 1500))
add("Tie", "Deep Warm Pad Cm9", "deephouse", lambda s: strings(chord(C3, "m9"), 2.5, attack=0.5, cutoff=1400, detune=0.008))
add("Tie", "Deep Funky Bass C", "deephouse", lambda s: tonal([C2, C3], [1, 0.4], 0.35, kind="saw", cutoff=900, cut_env=2, q=1.8, tau=0.12, drive=1.8))
add("Tie", "Deep Lofi Rhodes Cm7", "deephouse", lambda s: vinyl(lofi(sum(fm_tone(f, 1.0, 1.4, 1.6, 0.9, 0.3) for f in chord(C3, "m7")) * 0.4, 10, 2, 5000), 0.3, s))
add("Tie", "Deep Koze Toy C", "deephouse", lambda s: fm_tone(C4, 2.01, 2.5, 0.6, 0.25, 0.1) * (1 + 0.5 * np.sin(2 * np.pi * 7 * t_axis(0.6))))
add("Tie", "Deep Sub C", "deephouse",      lambda s: tonal([C2], [1], 0.9, tau=0.5, drive=1.2))
add("Tie", "Electro Square Bass C", "breakbeat", lambda s: tonal([C2], [1], 0.4, kind="square", cutoff=700, cut_env=3, q=2, tau=0.15, drive=1.8))
add("Tie", "Electro Robot Bass C", "breakbeat", lambda s: tonal([C2], [1], 0.5, kind="square", cutoff=300, cut_env=12, q=4, tau=0.2, drive=2.5))
add("Tie", "Electro Bleep Lead C", "breakbeat", lambda s: tonal([C4 * 2], [1], 0.25, kind="square", tau=0.06))
add("Tie", "Electro Minor Stab", "breakbeat", lambda s: tonal(chord(C3, "min"), [1] * 3, 0.5, kind="saw", cutoff=3000, cut_env=1, tau=0.15, detune=0.01))
add("Tie", "Electro Saw Bass C", "breakbeat", lambda s: tonal([C2], [1], 0.5, kind="saw", detune=0.01, cutoff=900, tau=0.25, drive=2.0))
add("Tie", "Electro Vocoder Oh", "breakbeat", lambda s: formant(C3, "o", 0.5, 0.2) * (np.sin(2 * np.pi * 30 * t_axis(0.5)) > -0.3))

# ===== POD (percussion / fx)
add("Pod", "Tek Metal Perc", "techno",     lambda s: perc(410, 0.6, 0.3, 0.0, ((1, 1), (2.76, 0.6), (5.4, 0.4), (8.9, 0.3)), 0.2, s))
add("Pod", "Tek Clank", "techno",          lambda s: convolve(perc(780, 0.3, 0.08, 0.0, ((1, 1), (1.73, 0.8), (2.9, 0.5)), 0.4, s), reverb_ir(0.6, s, 0.2))[:int(0.9 * SR)])
add("Pod", "Tek Noise Sweep", "techno",    lambda s: noise_fx(1.0, "sweep", s))
add("Pod", "Tek Tom", "techno",            lambda s: perc(110, 0.5, 0.22, 0.6, ((1, 1),), 0.2, s))
add("Pod", "Tek Hypno Blip", "techno",     lambda s: delay_fx(tonal([C4 * 1.5], [1], 0.15, kind="square", tau=0.03), 0.375, 130, 0.55, 0.5, 2500))
add("Pod", "Tek Shot", "techno",           lambda s: convolve(noise_fx(0.2, "burst", s), reverb_ir(0.9, s, 0.3))[:int(1.0 * SR)])
add("Pod", "Min Vinyl Loop", "minimal",    lambda s: noise_fx(1.2, "crackle", s))
add("Pod", "Min Glitch Click", "minimal",  lambda s: np.round(perc(1200, 0.06, 0.01, 0.0, ((1, 1),), 0.5, s) * 4) / 4)
add("Pod", "Min Micro Blip", "minimal",    lambda s: tonal([2200], [1], 0.05, kind="square", tau=0.012))
add("Pod", "Min Granular Burst", "minimal", lambda s: lofi(noise_fx(0.25, "zip", s), 6, 5, 7000))
add("Pod", "Min Jazz Cymbal Chop", "minimal", lambda s: lofi(hat(0.2, 4000, 0.9, 520, 0.4, seed=s), 9, 2, 6000, 0.4, s))
add("Pod", "Min Pop", "minimal",           lambda s: perc(900, 0.12, 0.03, -0.4, ((1, 1),), 0.0, s))
add("Pod", "Mic Conga Hi", "microhouse",   lambda s: perc(340, 0.4, 0.12, 0.35, ((1, 1), (2.1, 0.15)), 0.2, s))
add("Pod", "Mic Conga Lo", "microhouse",   lambda s: perc(220, 0.5, 0.16, 0.35, ((1, 1), (2.1, 0.15)), 0.2, s))
add("Pod", "Mic Bongo", "microhouse",      lambda s: perc(420, 0.3, 0.08, 0.4, ((1, 1), (2.4, 0.2)), 0.3, s))
add("Pod", "Mic Woodblock", "microhouse",  lambda s: perc(1150, 0.2, 0.04, 0.05, ((1, 1), (2.6, 0.3)), 0.15, s))
add("Pod", "Mic Vox Hey", "microhouse",    lambda s: formant(220, "e", 0.3, 0.12))
add("Pod", "Mic Shaker Long", "microhouse", lambda s: noise_fx(0.4, "breath", s))
add("Pod", "Ro Wood Perc", "rominimal",    lambda s: perc(1300, 0.15, 0.03, 0.1, ((1, 1), (2.7, 0.3)), 0.2, s))
add("Pod", "Ro Clave", "rominimal",        lambda s: perc(2500, 0.15, 0.03, 0.0, ((1, 1),), 0.1, s))
add("Pod", "Ro Click Perc", "rominimal",   lambda s: perc(1800, 0.08, 0.012, 0.5, ((1, 1), (1.7, 0.5)), 0.4, s, 6000))
add("Pod", "Ro Log Drum", "rominimal",     lambda s: perc(160, 0.5, 0.2, 0.15, ((1, 1), (2.76, 0.4)), 0.0, s))
add("Pod", "Ro Metal Tic", "rominimal",    lambda s: perc(3300, 0.12, 0.02, 0.0, ((1, 1), (1.41, 0.7), (2.3, 0.5)), 0.1, s))
add("Pod", "Ro Water Drop", "rominimal",   lambda s: perc(900, 0.2, 0.05, -0.5, ((1, 1),), 0.0, s))
add("Pod", "House Shaker", "house",        lambda s: noise_fx(0.3, "breath", s))
add("Pod", "House Tambourine", "house",    lambda s: hat(0.08, 6000, 0.45, 700, 0.35, seed=s, bp=9000, shaker=True))
add("Pod", "House Conga", "house",         lambda s: perc(300, 0.45, 0.14, 0.3, ((1, 1), (2.1, 0.2)), 0.25, s))
add("Pod", "House Cowbell", "house",       lambda s: cowbell808(0.4, 0.12))
add("Pod", "House Vox Uh", "house",        lambda s: formant(160, "u", 0.35, 0.15))
add("Pod", "House Bongo Hi", "house",      lambda s: perc(480, 0.3, 0.07, 0.4, ((1, 1), (2.4, 0.2)), 0.3, s))
add("Pod", "TH 808 Cowbell", "techhouse",  lambda s: cowbell808(0.35, 0.08))
add("Pod", "TH 707 Tamb", "techhouse",     lambda s: hat(0.06, 7000, 0.5, 650, 0.3, seed=s, bp=9500, shaker=True))
add("Pod", "TH Rim Perc", "techhouse",     lambda s: rim(1250, 0.025, noise=0.3, seed=s))
add("Pod", "TH Tom", "techhouse",          lambda s: perc(150, 0.45, 0.18, 0.5, ((1, 1),), 0.2, s))
add("Pod", "TH Perc Stab", "techhouse",    lambda s: perc(720, 0.25, 0.06, 0.3, ((1, 1), (1.6, 0.4)), 0.3, s))
add("Pod", "TH Clave 808", "techhouse",    lambda s: perc(2500, 0.12, 0.025, 0.0, ((1, 1),), 0.05, s))
add("Pod", "Deep Vinyl", "deephouse",      lambda s: noise_fx(1.2, "crackle", s + 3))
add("Pod", "Deep Dub Perc", "deephouse",   lambda s: delay_fx(perc(520, 0.25, 0.08, 0.2, ((1, 1), (2.0, 0.3)), 0.2, s), 0.75, 120, 0.55, 0.5, 1400))
add("Pod", "Deep Lofi Conga", "deephouse", lambda s: lofi(perc(260, 0.45, 0.14, 0.3, ((1, 1), (2.1, 0.2)), 0.2, s), 9, 2, 4500, 0.4, s))
add("Pod", "Deep Vox Snip", "deephouse",   lambda s: lofi(formant(200, "a", 0.3, 0.12), 10, 2, 5000))
add("Pod", "Deep Steel Pan", "deephouse",  lambda s: fm_tone(523, 2.0, 1.0, 0.8, 0.35, 0.1))
add("Pod", "Deep Toy Blip", "deephouse",   lambda s: fm_tone(1046, 3.01, 1.5, 0.3, 0.08, 0.02))
add("Pod", "Electro Zap", "breakbeat",     lambda s: osc_sine(60 + 3000 * env_exp(t_axis(0.4), 0.03)) * env_exp(t_axis(0.4), 0.1))
add("Pod", "Electro Laser", "breakbeat",   lambda s: osc_square(200 + 2000 * env_exp(t_axis(0.4), 0.05)) * env_exp(t_axis(0.4), 0.12) * 0.6)
add("Pod", "Electro 808 Cowbell", "breakbeat", lambda s: cowbell808(0.4, 0.1))
add("Pod", "Electro Robot Voice", "breakbeat", lambda s: formant(110, "e", 0.4, 0.2) * (np.sin(2 * np.pi * 40 * t_axis(0.4)) > 0))
add("Pod", "Electro Bleep", "breakbeat",   lambda s: tonal([1760, 2637], [1, 0.4], 0.12, kind="square", tau=0.03))
add("Pod", "Electro 808 Tom", "breakbeat", lambda s: perc(130, 0.5, 0.25, 0.4, ((1, 1),), 0.1, s))

GENRES = ["techno", "minimal", "microhouse", "rominimal", "house", "techhouse", "deephouse", "breakbeat"]
seed = 1000
for module in ("Kick", "Snare", "Tie", "Pod"):
    prefix = module[0]
    for i, (name, genre, fn) in enumerate(G[module]):
        seed += 1
        write(module, f"{prefix}{i + 1:02d} {name}", fn(seed), [genre])
i = 0
for genre in GENRES:
    for name, (tau, hp, metal, base), otau, extra in HATS[genre]:
        i += 1; seed += 1
        fname = f"H{i:02d} {name}"
        write("Hat", fname, hat(tau, hp, metal, base, max(0.12, tau * 7), seed=seed, **extra), [genre])
        write("Hat", fname, hat(otau, hp, metal, base, max(0.3, otau * 6), seed=seed, **extra), [genre], subdir="open")

with open(os.path.join(OUT, "manifest.json"), "w") as f:
    json.dump({"version": 3, "genres": GENRES, "modules": manifest}, f, indent=1)
print({k: len(v) for k, v in manifest.items()})

import zipfile, shutil
zpath = os.path.join(os.path.dirname(os.path.abspath(OUT)), "factory_samples.zip")
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk(OUT):
        for fn in sorted(files):
            full = os.path.join(root, fn)
            z.write(full, os.path.relpath(full, OUT))
shutil.copy(os.path.join(OUT, "manifest.json"), os.path.join(os.path.dirname(os.path.abspath(OUT)), "samples_manifest.json"))
print("archive:", zpath, os.path.getsize(zpath) // 1024, "KB")
