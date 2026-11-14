#!/usr/bin/env python3
"""
Twinbox genre preset generator: 8 genres x 16 presets for Section A (drum-synth kit + pattern)
and Section B (sample kit + FX + patterns A-D), written to resources/presets_A.json / presets_B.json.

Style notes per genre (what the rules below try to capture):
  techno      125-138  driving 4/4, rumble (Tie sub on off-beats), off-beat hats, metallic perc, tension via Mod
  minimal     122-128  sparse, clicky, space; ghost notes, odd lane lengths (polymeter), small accents
  microhouse  120-126  swung 16ths, tiny clicks, vocal blips, glitchy re-orders (xFx slice / lofi)
  rominimal   118-130  (Villalobos, Bauch, Barac, Ohm Hourani, Sublee, Zavor) rolling hypnotic grooves,
                       long 32/64-step bars, off-grid wooden perc, rolling sub with portamento, heavy swing,
                       very little snare, shuffled rims, dub delays, constant micro-variation
  chicago     120-126  jacking 909/808 hats, claps on 2+4, cowbell, open hats, raw saturation
  techhouse   124-128  shuffle groove, bass stabs, percussive hooks, clap/snare layers
  deephouse   118-124  swung hats, rims, shakers, Rhodes/organ chords on Tie, warm reverb
  breakbeat   126-136  syncopated kick/snare breaks, ghost snares, electro zaps, chopped fx
"""
import json, os, random, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
manifest = json.load(open(os.path.join(ROOT, "resources", "samples_manifest.json")))
GENRES = ["techno", "minimal", "microhouse", "rominimal", "chicago", "techhouse", "deephouse", "breakbeat"]
LABEL = {"techno": "Techno", "minimal": "Minimal", "microhouse": "Microhouse", "rominimal": "Rominimal",
         "chicago": "Chicago House", "techhouse": "Tech House", "deephouse": "Deep House", "breakbeat": "Breakbeat"}
BPM = {"techno": (126, 138), "minimal": (122, 128), "microhouse": (120, 126), "rominimal": (118, 130),
       "chicago": (120, 126), "techhouse": (124, 128), "deephouse": (118, 124), "breakbeat": (126, 136)}

NAMES = {
    "techno": ["Concrete Ritual", "Rotor Seven", "Tunnel Vision", "Static Hymn", "Iron Lung", "Night Shift", "Sodium Glow",
               "Black Box", "Pressure Drop", "Hollow Grid", "Dead Signal", "Machine Psalm", "Kiln", "Low Orbit", "Sub Zero Unit", "Furnace Room"],
    "minimal": ["Dry Ink", "Pin Drop", "White Room", "Sparse Logic", "Thin Line", "Quiet Engine", "Paper Moon", "Negative Space",
                "Blank Page", "Small Talk", "Glass Floor", "Half Light", "Still Water", "Dot Matrix", "Low Ceiling", "Clean Cut"],
    "microhouse": ["Dust Bunny", "Pocket Lint", "Velcro", "Lint Roller", "Cut Glass Pop", "Tiny Tremors", "Breadcrumbs", "Fizz",
                   "Static Kiss", "Crumb Shuffle", "Felt Tip", "Rubber Band", "Lo Res Love", "Mouse Clicks", "Bubble Wrap", "Kitchen Floor"],
    "rominimal": ["Cismigiu Sunrise", "Lipscani Shuffle", "Strada Lunga", "Wooden Horse", "Dacia Groove", "Sapte Ore", "Paduri",
                  "Fluture", "Bucegi Fog", "Leagan", "Vama Veche", "Zmeu", "Carousel Dub", "Ploaie Calda", "Ceai de Tei", "Lume Lunga"],
    "chicago": ["Jackin Jefferson", "South Side Box", "Warehouse 1986", "Trax Tape", "Acid Errand", "Lake Shore Jack", "Bump The Box",
                "Basement Party", "Jack Your Body Twice", "Cabrini Groove", "Westside Snare", "Dance Mania", "Midnight Jack",
                "Pullman Shuffle", "Wicker Park", "Blue Line"],
    "techhouse": ["Bassline Theory", "Rubber Floor", "Afterhours Shuffle", "Neon Chevrons", "Steel Toe", "Pivot", "Low Slung",
                  "Tilt Shift", "Dry Martini", "Hook Line", "Off The Grid", "Club Sandwich", "Two Step Theory", "Tinted", "Bounce Cut", "Swagger"],
    "deephouse": ["Soft Focus", "Late Bloom", "Velvet Hour", "Warm Static", "Tidal Room", "Slow Glow", "Rhodes Less Travelled",
                  "Amber Lights", "Night Bus Home", "Pale Blue", "Honey Dust", "Moss", "Low Tide", "Kept Secret", "Satin Sky", "Slow Burn"],
    "breakbeat": ["Broken Arrow", "Snare Rush", "Electro Glide", "Funk Debris", "Zap Grid", "Uprock", "Hard Edit", "Scratch Theory",
                  "Ghost Break", "Breakdown 99", "Tape Splice", "Breaker Box", "Skippy", "Crowbar", "Jungle Gym", "Rewind Rifle"],
}


# ------------------------------------------------------------------------------- rhythm helpers
def euclid(k, n, rot=0):
    pat = [1 if ((i * k) % n) < k else 0 for i in range(n)]
    return pat[-rot:] + pat[:-rot] if rot else pat

def grid(s):
    """'x...X..' -> list of 0/1/2"""
    return [2 if c == "X" else (1 if c == "x" else 0) for c in s]

def tile(p, n):
    return [p[i % len(p)] for i in range(n)]

def sprinkle(p, prob, r, level=1, avoid=None):
    out = list(p)
    for i in range(len(out)):
        if out[i] == 0 and r.random() < prob and not (avoid and avoid[i]):
            out[i] = level
    return out

def thin(p, prob, r):
    return [0 if (v == 1 and r.random() < prob) else v for v in p]

def accent_some(p, prob, r):
    return [2 if (v == 1 and r.random() < prob) else v for v in p]

def vary_bars(base, n, r, mutate):
    """repeat a 16-step bar to n steps, mutating later bars for micro-variation."""
    out = []
    for bar in range(n // 16):
        b = base[:16] if len(base) >= 16 else tile(base, 16)
        out += mutate(list(b), bar) if bar > 0 else b
    return out[:n]

def s(p):
    return "".join(str(v) for v in p)


# ------------------------------------------------------------------------------- Section B (Limelite kit)
def pick(module, genre, r, prefer=None):
    items = manifest["modules"][module]
    cands = [it["file"][:-4] for it in items if genre in it["tags"] and not (module == "Kick" and "Ghost" in it["file"])]
    if prefer:
        pc = [c for c in cands if any(w.lower() in c.lower() for w in prefer)]
        if pc:
            cands = pc
    return r.choice(cands or [it["file"][:-4] for it in items])

def b_patterns(genre, r, length):
    """Returns 4 patterns (A-D) with trig lanes + value lanes."""
    pats = []
    for variant in range(4):
        L = length
        if genre == "techno":
            k = tile(grid("X---x---X---x---"), L)
            sn = tile(grid("----x-------x---"), L) if r.random() < 0.6 else [0] * L
            ch = tile(grid("--x---x---x---x-"), L)
            ch = sprinkle(ch, 0.12, r, 1, k)
            oh = tile(grid("--x---x---x---x-"), L) if variant % 2 else [0] * L
            tie = [0.0] * L
            for i in range(L):                       # rumble: off-beat sub notes
                if i % 4 in (2, 3) and r.random() < 0.6: tie[i] = round(0.42 + r.random() * 0.12, 2)
            pod = [round(0.4 + r.random() * 0.6, 2) if (i % 16) in r.sample(range(16), 3) else 0 for i in range(L)]
        elif genre == "minimal":
            k = tile(grid(r.choice(["X---x---X---x---", "X---x---X---x--x", "X-----x-X---x---"])), L)
            sn = tile(grid(r.choice(["----x-------x---", "-------x----x---", "----x---------x-"])), L)
            sn = sprinkle(sn, 0.03, r, 1, k)
            ch = thin(tile(grid("--x---x---x---x-"), L), 0.15, r)
            ch = sprinkle(ch, 0.08, r, 1, k)
            oh = [0] * L
            tie = [round(0.5 + r.choice([0, 0, 0.04, -0.04, 0.12]), 2) if (i % 16) in (3, 10) and r.random() < 0.7 else 0 for i in range(L)]
            pod = [round(0.3 + 0.5 * r.random(), 2) if r.random() < 0.12 else 0 for i in range(L)]
        elif genre == "microhouse":
            k = tile(grid(r.choice(["X---x---X---x---", "X--x----X---x---", "X---x--xX---x---"])), L)
            sn = tile(grid("----x-------x---"), L)
            sn = sprinkle(sn, 0.06, r, 1, k)
            ch = accent_some(sprinkle(tile(grid("x-xxx-x-x-xx--x-"), L), 0.1, r, 1), 0.2, r)
            oh = [0] * L
            tie = [round(0.5 + r.choice([0, 0.17, 0.25, 0.33]), 2) if r.random() < 0.18 else 0 for i in range(L)]
            pod = [round(0.25 + 0.6 * r.random(), 2) if r.random() < 0.3 else 0 for i in range(L)]
        elif genre == "rominimal":
            # long rolling grooves: kick with subtle displacement, almost no snare, rolling wood/perc, rolling sub
            base_k = grid(r.choice(["X---x---X---x---", "X---x---X--xx---", "X---x--xX---x---", "X-----x-X---x---"]))
            k = vary_bars(base_k, L, r, lambda b, bar: (b[:13] + [0, 1 if r.random() < 0.5 else 0, 0]) if bar % 2 else b)
            sn = [0] * L
            for i in range(L):
                if i % 16 in (4, 12) and r.random() < 0.25: sn[i] = 1           # occasional rim/snap
                if r.random() < 0.025: sn[i] = 1
            ch = tile(grid(r.choice(["--x---x---x---x-", "--xx--x---x--xx-", "-xx--x-x-xx--x-x"])), L)
            ch = sprinkle(thin(ch, 0.1, r), 0.06, r, 1)
            oh = [0] * L
            if variant >= 2:
                oh = [1 if i % 16 in (6, 14) and r.random() < 0.5 else 0 for i in range(L)]
            notes = r.choice([[0.5, 0.5, 0.58, 0.42], [0.5, 0.54, 0.5, 0.46], [0.5, 0.62, 0.5, 0.58], [0.5, 0.5, 0.5, 0.71]])
            tie = [0.0] * L
            for i in range(L):
                if i % 16 in (2, 7, 10, 15) or (i % 16 in (5, 13) and r.random() < 0.4):
                    if r.random() < 0.8: tie[i] = notes[(i // 4) % 4]
            pod = [0.0] * L
            for i in range(L):
                if r.random() < (0.38 if i % 2 else 0.22): pod[i] = round(0.3 + 0.65 * r.random(), 2)
        elif genre == "chicago":
            k = tile(grid("X---x---X---x---"), L)
            sn = tile(grid("----X-------X---"), L)
            ch = accent_some(tile(grid("xxxxxxxxxxxxxxxx"), L), 0.0, r)
            ch = [2 if i % 4 == 2 else (1 if v else 0) for i, v in enumerate(ch)]
            ch = thin(ch, 0.1, r)
            oh = tile(grid("--x---x---x---x-"), L) if r.random() < 0.7 else [0] * L
            tie = [round(r.choice([0.5, 0.5, 0.58, 0.62, 0.75]), 2) if i % 16 in r.sample([0, 3, 6, 8, 10, 11, 14], 4) else 0 for i in range(L)]
            pod = [0.7 if (i % 16) in (3, 7, 11, 15) and r.random() < 0.5 else 0 for i in range(L)]
        elif genre == "techhouse":
            k = tile(grid("X---x---X---x---"), L)
            sn = tile(grid("----X-------X---"), L)
            sn = sprinkle(sn, 0.04, r, 1, k)
            ch = tile(grid("--x---x---x---x-"), L)
            ch = sprinkle(ch, 0.25, r, 1, k)
            oh = [0] * L
            tie = [round(0.5 + r.choice([0, 0, 0.08, 0.25]), 2) if (i % 16) in (2, 6, 10, 14, 11, 7) and r.random() < 0.55 else 0 for i in range(L)]
            pod = [round(0.4 + 0.5 * r.random(), 2) if (i % 16) in r.sample(range(16), 5) else 0 for i in range(L)]
        elif genre == "deephouse":
            k = tile(grid(r.choice(["X---x---X---x---", "X---x---X---x--x"])), L)
            sn = tile(grid("----x-------x---"), L)
            ch = tile(grid("--x---x---x---x-"), L)
            ch = sprinkle(ch, 0.2, r)
            oh = tile(grid("------x-------x-"), L) if variant % 2 else [0] * L
            tie = [0.0] * L
            for bar in range(L // 16):
                for pos in r.choice([[0, 6, 10], [3, 11], [0, 7], [2, 10, 14]]):
                    tie[bar * 16 + pos] = round(r.choice([0.5, 0.5, 0.58, 0.42]), 2)
            pod = [round(0.3 + 0.4 * r.random(), 2) if i % 2 == 1 and r.random() < 0.35 else 0 for i in range(L)]
        else:  # breakbeat
            k = tile(grid(r.choice(["X-----x---x-----", "X--x------X--x--", "X-x-------x-x---", "X------xx-----x-"])), L)
            sn = tile(grid(r.choice(["----X-------X---", "----X--x----X--x", "----X-------X-x-"])), L)
            sn = sprinkle(sn, 0.08, r, 1, k)
            ch = tile(grid("x-x-x-x-x-x-x-x-"), L)
            ch = sprinkle(ch, 0.2, r)
            oh = [1 if i % 16 == 14 and r.random() < 0.6 else 0 for i in range(L)]
            tie = [round(r.choice([0.5, 0.5, 0.42, 0.58, 0.67]), 2) if r.random() < 0.15 else 0 for i in range(L)]
            pod = [0.8 if r.random() < 0.08 else 0 for i in range(L)]

        if variant == 3:            # D = fill / break variation
            for i in range(L - 4, L):
                sn[i] = 1 if r.random() < 0.6 else sn[i]
            if genre not in ("chicago",):
                k[L - 2] = 0
        if variant == 1:            # B = variation: shift a few percussion notes
            pod = pod[1:] + pod[:1]
        mod = [round(0.5 + 0.5 * __import__("math").sin(i / L * 6.283 * r.choice([1, 2, 3])) * r.choice([0.4, 0.8, 1.0]), 2) for i in range(L)]
        pats.append({"len": L, "trig": [s(k[:L]), s(sn[:L]), s(ch[:L]), s(oh[:L])],
                     "tie": [round(x, 3) for x in tie[:L]], "pod": [round(x, 3) for x in pod[:L]],
                     "mod": [max(0.0, min(1.0, x)) for x in mod[:L]]})
    return pats

def b_kit(genre, r):
    P = {}
    def v(voice, key, val): P[f"b_{voice}_{key}"] = val
    smp = {
        "Kick": pick("Kick", genre, r), "Snare": pick("Snare", genre, r), "Hat": pick("Hat", genre, r),
        "Tie": pick("Tie", genre, r), "Pod": pick("Pod", genre, r),
    }
    # defaults
    v("kick", "gain", 0); v("snare", "gain", -3); v("hat", "gain", -6); v("tie", "gain", -4); v("pod", "gain", -7); v("pulp", "gain", -9)
    v("kick", "d", 65); v("snare", "d", 50); v("hat", "cd", 35); v("hat", "od", 65); v("tie", "d", 60); v("pod", "d", 55)
    v("tie", "vamt", 60); v("pod", "vamt", 80)
    v("tie", "pan", -15); v("pod", "pan", r.choice([-30, 25, 35])); v("hat", "pan", r.choice([-8, 8, 12]))
    fx = {"b_fx_mix": 50, "b_dly_one": 3, "b_dly_two": 4, "b_dly_fb": 35, "b_dly_gain": -16, "b_x_gain": -12, "b_dly_cut": 5000, "b_dly_wide": 70,
          "b_x_mode": 0, "b_dirt": 0, "b_norm": 0, "b_eq_amp": 0, "b_mag_mix": 0, "b_flt_cut": 20000, "b_swing": 0,
          "b_remix": 0, "b_remix_rate": 0, "b_tie_str": 0, "b_pod_str": 0, "b_mod_str": 0}
    if genre == "techno":
        v("kick", "sat", r.randint(10, 40)); v("kick", "shp", r.randint(0, 40)); v("tie", "vel", 2); v("tie", "d", 45)
        v("tie", "ft", 0); v("tie", "cut", r.choice([300, 450, 600])); v("tie", "res", 30)
        v("hat", "mod", 3); fx.update({"b_dly_one": 3, "b_dly_two": 5, "b_dly_fb": 55, "b_dly_gain": -10, "b_dly_cut": 2500,
                                          "b_x_mode": r.choice([11, 6, 9, 4]), "b_x_x": r.randint(30, 70), "b_x_y": r.randint(30, 70),
                                          "b_x_gain": -9, "b_dirt": r.randint(5, 25), "b_norm": 1, "b_eq_lo": 2, "b_flt_mod": -20})
    elif genre == "minimal":
        v("kick", "d", 45); v("snare", "d", 30); v("tie", "vel", 2); v("tie", "d", 35); v("pod", "vel", 6); v("pod", "cut", 4000)
        fx.update({"b_dly_one": 3, "b_dly_two": 3, "b_dly_fb": 45, "b_dly_gain": -8, "b_x_mode": r.choice([10, 16, 3]),
                   "b_x_x": r.randint(20, 60), "b_x_y": r.randint(20, 60), "b_x_gain": -12, "b_swing": r.randint(5, 20)})
    elif genre == "microhouse":
        v("hat", "mod", 1); v("tie", "vel", 1); v("pod", "vel", 1); v("tie", "d", 30); v("snare", "flam", r.choice([0, 0, 30]))
        fx.update({"b_swing": r.randint(30, 55), "b_x_mode": r.choice([20, 16, 19, 13]), "b_x_x": r.randint(20, 60),
                   "b_x_y": r.randint(40, 80), "b_x_gain": -6, "b_remix": r.randint(1, 127), "b_remix_rate": r.choice([2, 3]),
                   "b_dly_one": 3, "b_dly_two": 2, "b_mag_mix": r.choice([0, 20]), "b_mag_pitch": r.choice([-5, 7, 12])})
    elif genre == "rominimal":
        v("kick", "d", r.randint(40, 60)); v("kick", "shp", r.randint(-20, 10)); v("snare", "d", 25)
        v("tie", "vel", 2); v("tie", "port", r.randint(25, 60)); v("tie", "d", r.randint(30, 55)); v("tie", "ft", 0)
        v("tie", "cut", r.choice([500, 800, 1200])); v("tie", "res", r.randint(10, 40))
        v("pod", "vel", r.choice([1, 2, 6])); v("pod", "d", r.randint(25, 45)); v("pod", "cut", r.choice([3000, 6000, 20000]))
        v("hat", "cd", r.randint(20, 35)); v("hat", "cut", r.choice([20, 3000, 6000]))
        v("pulp", "comb", r.randint(10, 40)); v("pulp", "d", 30)
        fx.update({"b_swing": r.randint(25, 50), "b_tie_str": 0, "b_dly_one": r.choice([3, 5]), "b_dly_two": r.choice([4, 6, 7]),
                   "b_dly_fb": r.randint(40, 65), "b_dly_gain": -9, "b_dly_cut": r.choice([1800, 2500, 3500]), "b_dly_dm": r.randint(5, 30),
                   "b_x_mode": r.choice([11, 10, 5, 8]), "b_x_x": r.randint(25, 70), "b_x_y": r.randint(30, 70), "b_x_gain": -10,
                   "b_flt_trk": r.randint(0, 25), "b_eq_lo": 1.5, "b_eq_hi": -1})
    elif genre == "chicago":
        v("kick", "sat", r.randint(20, 50)); v("snare", "sat", r.randint(10, 40)); v("hat", "acc", 70)
        v("tie", "vel", 2); v("tie", "d", 40); v("pod", "vel", 0)
        fx.update({"b_swing": r.randint(10, 30), "b_dirt": r.randint(15, 40), "b_eq_amp": r.randint(3, 9),
                   "b_x_mode": r.choice([0, 3, 11]), "b_x_x": 55, "b_x_y": 45, "b_x_gain": -10})
    elif genre == "techhouse":
        v("tie", "vel", 2); v("tie", "d", 30); v("tie", "ft", 0); v("tie", "cut", r.choice([700, 1000, 1500])); v("tie", "res", 35)
        v("pod", "vel", 2)
        fx.update({"b_swing": r.randint(15, 35), "b_norm": 1, "b_x_mode": r.choice([3, 4, 18, 11]), "b_x_x": r.randint(30, 70),
                   "b_x_y": r.randint(20, 60), "b_x_gain": -9, "b_dly_one": 3, "b_dly_two": 3, "b_dly_gain": -14})
    elif genre == "deephouse":
        v("tie", "vel", 2); v("tie", "d", 75); v("tie", "a", 5); v("tie", "gain", -6); v("hat", "cd", 30)
        v("pod", "vel", 0); v("kick", "d", 70)
        fx.update({"b_swing": r.randint(20, 45), "b_x_mode": 11, "b_x_x": r.randint(55, 85), "b_x_y": r.randint(40, 70),
                   "b_x_gain": -8, "b_x_dly": 30, "b_dly_one": 3, "b_dly_two": 4, "b_dly_cut": 3000, "b_eq_hi": -2, "b_eq_amp": 3})
    else:  # breakbeat
        v("kick", "sat", r.randint(20, 50)); v("snare", "sat", r.randint(20, 60)); v("snare", "shp", r.randint(0, 50))
        v("tie", "vel", 2); v("tie", "d", 35)
        fx.update({"b_swing": r.randint(0, 15), "b_dirt": r.randint(10, 35), "b_x_mode": r.choice([19, 20, 14, 7]),
                   "b_x_x": r.randint(30, 70), "b_x_y": r.randint(30, 70), "b_x_gain": -8, "b_x_len": r.choice([1, 2, 4]),
                   "b_remix": r.randint(1, 127), "b_remix_rate": r.choice([2, 3])})
    P.update(fx)
    for k in ("b_dly_gain", "b_x_gain"):           # FX returns never above -16 dB by default
        P[k] = min(float(P.get(k, -16)), -16.0)
    P["b_dly_gain"] = -60.0                         # delay return at minimum by default (raise it by hand)
    return P, smp

def b_preset(genre, idx, r):
    lo, hi = BPM[genre]
    length = r.choice([32, 32, 64]) if genre == "rominimal" else (r.choice([16, 16, 32]) if genre in ("minimal", "microhouse", "techno") else 16)
    kit, smp = b_kit(genre, r)
    pats = b_patterns(genre, r, length)
    lanes = {"b_len": length, "b_tie_len": length, "b_pod_len": length, "b_mod_len": length}
    if genre in ("minimal", "rominimal", "microhouse", "techno") and r.random() < 0.6:
        lanes["b_pod_len"] = r.choice([12, 14, 20, 24, 28, 30]) if length >= 32 else r.choice([10, 12, 14])   # polymeter
    if genre == "rominimal" and r.random() < 0.5:
        lanes["b_mod_len"] = r.choice([24, 48, 64])
    kit.update(lanes)
    return {"name": NAMES[genre][idx], "genre": LABEL[genre], "bpm": r.randint(lo, hi), "params": kit, "samples": smp, "patterns": pats}


# ------------------------------------------------------------------------------- Section A (Microtonic kit)
def patch(role, genre, r):
    """Returns a drum patch in natural units for the Microtonic-style voice."""
    j = lambda a, b: round(r.uniform(a, b), 2)
    if role == "kick":
        base = {"techno": (44, 52, 300, 600, 3, 25), "minimal": (50, 60, 150, 300, 0, 8), "microhouse": (55, 70, 70, 180, 0, 12),
                "rominimal": (46, 56, 160, 420, 0, 4), "chicago": (48, 56, 250, 500, 5, 30), "techhouse": (48, 56, 200, 400, 3, 18),
                "deephouse": (45, 54, 300, 650, 0, 8), "breakbeat": (52, 64, 150, 300, 10, 40)}[genre]
        return dict(osc_wave=0, osc_freq=j(base[0], base[1]), osc_atk=0, osc_dcy=j(base[2], base[3]), pm_mode=0,
                    pm_amt=j(18, 40), pm_rate=j(30, 45), n_mode=0, n_freq=j(1500, 4000), n_q=0.9, n_stereo=0, n_env=0,
                    n_atk=0, n_dcy=j(6, 20), mix=j(3, 12), dist=j(base[4], base[5]), eq_freq=j(70, 110), eq_gain=j(0, 4),
                    level=0, pan=0, choke=0, vel_osc=60, vel_noise=60, vel_mod=0)
    if role == "snare":
        clap = genre in ("chicago", "techhouse", "deephouse") or r.random() < 0.3
        rim = genre in ("minimal", "rominimal", "microhouse") and r.random() < 0.6
        if rim:
            return dict(osc_wave=1, osc_freq=j(600, 1400), osc_atk=0, osc_dcy=j(15, 45), pm_mode=0, pm_amt=j(-6, 0), pm_rate=j(5, 15),
                        n_mode=1, n_freq=j(2500, 5000), n_q=j(1, 3), n_stereo=0, n_env=0, n_atk=0, n_dcy=j(10, 30), mix=j(15, 40),
                        dist=j(10, 40), eq_freq=2500, eq_gain=j(0, 5), level=-5, pan=j(-15, 15), choke=0, vel_osc=60, vel_noise=60, vel_mod=0)
        if clap:
            return dict(osc_wave=1, osc_freq=900, osc_atk=0, osc_dcy=10, pm_mode=0, pm_amt=0, pm_rate=10, n_mode=1, n_freq=j(1000, 1600),
                        n_q=j(1.6, 3), n_stereo=1, n_env=2, n_atk=0, n_dcy=j(150, 350), mix=100, dist=j(0, 20), eq_freq=3000,
                        eq_gain=j(0, 3), level=-3, pan=0, choke=0, vel_osc=60, vel_noise=60, vel_mod=0)
        return dict(osc_wave=1, osc_freq=j(170, 230), osc_atk=0, osc_dcy=j(80, 160), pm_mode=0, pm_amt=j(6, 14), pm_rate=j(10, 20),
                    n_mode=r.choice([1, 2]), n_freq=j(2000, 4500), n_q=j(0.8, 1.5), n_stereo=1, n_env=0, n_atk=0, n_dcy=j(120, 260),
                    mix=j(50, 75), dist=j(5, 30), eq_freq=6000, eq_gain=j(0, 5), level=-3, pan=0, choke=0, vel_osc=60, vel_noise=60, vel_mod=0)
    if role in ("ch", "oh"):
        dcy = j(15, 60) if role == "ch" else j(200, 500)
        if genre in ("microhouse", "minimal"): dcy *= 0.6
        return dict(osc_wave=2, osc_freq=j(2500, 6000), osc_atk=0, osc_dcy=j(10, 40), pm_mode=2, pm_amt=j(6, 20), pm_rate=j(60, 85),
                    n_mode=2, n_freq=j(6500, 11000), n_q=j(0.8, 2.0), n_stereo=1, n_env=0, n_atk=0, n_dcy=round(dcy, 1),
                    mix=j(80, 100), dist=0, eq_freq=10000, eq_gain=j(0, 4), level=-6, pan=j(-20, 20), choke=1, vel_osc=60, vel_noise=60, vel_mod=0)
    if role == "perc":
        kind = r.choice(["wood", "conga", "tom", "metal", "click"]) if genre != "rominimal" else r.choice(["wood", "conga", "click", "wood"])
        f = {"wood": (700, 1400), "conga": (220, 420), "tom": (90, 200), "metal": (400, 900), "click": (1500, 3500)}[kind]
        return dict(osc_wave=0 if kind in ("conga", "tom") else r.choice([1, 2]), osc_freq=j(*f), osc_atk=0,
                    osc_dcy=j(30, 90) if kind in ("wood", "click") else j(120, 320), pm_mode=r.choice([0, 0, 1]), pm_amt=j(-3, 14),
                    pm_rate=j(15, 50), n_mode=1, n_freq=j(1500, 6000), n_q=j(1, 4), n_stereo=0, n_env=0, n_atk=0, n_dcy=j(8, 30),
                    mix=j(5, 30), dist=j(0, 15), eq_freq=j(500, 3000), eq_gain=j(0, 6), level=j(-10, -5), pan=j(-45, 45), choke=0,
                    vel_osc=80, vel_noise=60, vel_mod=0)
    if role == "tone":
        return dict(osc_wave=r.choice([0, 1, 2]), osc_freq=j(55, 140) if genre in ("techno", "rominimal", "techhouse") else j(200, 600),
                    osc_atk=j(0, 5), osc_dcy=j(120, 400), pm_mode=r.choice([0, 1]), pm_amt=j(0, 5), pm_rate=j(20, 50), n_mode=0,
                    n_freq=j(400, 1500), n_q=1, n_stereo=0, n_env=0, n_atk=0, n_dcy=10, mix=j(0, 8), dist=j(10, 50),
                    eq_freq=j(150, 800), eq_gain=j(0, 6), level=-7, pan=j(-10, 10), choke=0, vel_osc=60, vel_noise=60, vel_mod=0)
    # fx
    return dict(osc_wave=2, osc_freq=j(300, 2000), osc_atk=0, osc_dcy=j(20, 120), pm_mode=r.choice([1, 2]), pm_amt=j(-24, 24),
                pm_rate=j(40, 90), n_mode=r.choice([1, 2]), n_freq=j(1000, 9000), n_q=j(1, 8), n_stereo=1, n_env=r.choice([0, 1, 2]),
                n_atk=0, n_dcy=j(40, 250), mix=j(40, 90), dist=j(0, 40), eq_freq=j(800, 5000), eq_gain=j(0, 6), level=-9,
                pan=j(-50, 50), choke=0, vel_osc=60, vel_noise=60, vel_mod=0)

ROLES = ["kick", "snare", "ch", "oh", "perc", "perc", "tone", "fx"]
CH_NAMES = {"kick": "Kick", "snare": "Snare", "ch": "CH", "oh": "OH", "perc": "Perc", "tone": "Tone", "fx": "FX"}

def a_pattern(genre, r, L):
    """8 channels x L steps, each step digit: bit0 trig, bit1 accent, bit2 fill."""
    b = b_patterns(genre, r, L)[r.randint(0, 3)]
    tr = lambda st: [int(c) for c in st]
    k, sn, ch, oh = (tr(x) for x in b["trig"])
    perc1 = [1 if x > 0.001 else 0 for x in b["pod"]]
    perc2 = sprinkle([0] * L, 0.1 if genre in ("rominimal", "microhouse") else 0.05, r, 1)
    tone = [1 if x > 0.001 else 0 for x in b["tie"]]
    fx = [1 if (i % 16 == 15 and r.random() < 0.5) else 0 for i in range(L)]
    rows = [k, sn, ch, oh, perc1, perc2, tone, fx]
    out = []
    for ri, row in enumerate(rows):
        steps = []
        for i in range(L):
            v = row[i]
            d = 0
            if v: d |= 1
            if v == 2 or (v and r.random() < 0.12): d |= 2
            if not v and ri in (1, 2, 4, 5) and i % 16 >= 12 and r.random() < 0.25: d |= 4      # fill-only notes at bar end
            steps.append(d)
        out.append("".join(str(x) for x in steps))
    return out

def a_preset(genre, idx, r):
    lo, hi = BPM[genre]
    L = r.choice([32, 32, 64]) if genre == "rominimal" else (r.choice([16, 32]) if genre in ("minimal", "microhouse") else 16)
    kit = [patch(role, genre, r) for role in ROLES]
    swing = {"rominimal": (25, 50), "microhouse": (30, 55), "deephouse": (20, 40), "chicago": (10, 30), "techhouse": (15, 30),
             "minimal": (5, 20), "techno": (0, 8), "breakbeat": (0, 15)}[genre]
    return {"name": NAMES[genre][idx], "genre": LABEL[genre], "bpm": r.randint(lo, hi),
            "names": [CH_NAMES[x] + ("" if x not in ("perc",) else str(1 + ROLES[:i].count("perc"))) for i, x in enumerate(ROLES)],
            "kit": kit, "len": L, "steps": a_pattern(genre, r, L), "swing": r.randint(*swing), "fillrate": r.choice([3, 3, 4, 5])}


def main():
    out = os.path.join(ROOT, "resources")
    A, B = [], []
    for gi, g in enumerate(GENRES):
        for i in range(16):
            A.append(a_preset(g, i, random.Random(1000 + gi * 100 + i)))
            B.append(b_preset(g, i, random.Random(5000 + gi * 100 + i)))
    json.dump({"presets": A}, open(os.path.join(out, "presets_A.json"), "w"), separators=(",", ":"))
    json.dump({"presets": B}, open(os.path.join(out, "presets_B.json"), "w"), separators=(",", ":"))
    print("A:", len(A), "B:", len(B))

if __name__ == "__main__":
    main()
