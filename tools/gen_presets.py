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
  house       120-125  deep/garage house (Adriano, Kerri Chandler, Darryl B & Matt Cole, Enrico Mantini): warm 909, claps, organ/rhodes
  techhouse   124-128  shuffle groove, bass stabs, percussive hooks, clap/snare layers
  deephouse   118-124  swung hats, rims, shakers, Rhodes/organ chords on Tie, warm reverb
  breakbeat   126-136  syncopated kick/snare breaks, ghost snares, electro zaps, chopped fx
"""
import json, os, random, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
manifest = json.load(open(os.path.join(ROOT, "resources", "samples_manifest.json")))
GENRES = ["techno", "minimal", "microhouse", "rominimal", "house", "techhouse", "deephouse", "breakbeat"]
LABEL = {"techno": "Techno", "minimal": "Minimal", "microhouse": "Microhouse", "rominimal": "Rominimal",
         "house": "House", "techhouse": "Tech House", "deephouse": "Deep House", "breakbeat": "Breakbeat"}
BPM = {"techno": (126, 138), "minimal": (122, 128), "microhouse": (120, 126), "rominimal": (118, 130),
       "house": (120, 125), "techhouse": (124, 128), "deephouse": (118, 124), "breakbeat": (126, 136)}

NAMES = {
    "techno": ["Concrete Ritual", "Rotor Seven", "Tunnel Vision", "Static Hymn", "Iron Lung", "Night Shift", "Sodium Glow",
               "Black Box", "Pressure Drop", "Hollow Grid", "Dead Signal", "Machine Psalm", "Kiln", "Low Orbit", "Sub Zero Unit", "Furnace Room"],
    "minimal": ["Dry Ink", "Pin Drop", "White Room", "Sparse Logic", "Thin Line", "Quiet Engine", "Paper Moon", "Negative Space",
                "Blank Page", "Small Talk", "Glass Floor", "Half Light", "Still Water", "Dot Matrix", "Low Ceiling", "Clean Cut"],
    "microhouse": ["Dust Bunny", "Pocket Lint", "Velcro", "Lint Roller", "Cut Glass Pop", "Tiny Tremors", "Breadcrumbs", "Fizz",
                   "Static Kiss", "Crumb Shuffle", "Felt Tip", "Rubber Band", "Lo Res Love", "Mouse Clicks", "Bubble Wrap", "Kitchen Floor"],
    "rominimal": ["Cismigiu Sunrise", "Lipscani Shuffle", "Strada Lunga", "Wooden Horse", "Dacia Groove", "Sapte Ore", "Paduri",
                  "Fluture", "Bucegi Fog", "Leagan", "Vama Veche", "Zmeu", "Carousel Dub", "Ploaie Calda", "Ceai de Tei", "Lume Lunga"],
    "house": ["Basement Soul", "Late Garage", "Organ Donor", "Warm Floor", "Midnight Keys", "Soul Shift", "Sunday Groove",
              "Bassline Kiss", "Deep Inside Out", "Velvet Room", "Garage Steps", "Smoke And Keys", "Night Train House",
              "Soft Pressure", "Golden Hour Jam", "Back To Basics"],
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

def b_patterns(genre, r, length, cx=1):
    """4 patterns (A-D). cx = 0: simple, essential groove; cx = 1: intricate (ghosts, polyrhythm, bar variations)."""
    L = length
    T = lambda st: tile(grid(st), L)
    def ghosts(p, prob, avoid=None):            # add soft notes only in the intricate version
        return sprinkle(p, prob, r, 1, avoid) if cx else p
    def evolve(p, every=2):                     # bar-to-bar variation (intricate)
        if not cx or L <= 16: return p
        out = list(p)
        for bar in range(L // 16):
            if bar % every == every - 1:
                i = bar * 16 + r.choice([13, 14, 15]); out[i] = 0 if out[i] else 1
        return out
    def vals(positions, lo=0.5, hi=0.9, prob=1.0):
        v = [0.0] * L
        for i in range(L):
            if (i % 16) in positions and r.random() < prob: v[i] = round(r.uniform(lo, hi), 2)
        return v
    pats = []
    for variant in range(4):
        if genre == "techno":                    # CVO / Alarico / Danny Wabbit: hypnotic, rumble, rolling percussion
            k = T("X---x---X---x---")
            sn = T(r.choice(["----x-------x---", "----------------"])) if not cx else ghosts(T("----x-------x---"), 0.03, k)
            ch = T("--x---x---x---x-") if not cx else accent_some(T("x-X-x-X-x-X-x-X-"), 0.0, r)
            oh = T("--x---x---x---x-") if (variant % 2 and not cx) else ([0] * L if not cx else evolve(T("------x-------x-")))
            tie = vals({2, 6, 10, 14}) if not cx else vals({2, 3, 6, 7, 10, 11, 14, 15}, 0.3, 0.95, 0.7)       # rumble on the off-beats
            pod = vals({3}, prob=0.8) if not cx else [round(r.uniform(0.3, 0.9), 2) if x else 0.0 for x in tile(euclid(5, 16, r.randint(0, 4)), L)]
        elif genre == "minimal":                 # Jan Jelinek / Farben / Farine: clicks & cuts, space, micro jazz
            k = T("X---x---X---x---") if not cx else T(r.choice(["X-----x-X---x--x", "X---x---X--x-x--", "X---x--xX---x---"]))
            sn = T("----x-------x---") if not cx else ghosts(T("----x-------x--x"), 0.05, k)
            ch = T("--x---x---x---x-") if not cx else thin(tile(euclid(7, 16, r.randint(0, 6)), L), 0.1, r)
            oh = [0] * L
            tie = vals({0, 10}, 0.5, 0.6) if not cx else vals({0, 3, 7, 10, 13}, 0.3, 0.9, 0.8)                # double-bass notes
            pod = vals({7, 15}, 0.3, 0.6, 0.7) if not cx else [round(r.uniform(0.2, 0.8), 2) if x else 0.0 for x in tile(euclid(9, 16, r.randint(0, 8)), L)]
        elif genre == "microhouse":              # Zavor / Villalobos: organic percussion, rolling, vocal snippets
            k = T("X---x---X---x---") if not cx else evolve(T(r.choice(["X---x---X---x---", "X---x--xX---x---", "X---x---X--xx---"])), 2)
            sn = T(r.choice(["----x-------x---", "----------------"])) if not cx else ghosts(T("----x-------x---"), 0.06, k)
            ch = T("x-x-x-x-x-x-x-x-") if not cx else accent_some(ghosts(T("x-xxx-x-x-xx--x-"), 0.1), 0.2, r)
            oh = [0] * L
            tie = vals({2, 10}, 0.5, 0.7) if not cx else vals({2, 5, 7, 10, 13, 15}, 0.3, 0.95, 0.6)
            pod = vals({3, 6, 11, 14}, 0.4, 0.8) if not cx else [round(r.uniform(0.25, 0.9), 2) if x else 0.0 for x in tile(euclid(7, 12, r.randint(0, 5)), L)]
        elif genre == "rominimal":               # Barac / David Nicolas / Sublee / Takashi Himeoka / Arapu
            base_k = grid("X---x---X---x---") if not cx else grid(r.choice(["X---x---X--xx---", "X---x--xX---x---", "X-----x-X---x---"]))
            k = tile(base_k, L) if not cx else vary_bars(base_k, L, r, lambda b, bar: (b[:13] + [0, 1 if r.random() < 0.5 else 0, 0]) if bar % 2 else b)
            sn = [1 if i % 16 in (4, 12) and r.random() < (0.5 if not cx else 0.25) else 0 for i in range(L)]
            ch = T("--x---x---x---x-") if not cx else ghosts(thin(T(r.choice(["--xx--x---x--xx-", "-xx--x-x-xx--x-x"])), 0.1, r), 0.06)
            oh = [0] * L if (not cx or variant < 2) else [1 if i % 16 in (6, 14) and r.random() < 0.5 else 0 for i in range(L)]
            tie = vals({2, 7, 10, 15}, 0.5, 0.8) if not cx else vals({2, 5, 7, 10, 13, 15}, 0.3, 0.95, 0.75)    # rolling bass
            pod = vals({3, 11}, 0.4, 0.8) if not cx else [round(r.uniform(0.3, 0.95), 2) if r.random() < (0.38 if i % 2 else 0.22) else 0.0 for i in range(L)]
        elif genre == "house":                   # Adriano / Kerri Chandler / Darryl B & Matt Cole / Enrico Mantini
            k = T("X---x---X---x---") if not cx else T(r.choice(["X---x---X---x-x-", "X---x---X---x---", "X---x--xX---x---"]))
            sn = T("----X-------X---") if not cx else ghosts(T("----X-------X---"), 0.05, k)
            ch = T("--x---x---x---x-") if not cx else accent_some(T("x-xxx-x-x-xxx-xx"), 0.25, r)
            oh = T("--x---x---x---x-") if not cx else evolve(T("--x---x---x---x-"))
            tie = vals(r.choice([{3, 11}, {0, 6, 10}, {2, 10}]), 0.5, 0.8) if not cx else vals({0, 3, 6, 10, 13}, 0.3, 0.95, 0.7)  # organ / rhodes stabs
            pod = vals({2, 6, 10, 14}, 0.4, 0.7) if not cx else [round(r.uniform(0.3, 0.9), 2) if x else 0.0 for x in tile(euclid(7, 16, r.randint(0, 6)), L)]
        elif genre == "techhouse":               # Adonis / Henriku / Noha / Saunderson-Larkin: raw jack, acid, Detroit
            k = T("X---x---X---x---")
            sn = T("----X-------X---") if not cx else ghosts(T("----X-------X--x"), 0.04, k)
            ch = accent_some(T("xxxxxxxxxxxxxxxx"), 0.0, r) if not cx else [2 if i % 4 == 2 else (1 if v else 0) for i, v in enumerate(thin(T("xxxxxxxxxxxxxxxx"), 0.12, r))]
            oh = T("--x---x---x---x-")
            acid_pos = r.choice([{0, 3, 6, 8, 10, 11, 14}, {0, 2, 3, 6, 8, 11, 12, 14}, {0, 3, 4, 7, 8, 10, 13}])
            tie = vals(acid_pos, 0.6, 0.9) if not cx else vals(acid_pos | {r.randint(0, 15), r.randint(0, 15)}, 0.3, 0.95)     # acid line
            pod = vals({3, 7, 11, 15}, 0.5, 0.8, 0.6) if not cx else [round(r.uniform(0.3, 0.9), 2) if x else 0.0 for x in tile(euclid(5, 16, 2), L)]
        elif genre == "deephouse":               # Mr G / UGLH / Sid Le Rock (DJ Koze) / Edit Murphy: dusty, dub, funky
            k = T("X---x---X---x---") if not cx else T(r.choice(["X---x---X---x--x", "X---x---X-x-x---"]))
            sn = T("----x-------x---") if not cx else ghosts(T("----x-------x---"), 0.05, k)
            ch = T("--x---x---x---x-") if not cx else ghosts(T("--x---x---x---x-"), 0.25)
            oh = [0] * L if not cx else evolve(T("------x-------x-"))
            tie = vals(r.choice([{0, 7}, {3, 11}, {0, 10}]), 0.5, 0.8) if not cx else vals({0, 3, 7, 10, 14}, 0.3, 0.95, 0.7)   # dub chords / funky bass
            pod = vals({5, 13}, 0.3, 0.6, 0.6) if not cx else [round(r.uniform(0.3, 0.7), 2) if i % 2 == 1 and r.random() < 0.35 else 0.0 for i in range(L)]
        else:                                    # breakbeat -> electro: Silicon Scally / Fleisch Fantasy / OzzE
            k = T(r.choice(["X------x--X-----", "X-----x---X-----"])) if not cx else T(r.choice(["X--x----x-X--x--", "X-x-------x-x---", "X------xx-----x-"]))
            sn = T("----X-------X---") if not cx else ghosts(T(r.choice(["----X--x----X--x", "----X-------X-x-"])), 0.06, k)
            ch = T("x-x-x-x-x-x-x-x-") if not cx else accent_some(ghosts(T("x-x-x-x-x-x-x-x-"), 0.25), 0.2, r)
            oh = [0] * L if not cx else [1 if i % 16 == 14 and r.random() < 0.6 else 0 for i in range(L)]
            tie = vals({0, 6, 10}, 0.5, 0.8) if not cx else vals({0, 3, 6, 10, 11, 14}, 0.3, 0.95, 0.8)         # robotic bass
            pod = vals({15}, 0.6, 0.9, 0.7) if not cx else [round(r.uniform(0.4, 0.9), 2) if x else 0.0 for x in tile(euclid(3, 8, 1), L)]

        if variant == 3:            # D = fill / break
            for i in range(L - 4, L):
                if r.random() < (0.5 if cx else 0.3): sn[i] = 1
        if variant == 1 and cx:     # B = variation: percussion shifted
            pod = pod[1:] + pod[:1]
        if variant == 2 and not cx: # C = stripped back
            oh = [0] * L
        import math
        mod = [round(0.5 + (0.5 * math.sin(i / L * 6.283 * r.choice([1, 2, 3])) * r.choice([0.4, 0.8, 1.0]) if cx else 0.0), 2) for i in range(L)]
        pats.append({"len": L, "trig": [s(k[:L]), s(sn[:L]), s(ch[:L]), s(oh[:L])],
                     "tie": [round(x, 3) for x in tie[:L]], "pod": [round(x, 3) for x in pod[:L]],
                     "mod": [max(0.0, min(1.0, x)) for x in mod[:L]]})
    return pats

def b_kit(genre, r, cx=1):
    P = {}
    def v(voice, key, val): P[f"b_{voice}_{key}"] = val
    smp = {m: pick(m, genre, r) for m in ("Kick", "Snare", "Hat", "Tie", "Pod")}
    v("kick", "gain", 0); v("snare", "gain", -3); v("hat", "gain", -6); v("tie", "gain", -4); v("pod", "gain", -7); v("pulp", "gain", -9)
    v("kick", "d", 100); v("snare", "d", 60); v("hat", "cd", 35); v("hat", "od", 65); v("tie", "d", 70); v("pod", "d", 70)
    v("tie", "pan", -12); v("pod", "pan", r.choice([-30, 25, 35])); v("hat", "pan", r.choice([-8, 8, 12]))
    fx = {"b_fx_mix": 50, "b_dly_one": 3, "b_dly_two": 4, "b_dly_fb": 35, "b_dly_gain": -60, "b_x_gain": -16, "b_dly_cut": 4000, "b_dly_wide": 70,
          "b_x_mode": 0, "b_dirt": 0, "b_norm": 0, "b_eq_amp": 0, "b_mag_mix": 0, "b_flt_cut": 20000, "b_swing": 0,
          "b_remix": 0, "b_remix_rate": 0, "b_tie_str": 0, "b_pod_str": 0, "b_mod_str": 0}
    X = lambda modes, x=(30, 70), y=(30, 70): {"b_x_mode": r.choice(modes), "b_x_x": r.randint(*x), "b_x_y": r.randint(*y)}
    if genre == "techno":            # rumble kicks, metallic hats, hypnotic stabs, reverb spaces
        v("kick", "sat", r.randint(0, 20)); v("hat", "mod", 3); v("tie", "ft", 0); v("tie", "cut", r.choice([600, 900, 1400])); v("tie", "res", 25)
        fx.update({"b_dirt": r.randint(4, 14), "b_norm": 1, "b_eq_lo": 1.5, "b_flt_mod": -15 if cx else 0})
        fx.update(X([11, 9, 6], (40, 75), (30, 60)) if cx else X([0, 11], (40, 60), (40, 60)))
    elif genre == "minimal":         # dry clicks, lofi, space
        v("kick", "d", 70); v("snare", "d", 40); v("pod", "cut", 6000); v("tie", "d", 60)
        fx.update({"b_swing": r.randint(5, 18)})
        fx.update(X([16, 10, 3], (20, 60), (20, 60)) if cx else X([0, 0, 16], (20, 40), (20, 40)))
    elif genre == "microhouse":      # organic, rolling, swung, vocal snippets
        v("hat", "mod", 1 if cx else 0); v("tie", "d", 60); v("snare", "flam", r.choice([0, 0, 25]) if cx else 0)
        fx.update({"b_swing": r.randint(30, 55)})
        fx.update(X([20, 13, 16], (20, 60), (40, 80)) if cx else X([0, 11], (40, 60), (40, 60)))
        if cx: fx.update({"b_remix": r.randint(1, 127), "b_remix_rate": r.choice([2, 3]), "b_mag_mix": r.choice([0, 15])})
    elif genre == "rominimal":       # round kicks, rims, rolling bass with glide, swing
        v("kick", "shp", r.randint(-20, 10)); v("snare", "d", 40); v("tie", "port", r.randint(20, 55)); v("tie", "ft", 0)
        v("tie", "cut", r.choice([700, 1000, 1500])); v("tie", "res", r.randint(10, 35)); v("hat", "cd", r.randint(20, 35))
        v("pulp", "comb", r.randint(10, 40)); v("pulp", "d", 30)
        fx.update({"b_swing": r.randint(25, 50), "b_flt_trk": r.randint(0, 20) if cx else 0, "b_eq_lo": 1.5, "b_eq_hi": -1})
        fx.update(X([11, 10, 5, 8], (25, 70), (30, 70)) if cx else X([0, 11], (30, 50), (40, 60)))
    elif genre == "house":           # warm 909, claps, organ / rhodes, garage swing
        v("kick", "sat", r.randint(5, 20)); v("snare", "sat", r.randint(0, 15)); v("hat", "acc", 60)
        fx.update({"b_swing": r.randint(15, 35), "b_dirt": r.randint(3, 10), "b_eq_amp": r.randint(2, 5)})
        fx.update(X([11, 4], (40, 70), (40, 60)) if cx else X([0, 0, 11], (40, 60), (40, 60)))
    elif genre == "techhouse":       # raw Chicago / Detroit: acid basslines, 707 / 808, jacking hats
        v("tie", "ft", 0); v("tie", "cut", r.choice([900, 1400, 2200])); v("tie", "res", r.randint(30, 55)); v("tie", "d", 55)
        fx.update({"b_swing": r.randint(10, 30), "b_norm": 1, "b_dirt": r.randint(3, 12)})
        fx.update(X([4, 18, 3, 11], (30, 70), (20, 60)) if cx else X([0, 3], (50, 70), (20, 40)))
    elif genre == "deephouse":       # dusty, dub chords, funky bass, Koze-style quirks
        v("tie", "d", 80); v("tie", "a", 5); v("tie", "gain", -6); v("hat", "cd", 30)
        fx.update({"b_swing": r.randint(20, 45), "b_eq_hi": -2, "b_eq_amp": 3, "b_dirt": r.randint(5, 15)})
        fx.update(dict(X([11], (55, 85), (40, 70)), b_x_dly=30) if cx else X([0, 11], (50, 70), (40, 60)))
    else:                            # breakbeat = electro: 808 booms, snappy snares, zaps, robotic bass
        v("kick", "sat", r.randint(5, 25)); v("snare", "sat", r.randint(10, 30))
        fx.update({"b_swing": r.randint(0, 10), "b_dirt": r.randint(4, 14)})
        fx.update(dict(X([19, 20, 14, 7], (30, 70), (30, 70)), b_x_len=r.choice([1, 2, 4])) if cx else X([0, 0, 14], (20, 40), (20, 40)))
        if cx: fx.update({"b_remix": r.randint(1, 127), "b_remix_rate": r.choice([2, 3])})
    P.update(fx)
    P["b_x_gain"] = min(float(P.get("b_x_gain", -16)), -16.0)   # FX returns never above -16 dB by default
    P["b_dly_gain"] = -60.0                                      # delay return at minimum (raise it by hand)
    return P, smp

def lanes_to_samples(preset, genre, r, cx=1):
    """Tie / Pod lanes: the column height selects the sample of the module list (notch 1 = bottom = last sample).
    Every note points to the kit's sample, sometimes alternated with 1-2 other samples of the same genre."""
    for module, lane in (("Tie", "tie"), ("Pod", "pod")):
        files = sorted(it["file"][:-4] for it in manifest["modules"][module])
        n = len(files)
        main = preset["samples"][module]
        alts = [f for f in files if f != main and any(genre in it["tags"] for it in manifest["modules"][module] if it["file"][:-4] == f)]
        r.shuffle(alts)
        choices = [main] + (alts[:2] if cx else [])
        for pat in preset["patterns"]:
            out = []
            for v in pat[lane]:
                if v <= 0.001:
                    out.append(0.0); continue
                # louder / higher original steps keep the main sample, the others alternate
                name = main if (not cx or v >= 0.45 or r.random() < 0.6) else r.choice(choices)
                i = files.index(name)
                out.append(round((n - i) / n, 4))
            pat[lane] = out
        preset["params"]["b_%s_vel" % lane] = 1          # VEL = wav: the lane selects the samples
    return preset

def b_preset(genre, idx, r):
    lo, hi = BPM[genre]
    cx = 0 if idx < 8 else 1                       # 8 simple + 8 intricate presets per genre
    if cx: length = r.choice([32, 64]) if genre in ("rominimal", "microhouse") else r.choice([16, 32, 32])
    else:  length = 32 if genre == "rominimal" and r.random() < 0.5 else 16
    kit, smp = b_kit(genre, r, cx)
    pats = b_patterns(genre, r, length, cx)
    lanes = {"b_len": length, "b_tie_len": length, "b_pod_len": length, "b_mod_len": length}
    if cx and genre in ("minimal", "rominimal", "microhouse", "techno", "deephouse") and r.random() < 0.7:
        lanes["b_pod_len"] = r.choice([12, 14, 20, 24, 28, 30]) if length >= 32 else r.choice([10, 12, 14])   # polymeter
    if cx and genre in ("rominimal", "microhouse") and r.random() < 0.5:
        lanes["b_mod_len"] = r.choice([24, 48, 64])
    kit.update(lanes)
    preset = {"name": NAMES[genre][idx], "genre": LABEL[genre], "level": "Complex" if cx else "Simple",
              "bpm": r.randint(lo, hi), "params": kit, "samples": smp, "patterns": pats}
    return lanes_to_samples(preset, genre, r, cx)


# ------------------------------------------------------------------------------- Section A (Microtonic kit)
def patch(role, genre, r):
    """Returns a drum patch in natural units for the Microtonic-style voice."""
    j = lambda a, b: round(r.uniform(a, b), 2)
    if role == "kick":
        # (freq lo, hi, decay lo, hi, distort lo, hi) - tuned on the reference styles
        base = {"techno": (44, 50, 380, 700, 8, 30),      # long rumble-ready kicks (CVO, Alarico)
                "minimal": (58, 72, 60, 170, 0, 6),       # tiny dry clicks (Jelinek, Farben)
                "microhouse": (48, 60, 200, 450, 0, 8),   # soft round kicks (Villalobos, Zavor)
                "rominimal": (48, 56, 160, 380, 0, 4),    # round, short-medium (Barac, Sublee)
                "house": (48, 54, 280, 520, 3, 15),       # warm 909 (Kerri Chandler)
                "techhouse": (46, 56, 180, 450, 5, 25),   # raw 707/808/909 (Adonis, Saunderson)
                "deephouse": (46, 54, 300, 600, 5, 18),   # dusty, saturated (Mr G, UGLH)
                "breakbeat": (42, 50, 500, 1100, 2, 15)}[genre]   # electro 808 booms (Silicon Scally)
        return dict(osc_wave=0, osc_freq=j(base[0], base[1]), osc_atk=0, osc_dcy=j(base[2], base[3]), pm_mode=0,
                    pm_amt=j(18, 40), pm_rate=j(30, 45), n_mode=0, n_freq=j(1500, 4000), n_q=0.9, n_stereo=0, n_env=0,
                    n_atk=0, n_dcy=j(6, 20), mix=j(3, 12), dist=j(base[4], base[5]), eq_freq=j(70, 110), eq_gain=j(0, 3),
                    level=-2.5, pan=0, choke=0, vel_osc=60, vel_noise=60, vel_mod=0)
    if role == "snare":
        clap = genre in ("house", "techhouse", "deephouse") or r.random() < 0.3
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

def a_pattern(genre, r, L, cx=1):
    """8 channels x L steps, each step digit: bit0 trig, bit1 accent, bit2 fill."""
    b = b_patterns(genre, r, L, cx)[0 if not cx else r.randint(0, 3)]
    tr = lambda st: [int(c) for c in st]
    k, sn, ch, oh = (tr(x) for x in b["trig"])
    perc1 = [1 if x > 0.001 else 0 for x in b["pod"]]
    perc2 = sprinkle([0] * L, (0.1 if genre in ("rominimal", "microhouse") else 0.05) if cx else 0.0, r, 1)
    tone = [1 if x > 0.001 else 0 for x in b["tie"]]
    fx = [1 if (cx and i % 16 == 15 and r.random() < 0.5) else 0 for i in range(L)]
    rows = [k, sn, ch, oh, perc1, perc2, tone, fx]
    out = []
    for ri, row in enumerate(rows):
        steps = []
        for i in range(L):
            v = row[i]
            d = 0
            if v: d |= 1
            if v == 2 or (cx and v and r.random() < 0.12): d |= 2
            if cx and not v and ri in (1, 2, 4, 5) and i % 16 >= 12 and r.random() < 0.25: d |= 4      # fill-only notes at bar end
            steps.append(d)
        out.append("".join(str(x) for x in steps))
    return out

def a_preset(genre, idx, r):
    lo, hi = BPM[genre]
    cx = 0 if idx < 8 else 1                       # 8 simple + 8 intricate presets per genre
    if cx: L = r.choice([32, 64]) if genre in ("rominimal", "microhouse") else r.choice([16, 32])
    else:  L = 32 if genre == "rominimal" and r.random() < 0.5 else 16
    kit = [patch(role, genre, r) for role in ROLES]
    swing = {"rominimal": (25, 50), "microhouse": (30, 55), "deephouse": (20, 40), "house": (15, 35), "techhouse": (15, 30),
             "minimal": (5, 20), "techno": (0, 8), "breakbeat": (0, 15)}[genre]
    return {"name": NAMES[genre][idx], "genre": LABEL[genre], "level": "Complex" if cx else "Simple", "bpm": r.randint(lo, hi),
            "names": [CH_NAMES[x] + ("" if x not in ("perc",) else str(1 + ROLES[:i].count("perc"))) for i, x in enumerate(ROLES)],
            "kit": kit, "len": L, "steps": a_pattern(genre, r, L, cx), "swing": r.randint(*swing) if cx else r.randint(swing[0], (swing[0] + swing[1]) // 2), "fillrate": r.choice([3, 3, 4, 5])}


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
