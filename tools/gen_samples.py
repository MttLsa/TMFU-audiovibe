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


# ---------------------------------------------------------------- v4: soft / clean instruments
def tom(f, tau, drop=0.45, length=None, stick=0.12, seed=1):
    """Soft membrane tom: pitch drop, faint second mode, gentle stick noise. No saturation."""
    length = length or min(1.6, tau * 6 + 0.1); t = t_axis(length); r = rng(seed)
    fc = f * (1 + drop * env_exp(t, 0.03))
    x = osc_sine(fc) + 0.22 * osc_sine(fc * 1.5) * env_exp(t, tau * 0.4)
    x = x * env_ad(t, 0.0015, tau)
    x += stick * svf(r.standard_normal(len(t)), 2200, 0.8, "bp") * env_exp(t, 0.006)
    return svf(x, 5000, 0.7, "lp")

def conga(f, tau, slap=0.0, seed=1, mute=False):
    """Conga / bongo: circular membrane modes (1, 1.59, 2.14) with low weights, optional slap."""
    length = min(1.2, tau * 6 + 0.08); t = t_axis(length); r = rng(seed)
    fc = f * (1 + 0.18 * env_exp(t, 0.012))
    x = osc_sine(fc) + 0.18 * osc_sine(fc * 1.59) * env_exp(t, tau * 0.5) + 0.08 * osc_sine(fc * 2.14) * env_exp(t, tau * 0.3)
    x = x * env_ad(t, 0.001, tau * (0.45 if mute else 1.0))
    x += (0.1 + slap) * svf(r.standard_normal(len(t)), 1800 + 1500 * slap, 0.9, "bp") * env_exp(t, 0.008 + 0.01 * slap)
    return svf(x, 6000, 0.7, "lp")

def softrim(f=480, decay=0.03, seed=1, body=1.0):
    """Rimshot without metal: woody body + short filtered click, low-passed."""
    t = t_axis(0.25); r = rng(seed)
    x = body * (osc_sine(np.full_like(t, f)) + 0.35 * osc_sine(np.full_like(t, f * 1.47))) * env_exp(t, decay)
    x += 0.6 * svf(r.standard_normal(len(t)), 2600, 1.0, "bp") * env_exp(t, 0.004)
    return svf(x, 5500, 0.7, "lp")

def shaker(length, tau, fc=6000, seed=1, att=0.015):
    t = t_axis(length); r = rng(seed)
    return svf(r.standard_normal(len(t)), fc, 0.8, "bp") * env_ad(t, att, tau)

def softsnare(tone=190, body_tau=0.06, n_tau=0.13, n_fc=2800, mix=0.6, length=0.5, room=0.0, seed=1, lp=7000):
    x = snare(tone, body_tau, n_tau, n_fc, "bp", mix=mix, length=length, drive=1.0, seed=seed, q=0.7)
    if room: x = x + room * convolve(x, reverb_ir(0.4, seed, 0.5))[:len(x)] * 0.5
    return svf(x, lp, 0.7, "lp")

def softclap(fc=1100, tail=0.15, bursts=4, spacing=0.011, room=0.3, length=0.6, seed=1, lp=6500):
    return svf(clap(fc, spacing, bursts, tail, length=length, q=1.2, seed=seed, room=room), lp, 0.7, "lp")

def rhodes(freqs, length=1.5, tau=0.9, idx=1.0):
    return sum(fm_tone(f, 1.0, idx, length, tau, 0.25) for f in freqs) * (0.9 / len(freqs))

def softbass(f, length, tau, cutoff=500, kind="saw", q=0.9, cut_env=1.0, sub=0.5):
    x = tonal([f], [1], length, kind=kind, cutoff=cutoff, cut_env=cut_env, q=q, tau=tau, drive=1.0)
    return x + sub * tonal([f], [1], length, tau=tau, drive=1.0)

C1, C2, C3, C4 = 32.70, 65.41, 130.81, 261.63
Eb3, G2, F2 = 155.56, 98.0, 87.31

# ---------------------------------------------------------------- the library: 6 per genre and module (hats: 4 pairs)
G = {}   # module -> list of (name, genre, fn)
def add(module, name, genre, fn): G.setdefault(module, []).append((name, genre, fn))

# ===== KICKS (clean: drive <= 1.25, no bit-crush / distortion)
K = lambda *a, **k: kick(*a, **dict(dict(drive=1.15), **k))
add("Kick", "Tek Rumble Deep", "techno",   lambda s: rumble_kick(46, 180, 0.035, 0.38, 1.6, rumble=0.45, rumble_lp=130, rumble_drive=1.3, click=0.25, drive=1.2, seed=s))
add("Kick", "Tek Rumble Soft", "techno",   lambda s: rumble_kick(48, 200, 0.03, 0.30, 1.4, rumble=0.35, rumble_lp=150, rumble_drive=1.2, click=0.3, drive=1.15, seed=s))
add("Kick", "Tek Hypno", "techno",         lambda s: K(50, 200, 0.03, 0.40, 1.0, click=0.3, sub=0.2, seed=s))
add("Kick", "Tek Rolling", "techno",       lambda s: K(53, 230, 0.022, 0.22, 0.7, click=0.4, seed=s))
add("Kick", "Tek Warehouse", "techno",     lambda s: K(50, 240, 0.025, 0.30, 0.9, click=0.45, drive=1.25, seed=s))
add("Kick", "Tek Low Thud", "techno",      lambda s: K(43, 140, 0.05, 0.5, 1.4, click=0.12, seed=s))
add("Kick", "Min Dry", "minimal",          lambda s: K(58, 190, 0.018, 0.12, 0.4, click=0.3, seed=s))
add("Kick", "Min Click", "minimal",        lambda s: K(66, 350, 0.006, 0.07, 0.25, click=0.6, body_hp=40, seed=s))
add("Kick", "Min Felt", "minimal",         lambda s: svf(K(56, 170, 0.02, 0.15, 0.45, click=0.15, seed=s), 3000, 0.7, "lp"))
add("Kick", "Min Soft Thump", "minimal",   lambda s: K(54, 120, 0.035, 0.16, 0.5, click=0.05, drive=1.05, seed=s))
add("Kick", "Min Jazz Kick", "minimal",    lambda s: convolve(K(62, 140, 0.02, 0.1, 0.35, click=0.2, noise=0.3, seed=s), reverb_ir(0.25, s, 0.6))[:int(0.6 * SR)])
add("Kick", "Min Micro", "minimal",        lambda s: K(72, 280, 0.008, 0.05, 0.2, click=0.45, body_hp=50, seed=s))
add("Kick", "Mic Round Sub", "microhouse", lambda s: K(48, 110, 0.05, 0.45, 1.2, click=0.08, sub=0.3, seed=s))
add("Kick", "Mic Rubber", "microhouse",    lambda s: K(56, 160, 0.03, 0.2, 0.6, click=0.12, bend=25, seed=s))
add("Kick", "Mic Soft", "microhouse",      lambda s: K(52, 130, 0.03, 0.22, 0.6, click=0.05, drive=1.05, seed=s))
add("Kick", "Mic Deep Long", "microhouse", lambda s: K(45, 100, 0.06, 0.7, 1.8, click=0.05, sub=0.3, seed=s))
add("Kick", "Mic Wood", "microhouse",      lambda s: K(64, 190, 0.02, 0.09, 0.3, click=0.3, noise=0.15, seed=s))
add("Kick", "Mic Warm", "microhouse",      lambda s: svf(K(54, 150, 0.03, 0.25, 0.7, click=0.1, seed=s), 2500, 0.7, "lp"))
add("Kick", "Ro Punch", "rominimal",       lambda s: K(55, 170, 0.025, 0.17, 0.5, click=0.2, seed=s))
add("Kick", "Ro Round", "rominimal",       lambda s: K(50, 120, 0.04, 0.26, 0.8, click=0.06, sub=0.15, seed=s))
add("Kick", "Ro Tight", "rominimal",       lambda s: K(60, 200, 0.015, 0.1, 0.35, click=0.25, seed=s))
add("Kick", "Ro Sub", "rominimal",         lambda s: K(46, 95, 0.06, 0.55, 1.5, click=0.03, sub=0.35, seed=s))
add("Kick", "Ro Clean 909", "rominimal",   lambda s: K(52, 210, 0.022, 0.2, 0.6, click=0.3, noise=0.08, seed=s))
add("Kick", "Ro Felt", "rominimal",        lambda s: K(57, 130, 0.03, 0.14, 0.45, click=0.02, drive=1.05, seed=s))
add("Kick", "House 909 Warm", "house",     lambda s: K(52, 230, 0.024, 0.32, 0.9, click=0.35, noise=0.12, seed=s))
add("Kick", "House 909 Clean", "house",    lambda s: K(54, 250, 0.02, 0.28, 0.8, click=0.4, noise=0.12, seed=s))
add("Kick", "House Garage", "house",       lambda s: K(50, 180, 0.035, 0.35, 1.0, click=0.2, sub=0.2, seed=s))
add("Kick", "House Deep", "house",         lambda s: K(47, 140, 0.04, 0.45, 1.2, click=0.12, sub=0.25, seed=s))
add("Kick", "House Box", "house",          lambda s: K(60, 220, 0.018, 0.16, 0.5, click=0.3, seed=s))
add("Kick", "House Round", "house",        lambda s: K(50, 160, 0.03, 0.3, 0.9, click=0.15, seed=s))
add("Kick", "TH 808 Clean", "techhouse",   lambda s: K(47, 110, 0.05, 0.42, 1.2, click=0.1, seed=s))
add("Kick", "TH 707", "techhouse",         lambda s: K(60, 230, 0.017, 0.14, 0.45, click=0.35, noise=0.12, seed=s))
add("Kick", "TH 909", "techhouse",         lambda s: K(53, 250, 0.022, 0.27, 0.8, click=0.4, noise=0.12, seed=s))
add("Kick", "TH Jack", "techhouse",        lambda s: K(56, 230, 0.02, 0.2, 0.6, click=0.4, seed=s))
add("Kick", "TH UK Punch", "techhouse",    lambda s: K(54, 220, 0.022, 0.22, 0.7, click=0.3, sub=0.1, seed=s))
add("Kick", "TH Detroit", "techhouse",     lambda s: K(50, 190, 0.03, 0.34, 0.9, click=0.25, seed=s))
add("Kick", "Deep Dusty", "deephouse",     lambda s: svf(K(52, 160, 0.03, 0.3, 0.9, click=0.12, seed=s), 2800, 0.7, "lp"))
add("Kick", "Deep Thump", "deephouse",     lambda s: svf(K(55, 140, 0.03, 0.2, 0.6, click=0.08, seed=s), 2200, 0.7, "lp"))
add("Kick", "Deep Soft 909", "deephouse",  lambda s: K(52, 210, 0.026, 0.3, 0.9, click=0.25, noise=0.08, seed=s))
add("Kick", "Deep Dub", "deephouse",       lambda s: K(48, 120, 0.045, 0.4, 1.1, click=0.08, sub=0.25, seed=s))
add("Kick", "Deep Tape", "deephouse",      lambda s: svf(K(54, 190, 0.025, 0.28, 0.8, click=0.2, seed=s), 3500, 0.7, "lp"))
add("Kick", "Deep Round", "deephouse",     lambda s: K(50, 130, 0.04, 0.32, 0.9, click=0.05, seed=s))
add("Kick", "Electro 808 Boom", "breakbeat", lambda s: K(42, 150, 0.06, 0.9, 2.0, click=0.12, seed=s))
add("Kick", "Electro 808 Long", "breakbeat", lambda s: K(45, 120, 0.05, 0.7, 1.8, click=0.08, seed=s))
add("Kick", "Electro Punch", "breakbeat",  lambda s: K(58, 260, 0.018, 0.18, 0.5, click=0.4, seed=s))
add("Kick", "Electro Short 808", "breakbeat", lambda s: K(50, 140, 0.04, 0.25, 0.7, click=0.12, seed=s))
add("Kick", "Electro Tight", "breakbeat",  lambda s: K(62, 300, 0.012, 0.12, 0.4, click=0.45, seed=s))
add("Kick", "Electro 808 Mid", "breakbeat", lambda s: K(48, 160, 0.05, 0.5, 1.4, click=0.15, seed=s))

# ===== SNARE (only soft snares and soft claps)
add("Snare", "Tek Soft Snare", "techno",   lambda s: softsnare(200, 0.04, 0.14, 3000, mix=0.75, room=0.4, seed=s))
add("Snare", "Tek Room Clap", "techno",    lambda s: convolve(softclap(1200, 0.12, seed=s, room=0.3), reverb_ir(0.6, s, 0.4))[:int(0.9 * SR)])
add("Snare", "Tek Dark Snare", "techno",   lambda s: softsnare(180, 0.05, 0.12, 2200, mix=0.7, seed=s, lp=4500))
add("Snare", "Tek Clap Dry", "techno",     lambda s: softclap(1250, 0.08, length=0.4, room=0.0, seed=s))
add("Snare", "Tek Wide Clap", "techno",    lambda s: softclap(1050, 0.2, bursts=5, room=0.6, length=0.8, seed=s))
add("Snare", "Tek Long Snare", "techno",   lambda s: softsnare(190, 0.05, 0.25, 2600, mix=0.8, length=0.6, seed=s))
add("Snare", "Min Soft Snare", "minimal",  lambda s: softsnare(240, 0.02, 0.05, 3500, mix=0.7, length=0.25, seed=s))
add("Snare", "Min Tiny Clap", "minimal",   lambda s: softclap(1400, 0.04, bursts=3, spacing=0.008, room=0.0, length=0.25, seed=s))
add("Snare", "Min Brush Snare", "minimal", lambda s: softsnare(220, 0.015, 0.09, 4000, mix=0.9, length=0.35, seed=s, lp=5500))
add("Snare", "Min Felt Clap", "minimal",   lambda s: softclap(1100, 0.06, bursts=3, room=0.2, length=0.35, seed=s, lp=4500))
add("Snare", "Min Paper Snare", "minimal", lambda s: softsnare(250, 0.012, 0.045, 4200, mix=0.8, length=0.2, seed=s))
add("Snare", "Min Room Snare", "minimal",  lambda s: softsnare(210, 0.03, 0.08, 3000, mix=0.65, length=0.4, room=0.6, seed=s))
add("Snare", "Mic Organic Clap", "microhouse", lambda s: convolve(softclap(1100, 0.07, bursts=3, room=0.0, length=0.35, seed=s), reverb_ir(0.3, s, 0.5))[:int(0.5 * SR)])
add("Snare", "Mic Soft Snare", "microhouse", lambda s: softsnare(230, 0.02, 0.07, 3800, mix=0.75, length=0.3, seed=s))
add("Snare", "Mic Hand Clap", "microhouse", lambda s: softclap(1300, 0.05, bursts=3, spacing=0.009, room=0.1, length=0.3, seed=s))
add("Snare", "Mic Dust Snare", "microhouse", lambda s: softsnare(220, 0.015, 0.05, 3500, mix=0.8, length=0.25, seed=s, lp=5000))
add("Snare", "Mic Warm Clap", "microhouse", lambda s: softclap(950, 0.1, room=0.4, length=0.45, seed=s, lp=4000))
add("Snare", "Mic Short Snare", "microhouse", lambda s: softsnare(260, 0.015, 0.04, 4000, mix=0.7, length=0.2, seed=s))
add("Snare", "Ro Short Clap", "rominimal", lambda s: softclap(1400, 0.05, bursts=3, spacing=0.008, room=0.0, length=0.25, seed=s))
add("Snare", "Ro Soft Snare", "rominimal", lambda s: softsnare(170, 0.02, 0.07, 2200, mix=0.6, length=0.3, seed=s))
add("Snare", "Ro Room Clap", "rominimal",  lambda s: convolve(softclap(1200, 0.05, bursts=3, room=0.0, length=0.25, seed=s), reverb_ir(0.4, s, 0.5))[:int(0.6 * SR)])
add("Snare", "Ro Tight Snare", "rominimal", lambda s: softsnare(240, 0.015, 0.04, 3200, mix=0.7, length=0.2, seed=s))
add("Snare", "Ro Velvet Clap", "rominimal", lambda s: softclap(1000, 0.09, room=0.3, length=0.4, seed=s, lp=4500))
add("Snare", "Ro Ghost Snare", "rominimal", lambda s: softsnare(200, 0.02, 0.05, 2600, mix=0.7, length=0.25, seed=s, lp=4000))
add("Snare", "House 909 Clap", "house",    lambda s: softclap(1150, 0.15, room=0.4, seed=s))
add("Snare", "House Garage Snare", "house", lambda s: softsnare(185, 0.05, 0.12, 3200, mix=0.7, seed=s))
add("Snare", "House Clap Stack", "house",  lambda s: softclap(1050, 0.22, bursts=5, room=0.6, length=0.8, seed=s))
add("Snare", "House Soft Clap", "house",   lambda s: softclap(1250, 0.1, room=0.2, length=0.45, seed=s))
add("Snare", "House 909 Snare", "house",   lambda s: softsnare(180, 0.07, 0.15, 3200, mix=0.65, seed=s))
add("Snare", "House Room Snare", "house",  lambda s: softsnare(200, 0.05, 0.12, 2800, mix=0.7, room=0.5, seed=s))
add("Snare", "TH 707 Clap", "techhouse",   lambda s: softclap(1250, 0.09, length=0.4, room=0.1, seed=s))
add("Snare", "TH 808 Clap", "techhouse",   lambda s: softclap(1050, 0.13, length=0.5, room=0.3, seed=s))
add("Snare", "TH 707 Snare", "techhouse",  lambda s: softsnare(220, 0.04, 0.09, 3000, mix=0.6, length=0.35, seed=s))
add("Snare", "TH Tight Clap", "techhouse", lambda s: softclap(1500, 0.06, bursts=3, spacing=0.008, room=0.0, length=0.3, seed=s))
add("Snare", "TH Warm Snare", "techhouse", lambda s: softsnare(200, 0.05, 0.11, 3500, mix=0.7, seed=s))
add("Snare", "TH Layer Clap", "techhouse", lambda s: softclap(1150, 0.16, bursts=5, room=0.5, length=0.6, seed=s))
add("Snare", "Deep Dusty Clap", "deephouse", lambda s: softclap(1000, 0.18, room=0.6, length=0.7, seed=s, lp=4500))
add("Snare", "Deep Warm Snare", "deephouse", lambda s: softsnare(175, 0.05, 0.14, 2400, mix=0.6, seed=s, lp=4500))
add("Snare", "Deep Short Clap", "deephouse", lambda s: softclap(1200, 0.06, bursts=3, room=0.1, length=0.3, seed=s))
add("Snare", "Deep Dub Clap", "deephouse", lambda s: delay_fx(softclap(1100, 0.08, room=0.2, length=0.35, seed=s), 0.75, 120, 0.4, 0.3, 1500))
add("Snare", "Deep Soft Clap", "deephouse", lambda s: softclap(950, 0.25, room=1.0, length=0.9, seed=s, lp=5000))
add("Snare", "Deep Tape Snare", "deephouse", lambda s: softsnare(190, 0.06, 0.13, 3000, mix=0.65, seed=s, lp=4000))
add("Snare", "Electro 808 Snare", "breakbeat", lambda s: svf(snare808(185, 0.6, 0.12, seed=s), 7000, 0.7, "lp"))
add("Snare", "Electro Snappy", "breakbeat", lambda s: svf(snare808(220, 0.75, 0.09, 0.35, seed=s), 6500, 0.7, "lp"))
add("Snare", "Electro Clap", "breakbeat",  lambda s: softclap(1200, 0.1, length=0.45, room=0.2, seed=s))
add("Snare", "Electro Break Snare", "breakbeat", lambda s: softsnare(200, 0.05, 0.12, 3200, mix=0.7, room=0.3, seed=s))
add("Snare", "Electro Soft 808", "breakbeat", lambda s: svf(snare808(170, 0.5, 0.1, seed=s), 5000, 0.7, "lp"))
add("Snare", "Electro Room Clap", "breakbeat", lambda s: softclap(1100, 0.15, bursts=4, room=0.5, length=0.6, seed=s))

# ===== HATS (closed + open pairs): (tau, hp, metal, base, open_tau, extra) - low metal ratio, soft
HATS = {
 "techno":     [("Tek 909 Hat", (0.04, 8000, 0.3, 430), 0.5, {}), ("Tek Soft Steel", (0.035, 7000, 0.35, 480), 0.5, {}),
                ("Tek Hiss Ride", (0.06, 9000, 0.15, 520), 1.2, dict(bp=6000)), ("Tek Dark", (0.05, 4500, 0.3, 300), 0.4, {})],
 "minimal":    [("Min Tick", (0.012, 9500, 0.3, 500), 0.12, {}), ("Min Crackle", (0.02, 7000, 0.0, 400), 0.15, dict(bp=8000)),
                ("Min Grain", (0.018, 7000, 0.1, 300), 0.14, dict(bp=6500)), ("Min Micro", (0.006, 12000, 0.3, 700), 0.06, {})],
 "microhouse": [("Mic Shaker", (0.07, 5000, 0.0, 400), 0.22, dict(shaker=True, bp=7000)), ("Mic Soft Hat", (0.03, 6000, 0.2, 360), 0.3, {}),
                ("Mic Rattle", (0.05, 4800, 0.05, 340), 0.18, dict(shaker=True, bp=5600)), ("Mic Dust", (0.01, 11000, 0.1, 520), 0.09, {})],
 "rominimal":  [("Ro Crisp", (0.02, 8500, 0.2, 450), 0.2, {}), ("Ro Shuffle", (0.03, 6500, 0.05, 410), 0.16, dict(bp=8500)),
                ("Ro Tsk", (0.025, 9000, 0.25, 470), 0.25, {}), ("Ro Soft Shaker", (0.05, 5500, 0.0, 380), 0.2, dict(shaker=True))],
 "house":      [("House 909 Hat", (0.045, 8000, 0.25, 420), 0.42, {}), ("House Open Tss", (0.05, 7000, 0.25, 390), 0.6, {}),
                ("House Soft Ride", (0.1, 6000, 0.3, 520), 1.0, dict(bp=7000)), ("House Shaker", (0.06, 6000, 0.0, 700), 0.28, dict(shaker=True, bp=8000))],
 "techhouse":  [("TH 808 Hat", (0.035, 7000, 0.35, 360), 0.32, {}), ("TH 707 Hat", (0.03, 8500, 0.3, 450), 0.35, {}),
                ("TH Jack Hat", (0.03, 8000, 0.25, 450), 0.38, {}), ("TH Shuffle", (0.035, 7500, 0.2, 430), 0.3, dict(bp=10000))],
 "deephouse":  [("Deep Lofi Hat", (0.04, 5500, 0.2, 360), 0.45, {}), ("Deep Brushy", (0.08, 3500, 0.0, 300), 0.3, dict(shaker=True)),
                ("Deep Dusty Ride", (0.1, 5000, 0.3, 500), 0.9, dict(bp=6000)), ("Deep Soft Tsss", (0.05, 7000, 0.2, 390), 0.55, {})],
 "breakbeat":  [("Electro 808 Hat", (0.03, 7500, 0.35, 360), 0.3, {}), ("Electro Tick", (0.015, 10000, 0.3, 520), 0.15, {}),
                ("Electro Soft Cymbal", (0.12, 5000, 0.2, 330), 1.1, {}), ("Electro Soft Hat", (0.035, 6500, 0.3, 470), 0.4, {})],
}

# ===== TIE (tonal, rooted on C unless the name says otherwise; soft: no bells / blips)
add("Tie", "Tek Dub Chord m7", "techno",   lambda s: delay_fx(tonal(chord(C3, "m7"), [1] * 4, 0.5, kind="saw", cutoff=800, cut_env=1.0, q=0.9, tau=0.15, detune=0.006, drive=1.0), 0.75, 130, 0.45, 0.4, 1600))
add("Tie", "Tek Rumble Bass C", "techno",  lambda s: softbass(C2, 1.0, 0.5, cutoff=220, sub=0.8))
add("Tie", "Tek Low Tom C", "techno",      lambda s: tom(C3, 0.28, seed=s))
add("Tie", "Tek Dark Pad Cm", "techno",    lambda s: strings(chord(C3, "min"), 2.0, attack=0.4, cutoff=1200))
add("Tie", "Tek Conga C", "techno",        lambda s: conga(C3 * 2, 0.16, seed=s))
add("Tie", "Tek Soft Stab Cm", "techno",   lambda s: strings(chord(C3, "m7"), 0.6, attack=0.01, cutoff=1600) * env_exp(t_axis(0.6), 0.15))
add("Tie", "Min Upright Bass C", "minimal", lambda s: karplus(C2, 1.0, 0.994, 0.3, s, 0.55))
add("Tie", "Min Jazz Rhodes Cm9", "minimal", lambda s: svf(rhodes(chord(C3, "m9"), 1.4, 0.8, 1.0), 4000, 0.7, "lp"))
add("Tie", "Min Soft Organ C", "minimal",  lambda s: organ([C3], 0.5, (1, 0.6, 0.2, 0.1, 0, 0), click=0.1, tau=0.25))
add("Tie", "Min Muted Guitar C", "minimal", lambda s: karplus(C3, 0.4, 0.985, 0.4, s, 0.5))
add("Tie", "Min Tom C", "minimal",         lambda s: tom(C3, 0.12, seed=s))
add("Tie", "Min Felt Bass C", "minimal",   lambda s: softbass(C2, 0.35, 0.12, cutoff=400, kind="sine"))
add("Tie", "Mic Sub Bass C", "microhouse", lambda s: tonal([C2], [1], 0.9, tau=0.45, drive=1.0))
add("Tie", "Mic Vox Ah C", "microhouse",   lambda s: formant(C3, "a", 0.9, 0.35))
add("Tie", "Mic Vox Oh C", "microhouse",   lambda s: formant(C3, "o", 0.7, 0.25))
add("Tie", "Mic Conga C", "microhouse",    lambda s: conga(C3 * 2, 0.14, seed=s))
add("Tie", "Mic Celestial Pad", "microhouse", lambda s: strings(chord(C3, "maj9"), 2.5, attack=0.6, cutoff=2200, detune=0.01))
add("Tie", "Mic Rubber Bass C", "microhouse", lambda s: softbass(C2, 0.6, 0.22, cutoff=500, q=1.0, cut_env=1.5))
add("Tie", "Ro Rolling Bass C", "rominimal", lambda s: softbass(C2, 0.5, 0.2, cutoff=450, sub=0.8))
add("Tie", "Ro Pluck Bass C", "rominimal", lambda s: softbass(C2, 0.5, 0.18, cutoff=350, q=1.2, cut_env=2.5))
add("Tie", "Ro Soft Pad Cm", "rominimal",  lambda s: strings(chord(C3, "m9"), 2.0, attack=0.3, cutoff=1600, detune=0.006))
add("Tie", "Ro Log Tom C", "rominimal",    lambda s: tom(C3, 0.2, drop=0.2, stick=0.2, seed=s))
add("Tie", "Ro Conga C", "rominimal",      lambda s: conga(C3 * 2, 0.12, slap=0.2, seed=s))
add("Tie", "Ro Dub Tone C", "rominimal",   lambda s: tonal([C2, C3 * 1.498], [1, 0.2], 0.9, kind="sine", tau=0.45, drive=1.0, vib=0.003))
add("Tie", "House Organ Bass C", "house",  lambda s: organ([C2], 0.5, (1, 0.9, 0.2, 0.3, 0.0, 0.0), click=0.2, tau=0.25))
add("Tie", "House Organ Chord Cm7", "house", lambda s: organ(chord(C3, "m7"), 0.7, (1, 0.7, 0.3, 0.2, 0.1, 0.0), click=0.15, tau=0.35))
add("Tie", "House Rhodes Cm9", "house",    lambda s: rhodes(chord(C3, "m9"), 1.6, 1.0, 1.2))
add("Tie", "House Piano Chord Cm", "house", lambda s: svf(rhodes(chord(C3, "m7"), 1.5, 0.7, 0.8) + 0.3 * tonal(chord(C3, "m7"), [1] * 4, 1.5, kind="saw", cutoff=1500, tau=0.5, drive=1.0), 3500, 0.7, "lp"))
add("Tie", "House Deep Bass C", "house",   lambda s: tonal([C2, C3], [1, 0.15], 0.6, tau=0.3, drive=1.0))
add("Tie", "House Garage Stab", "house",   lambda s: tonal(chord(C3, "m9"), [1] * 5, 0.4, kind="saw", cutoff=1500, cut_env=0.8, tau=0.12, detune=0.005, drive=1.0))
add("Tie", "TH Acid C", "techhouse",       lambda s: acid(C2, 0.35, cutoff=450, env=3.5, res=0.8, decay=0.12, drive=1.2))
add("Tie", "TH Detroit Strings Cm", "techhouse", lambda s: strings(chord(C3, "m7"), 2.2, attack=0.35, cutoff=2500, detune=0.01))
add("Tie", "TH Chord Stab Cm", "techhouse", lambda s: tonal(chord(C3, "min"), [1] * 3, 0.5, kind="saw", cutoff=1800, cut_env=1.0, tau=0.15, detune=0.006, drive=1.0))
add("Tie", "TH Rubber Bass C", "techhouse", lambda s: softbass(C2, 0.4, 0.14, cutoff=500, q=1.1, cut_env=2.0))
add("Tie", "TH Tom C", "techhouse",        lambda s: tom(C2 * 2, 0.2, seed=s))
add("Tie", "TH Conga C", "techhouse",      lambda s: conga(C3 * 2, 0.13, slap=0.15, seed=s))
add("Tie", "Deep Dub Chord Cm7", "deephouse", lambda s: delay_fx(tonal(chord(C3, "m7"), [1] * 4, 0.6, kind="saw", cutoff=750, cut_env=0.8, q=0.9, tau=0.2, detune=0.004, drive=1.0), 0.75, 120, 0.45, 0.4, 1400))
add("Tie", "Deep Warm Pad Cm9", "deephouse", lambda s: strings(chord(C3, "m9"), 2.5, attack=0.5, cutoff=1400, detune=0.008))
add("Tie", "Deep Funky Bass C", "deephouse", lambda s: softbass(C2, 0.35, 0.12, cutoff=700, q=1.2, cut_env=1.5, sub=0.6))
add("Tie", "Deep Lofi Rhodes Cm7", "deephouse", lambda s: svf(rhodes(chord(C3, "m7"), 1.6, 0.9, 1.0), 3000, 0.7, "lp"))
add("Tie", "Deep Conga C", "deephouse",    lambda s: conga(C3 * 2, 0.15, seed=s))
add("Tie", "Deep Sub C", "deephouse",      lambda s: tonal([C2], [1], 0.9, tau=0.5, drive=1.0))
add("Tie", "Electro Bass C", "breakbeat",  lambda s: softbass(C2, 0.4, 0.15, cutoff=600, q=1.2, cut_env=2.0))
add("Tie", "Electro Robot Bass C", "breakbeat", lambda s: softbass(C2, 0.5, 0.2, cutoff=350, kind="square", q=1.5, cut_env=4.0))
add("Tie", "Electro 808 Tom C", "breakbeat", lambda s: tom(C2 * 2, 0.3, drop=0.3, stick=0.05, seed=s))
add("Tie", "Electro Minor Stab", "breakbeat", lambda s: tonal(chord(C3, "min"), [1] * 3, 0.5, kind="saw", cutoff=2000, cut_env=1, tau=0.15, detune=0.01, drive=1.0))
add("Tie", "Electro Saw Bass C", "breakbeat", lambda s: softbass(C2, 0.5, 0.25, cutoff=800, sub=0.4))
add("Tie", "Electro Conga C", "breakbeat", lambda s: conga(C3 * 2, 0.14, seed=s))

# ===== POD (soft percussion: toms, congas, bongos, rimshots, shakers)
add("Pod", "Tek Low Tom", "techno",        lambda s: tom(95, 0.25, seed=s))
add("Pod", "Tek Mid Tom", "techno",        lambda s: tom(140, 0.2, seed=s))
add("Pod", "Tek Rimshot", "techno",        lambda s: softrim(450, 0.03, seed=s))
add("Pod", "Tek Conga Lo", "techno",       lambda s: conga(200, 0.16, seed=s))
add("Pod", "Tek Shaker", "techno",         lambda s: shaker(0.25, 0.06, 5500, s))
add("Pod", "Tek Tom Echo", "techno",       lambda s: delay_fx(tom(120, 0.15, seed=s), 0.375, 130, 0.4, 0.35, 1500))
add("Pod", "Min Soft Rim", "minimal",      lambda s: softrim(520, 0.02, seed=s))
add("Pod", "Min Tight Tom", "minimal",     lambda s: tom(180, 0.08, seed=s))
add("Pod", "Min Bongo", "minimal",         lambda s: conga(420, 0.07, seed=s))
add("Pod", "Min Shaker", "minimal",        lambda s: shaker(0.15, 0.03, 6500, s))
add("Pod", "Min Muted Conga", "minimal",   lambda s: conga(330, 0.12, seed=s, mute=True))
add("Pod", "Min Rim Echo", "minimal",      lambda s: delay_fx(softrim(500, 0.02, seed=s), 0.375, 124, 0.35, 0.3, 1800))
add("Pod", "Mic Conga Hi", "microhouse",   lambda s: conga(340, 0.12, slap=0.1, seed=s))
add("Pod", "Mic Conga Lo", "microhouse",   lambda s: conga(220, 0.16, seed=s))
add("Pod", "Mic Bongo", "microhouse",      lambda s: conga(430, 0.08, seed=s))
add("Pod", "Mic Shaker Long", "microhouse", lambda s: shaker(0.4, 0.12, 5000, s, att=0.06))
add("Pod", "Mic Rimshot", "microhouse",    lambda s: softrim(480, 0.025, seed=s))
add("Pod", "Mic Tom Hi", "microhouse",     lambda s: tom(200, 0.12, seed=s))
add("Pod", "Ro Log Tom", "rominimal",      lambda s: tom(160, 0.2, drop=0.15, stick=0.2, seed=s))
add("Pod", "Ro Conga Slap", "rominimal",   lambda s: conga(320, 0.1, slap=0.4, seed=s))
add("Pod", "Ro Bongo Lo", "rominimal",     lambda s: conga(360, 0.09, seed=s))
add("Pod", "Ro Soft Rim", "rominimal",     lambda s: softrim(560, 0.02, seed=s))
add("Pod", "Ro Shaker", "rominimal",       lambda s: shaker(0.2, 0.05, 6000, s))
add("Pod", "Ro Tom Echo", "rominimal",     lambda s: delay_fx(tom(150, 0.12, seed=s), 0.75, 124, 0.4, 0.3, 1400))
add("Pod", "House Shaker", "house",        lambda s: shaker(0.3, 0.07, 6000, s, att=0.02))
add("Pod", "House Conga", "house",         lambda s: conga(300, 0.14, slap=0.1, seed=s))
add("Pod", "House Bongo Hi", "house",      lambda s: conga(480, 0.07, seed=s))
add("Pod", "House Rimshot", "house",       lambda s: softrim(500, 0.03, seed=s))
add("Pod", "House Tom", "house",           lambda s: tom(130, 0.2, seed=s))
add("Pod", "House Conga Lo", "house",      lambda s: conga(210, 0.18, seed=s))
add("Pod", "TH Low Tom", "techhouse",      lambda s: tom(110, 0.22, seed=s))
add("Pod", "TH Rim Perc", "techhouse",     lambda s: softrim(600, 0.025, seed=s))
add("Pod", "TH Conga", "techhouse",        lambda s: conga(280, 0.13, slap=0.15, seed=s))
add("Pod", "TH Bongo", "techhouse",        lambda s: conga(450, 0.07, seed=s))
add("Pod", "TH Shaker", "techhouse",       lambda s: shaker(0.2, 0.05, 6500, s))
add("Pod", "TH Tom Hi", "techhouse",       lambda s: tom(190, 0.14, seed=s))
add("Pod", "Deep Lofi Conga", "deephouse", lambda s: svf(conga(260, 0.14, seed=s), 3500, 0.7, "lp"))
add("Pod", "Deep Dub Rim", "deephouse",    lambda s: delay_fx(softrim(460, 0.03, seed=s), 0.75, 120, 0.45, 0.4, 1400))
add("Pod", "Deep Brush Shaker", "deephouse", lambda s: shaker(0.35, 0.1, 4500, s, att=0.04))
add("Pod", "Deep Soft Tom", "deephouse",   lambda s: tom(120, 0.22, stick=0.05, seed=s))
add("Pod", "Deep Bongo", "deephouse",      lambda s: conga(400, 0.08, seed=s))
add("Pod", "Deep Conga Lo", "deephouse",   lambda s: conga(200, 0.18, seed=s))
add("Pod", "Electro 808 Tom Lo", "breakbeat", lambda s: tom(100, 0.3, drop=0.3, stick=0.05, seed=s))
add("Pod", "Electro 808 Tom Hi", "breakbeat", lambda s: tom(180, 0.2, drop=0.3, stick=0.05, seed=s))
add("Pod", "Electro 808 Conga", "breakbeat", lambda s: conga(330, 0.12, seed=s))
add("Pod", "Electro Rimshot", "breakbeat", lambda s: softrim(520, 0.025, seed=s))
add("Pod", "Electro Shaker", "breakbeat",  lambda s: shaker(0.2, 0.05, 6000, s))
add("Pod", "Electro Tom Mid", "breakbeat", lambda s: tom(140, 0.24, drop=0.3, stick=0.05, seed=s))

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
    json.dump({"version": 4, "genres": GENRES, "modules": manifest}, f, indent=1)
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
