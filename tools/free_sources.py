#!/usr/bin/env python3
"""
Free (CC0 / Public Domain) sample sources for the Dualbox factory library (Tie + Pod).

The audio is read from local clones of two public repositories (not shipped in the Dualbox repo):
  danigb/samples  (https://github.com/danigb/samples)   - VCSL (CC0), Sample Pi (CC0), D. Smolken basses (CC0),
                                                           J. Learman Rhodes (CC0), Splendid Grand Piano (Public Domain)
  oramics/sampled (https://github.com/oramics/sampled)  - LinnDrum LM-2, Roland TR-505, Roland CR-78 (Public Domain)
Set FREE_SRC to the folder that holds "danigb/samples" and "oramics/sampled" (default: ~ i.e. /home/user).

Every Tie sample is tuned to C (basses) or built on a C root (chords); see resources/SAMPLE_CREDITS.md.
"""
import os, subprocess, functools
import numpy as np

SR = 44100
ROOT = os.environ.get("FREE_SRC", "/home/user")
DANI = os.path.join(ROOT, "danigb", "samples", "audio")
ORA = os.path.join(ROOT, "oramics", "sampled", "DM")


@functools.lru_cache(maxsize=None)
def _load(path):
    raw = subprocess.run(["ffmpeg", "-v", "quiet", "-i", path, "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"],
                         check=True, capture_output=True).stdout
    x = np.frombuffer(raw, dtype=np.float32).astype(float)
    if len(x) == 0: raise RuntimeError("empty audio: " + path)
    return x

def load(*parts, length=None, fade=0.03):
    p = os.path.join(*parts)
    if not os.path.isfile(p): raise FileNotFoundError(p)
    x = _load(p).copy()
    # trim leading silence
    idx = np.where(np.abs(x) > 0.01 * np.max(np.abs(x)))[0]
    if len(idx): x = x[max(0, idx[0] - 32):]
    if length and len(x) > int(length * SR):
        n = int(length * SR); f = int(fade * SR)
        x = x[:n]; x[-f:] *= np.linspace(1, 0, f)
    return x

def dani(*p, **k): return load(DANI, *p, **k)
def ora(*p, **k):  return load(ORA, *p, **k)


# ------------------------------------------------------------------------------------- pitch
def f0(x, fmin=25.0, fmax=1500.0):
    """YIN fundamental estimate on the body of the note (after the attack)."""
    a = int(0.04 * SR); seg = x[a:a + int(0.25 * SR)]
    if len(seg) < 4096: seg = x[:int(0.25 * SR)]
    seg = seg - np.mean(seg)
    W = len(seg) // 2
    tmax = min(int(SR / fmin), W - 1); tmin = int(SR / fmax)
    d = np.array([np.sum((seg[:W] - seg[t:t + W]) ** 2) for t in range(tmax + 1)])
    cm = d[1:] * np.arange(1, len(d)) / np.maximum(np.cumsum(d[1:]), 1e-12)
    cm = np.concatenate([[1.0], cm])
    cand = np.where(cm[tmin:] < 0.15)[0]
    if len(cand) == 0: t = tmin + int(np.argmin(cm[tmin:]))
    else:
        t = tmin + cand[0]
        while t + 1 < len(cm) and cm[t + 1] < cm[t]: t += 1
    if 0 < t < len(cm) - 1:                       # parabolic refinement
        y0, y1, y2 = cm[t - 1], cm[t], cm[t + 1]
        den = y0 - 2 * y1 + y2
        t = t + (0.5 * (y0 - y2) / den if abs(den) > 1e-12 else 0)
    return SR / t

def midi(f): return 69 + 12 * np.log2(f / 440.0)
def hz(m):   return 440.0 * 2 ** ((m - 69) / 12)

def repitch(x, ratio):
    n = int(len(x) / ratio)
    return np.interp(np.arange(n) * ratio, np.arange(len(x)), x)

def to_c(x, octave=None):
    """Tune a pitched sample exactly to the nearest C (or to C<octave>)."""
    m = midi(f0(x))
    target = (round((m - 24) / 12) * 12 + 24) if octave is None else 12 * (octave + 1)
    return repitch(x, hz(target) / hz(m))

def instrument(folder, pattern=None):
    """Detected pitch (MIDI) of every file of a multisampled instrument folder: [(midi, path)]."""
    out = []
    for fn in sorted(os.listdir(folder)):
        if not fn.endswith(".ogg") or (pattern and pattern not in fn): continue
        x = _load(os.path.join(folder, fn))
        out.append((midi(f0(x)), os.path.join(folder, fn)))
    return out

def chord(folder, notes, pattern=None, length=2.0, spread=0.0):
    """Builds a chord from single-note samples: each note is taken from the closest sample and tuned exactly."""
    inst = _inst_cache(folder, pattern)
    n = int(length * SR); y = np.zeros(n)
    for k, m in enumerate(notes):
        src_m, path = min(inst, key=lambda it: abs(it[0] - m))
        x = repitch(load(path), hz(m) / hz(src_m))
        off = int(spread * k * SR)
        x = x[: n - off]
        y[off:off + len(x)] += x
    f = int(0.08 * SR); y[-f:] *= np.linspace(1, 0, f)
    return y / max(1e-9, np.max(np.abs(y)))

@functools.lru_cache(maxsize=None)
def _inst_cache(folder, pattern):
    return tuple(instrument(folder, pattern))

C = {"min": (0, 3, 7), "m7": (0, 3, 7, 10), "m9": (0, 3, 7, 10, 14), "m11": (0, 3, 7, 10, 14, 17),
     "sus": (0, 5, 7, 10), "madd9": (0, 3, 7, 14), "m6": (0, 3, 7, 9)}

def cnotes(kind, root=48, voicing=None):
    iv = C[kind]
    ns = [root + i for i in iv]
    if voicing == "open":   # spread voicing: root low, rest up an octave
        ns = [root] + [root + 12 + i for i in iv[1:]]
    return ns
