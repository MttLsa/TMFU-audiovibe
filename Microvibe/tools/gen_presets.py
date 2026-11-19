#!/usr/bin/env python3
"""
Microvibe factory library: 10 categories x 15 presets -> resources/presets.json

Every preset starts from the plug-in defaults (all FX switched off, glitch on lane 1) and sets only what it needs.
Matrix destinations are written as "@<parameter id>" and resolved by the plug-in.
Trims (g_trim) come from tools/levels.json (MicrovibeTest levels), so every preset plays at a similar loudness.

Style notes
  microhouse glitch : swung 1/16 grid, short repeats and slice shuffles, vinyl dust, small dub delays
  minimal perc      : modal wood / glass / skin clicks on Euclidean lanes, odd lane lengths (polymeter)
  textures & beds   : granular clouds of the input, tuned noise beds, drones, long reverbs
  tape & vinyl      : hysteresis tape, worn records, warps, spin-downs driven by LFOs
  wobble / lo-fi    : RC-20 style wow/flutter, crushers, filters
  dub delays        : ping-pong / multitap / tape delays with pitch in the feedback, freezes
  shepard risers    : endless generators and endless filters / phasers / flangers
  microtonal        : scales, quarter tones, just intonation, resonators and percussion in tune
  polyrhythm        : tuplet lanes (3:2, 5:4, 7:8 ...), conditions, ratchets
  performance       : macros wired to many parameters, density, MIDI note triggers
"""
import json, os, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEVELS = os.path.join(ROOT, "tools", "levels.json")
levels = json.load(open(LEVELS)) if os.path.exists(LEVELS) else {}

CATS = ["Microhouse Glitch", "Minimal Perc", "Textures & Beds", "Tape & Vinyl", "Wobble / Lo-fi",
        "Dub Delays", "Shepard Risers", "Microtonal", "Polyrhythm", "Performance"]

FX_ON = {"Tape": "tp_on", "Vinyl": "vn_on", "Wobble": "wb_on", "Delay": "dl_on", "Reverb": "rv_on", "Filter": "ft_on",
         "Resonator": "rs_on", "Ring/Shift": "rf_on", "Crusher": "cr_on", "Chorus": "ch_on", "Phaser": "ph_on",
         "Comp": "cp_on", "Stereo": "st_on", "Shepard FX": "sf_on"}


class P:
    def __init__(self, name, cat, notes=""):
        self.name, self.cat, self.notes = name, cat, notes
        self.p = {}
        self.slot = 0
        self.lanes = None
        self.lfo = None
        self.chain([])

    def set(self, **kw):
        for k, v in kw.items():
            self.p[k] = v
        return self

    def chain(self, mods):
        """FX chain in order; switches the modules on."""
        for i in range(8):
            self.p["fx_slot%d" % (i + 1)] = mods[i] if i < len(mods) else "Off"
        for m in mods:
            self.p[FX_ON[m]] = 1
        return self

    def mod(self, src, dst, amt, via=None, curve=None):
        k = self.slot + 1
        self.slot += 1
        assert self.slot <= 32, self.name
        self.p["mx%d_src" % k] = src
        self.p["mx%d_dst" % k] = "@" + dst
        self.p["mx%d_amt" % k] = round(amt, 3)
        if via: self.p["mx%d_via" % k] = via
        if curve: self.p["mx%d_curve" % k] = curve
        return self

    def lfo_(self, k, shape="Sine", div=None, rate=None, **kw):
        self.p["lf%d_shape" % k] = shape
        if div is not None: self.p["lf%d_sync" % k] = 1; self.p["lf%d_div" % k] = div
        if rate is not None: self.p["lf%d_sync" % k] = 0; self.p["lf%d_rate" % k] = rate
        for a, v in kw.items(): self.p["lf%d_%s" % (k, a)] = v
        return self

    def lane(self, l, steps=16, pulses=4, rot=0, div="1/16", ratio="1:1", **kw):
        pre = "ln%d_" % l
        self.p.update({pre + "on": 1, pre + "steps": steps, pre + "pulses": pulses, pre + "rot": rot, pre + "div": div, pre + "ratio": ratio})
        for a, v in kw.items(): self.p[pre + a] = v
        return self

    def perc(self, v, lane, model="Wood", exc="Click", tune=0, decay=200, **kw):
        pre = "pc%d_" % v
        self.p.update({"pc_on": 1, pre + "lane": "Lane %d" % lane, pre + "model": model, pre + "exc": exc, pre + "tune": tune, pre + "decay": decay})
        for a, v2 in kw.items(): self.p[pre + a] = v2
        return self

    def glitch(self, lane=1, prob=0.35, **w):
        self.p.update({"gl_on": 1, "gl_lane": "Lane %d" % lane if lane else "MIDI only", "gl_prob": prob})
        names = ["repeat", "reverse", "slice", "stop", "gate", "freeze", "crush", "drop", "pitch"]
        if w:
            for i, n in enumerate(names):
                self.p["gl_w%d" % i] = w.get(n, 0.0)
        return self

    def no_glitch(self):
        self.p["gl_on"] = 0
        return self

    def out(self):
        d = {"name": self.name, "cat": self.cat, "notes": self.notes, "p": dict(self.p)}
        d["p"]["g_trim"] = levels.get(self.name, 0.0)
        if self.lanes: d["lanes"] = self.lanes
        if self.lfo: d["lfo"] = self.lfo
        return d


R = random.Random(20261119)
presets = []


def add(p):
    assert p.name not in [x.name for x in presets], p.name
    presets.append(p)
    return p


def macros(p, m1=None, m2=None, m3=None, m4=None):
    """Default macro wiring: 1 density/glitch, 2 space, 3 dirt, 4 tone."""
    p.mod("Macro 1", "g_density", 0.5).mod("Macro 1", "gl_prob", 0.45)
    p.mod("Macro 2", "dl_mix", 0.4).mod("Macro 2", "rv_mix", 0.35)
    p.mod("Macro 3", "wb_amt", 0.5).mod("Macro 3", "vn_dust", 0.5)
    p.mod("Macro 4", "ft_cut", -0.45)
    for k, v in ((1, m1), (2, m2), (3, m3), (4, m4)):
        if v is not None: p.set(**{"m_%d" % k: v})
    return p


# =============================================================================================== 1 Microhouse Glitch
mh = [
    ("Akufen Cutups", dict(rep=1, sl=0.9, rev=0.3), "1/32", 0.45, 16, 7),
    ("Clicks and Cuts", dict(rep=0.6, drop=0.6, gate=0.4), "1/64", 0.4, 16, 5),
    ("Montreal Stutter", dict(rep=1, pitch=0.3), "1/16", 0.35, 16, 6),
    ("Swung Shards", dict(sl=1, rev=0.5), "1/16", 0.4, 12, 5),
    ("Vocal Blips", dict(rep=0.8, freeze=0.4, pitch=0.4), "1/32", 0.5, 16, 9),
    ("Micro Rolls", dict(rep=1), "1/64", 0.3, 16, 4),
    ("Skippy CD", dict(drop=1, rep=0.4), "1/16", 0.45, 16, 7),
    ("Reverse Sparks", dict(rev=1, sl=0.3), "1/8", 0.35, 16, 5),
    ("Tape Stop Fills", dict(stop=1, rep=0.3), "1/16", 0.25, 16, 3),
    ("Crushed Edits", dict(crush=0.8, rep=0.6, gate=0.3), "1/32", 0.4, 16, 6),
    ("Shuffle House", dict(sl=1, rep=0.4), "1/16", 0.5, 16, 8),
    ("Granular Hiccups", dict(freeze=1, rep=0.5), "1/32", 0.4, 16, 5),
    ("Gate Chatter", dict(gate=1, rep=0.3), "1/32", 0.45, 16, 7),
    ("Pitch Splinters", dict(pitch=1, rep=0.5), "1/16", 0.35, 16, 5),
    ("Glitch Salad", dict(rep=0.6, rev=0.5, sl=0.6, stop=0.2, gate=0.4, freeze=0.3, crush=0.2, drop=0.3, pitch=0.3), "1/32", 0.5, 16, 7),
]
key = {"rep": "repeat", "rev": "reverse", "sl": "slice", "stop": "stop", "gate": "gate", "freeze": "freeze", "crush": "crush", "drop": "drop", "pitch": "pitch"}
for i, (n, w, rdiv, prob, steps, pulses) in enumerate(mh):
    p = add(P(n, CATS[0], "Euclidean lane 1 fires glitch events on the input"))
    p.glitch(1, prob, **{key[k]: v for k, v in w.items()})
    p.lane(1, steps, pulses, rot=i % 4, swing=0.12 + 0.04 * (i % 3))
    p.set(gl_rptdiv=rdiv, gl_len=1 + i % 3, gl_rptdecay=0.15 + 0.05 * (i % 4), gl_slices=8, gl_pitch=[7, 12, -5, 5][i % 4])
    p.chain(["Wobble", "Tape", "Vinyl", "Delay", "Reverb"] if i % 2 == 0 else ["Filter", "Tape", "Delay", "Reverb", "Vinyl"])
    p.set(wb_amt=0.12 + 0.03 * (i % 4), vn_dust=0.25, vn_crackle=0.2, vn_rumble=0.1, vn_year=1985, vn_wear=0.15,
          tp_drive=0.35, tp_hiss=0.08, dl_mode="Ping-Pong", dl_div=["1/8D", "1/16D", "1/8T", "1/4T"][i % 4], dl_mix=0.18, dl_fb=0.4,
          rv_mix=0.14, rv_decay=1.6, rv_size=0.45, ft_type="HP", ft_cut=90)
    p.lfo_(1, "S&H", div="1/16").mod("LFO 1", "gl_rptpitch", 0.15 if w.get("pitch") else 0.0)
    p.lfo_(2, "Sine", div="2 bar").mod("LFO 2", "dl_lp", -0.3).mod("LFO 2", "gl_prob", 0.25)
    p.set(rn1_mode="S&H", rn1_div="1/8").mod("Random 1", "ln1_rot", 0.2 if i % 3 == 0 else 0.0)
    macros(p)
    if p.p["mx1_amt"] == 0: pass

# =============================================================================================== 2 Minimal Perc
kits = [
    ("Wood Clicks", [("Wood", "Click", 12, 90), ("Wood", "Click", 19, 60), ("Skin", "Mallet", -12, 220)]),
    ("Glass Drops", [("Glass", "Mallet", 24, 600), ("Glass", "Click", 31, 400), ("Wood", "Click", 12, 70)]),
    ("Bottle Tocks", [("Tube", "Mallet", 12, 250), ("Tube", "Click", 19, 180), ("Stone", "Click", 24, 60)]),
    ("Rubber Congas", [("Skin", "Mallet", -5, 260), ("Skin", "Mallet", 2, 200), ("Plastic", "Click", 14, 80)]),
    ("Metal Blips", [("Metal", "Click", 24, 300), ("Metal", "Noise", 31, 160), ("Wood", "Click", 7, 70)]),
    ("Stone Garden", [("Stone", "Click", 12, 80), ("Stone", "Mallet", 5, 120), ("Wood", "Click", 19, 50)]),
    ("Plastic Rims", [("Plastic", "Click", 17, 90), ("Plastic", "Noise", 24, 70), ("Skin", "Mallet", -7, 180)]),
    ("Sand Shakers", [("Stone", "Noise", 36, 40), ("Plastic", "Noise", 31, 50), ("Wood", "Click", 12, 60)]),
    ("Kalimba Ghosts", [("Metal", "Mallet", 12, 700), ("Glass", "Mallet", 19, 500), ("Wood", "Click", 24, 50)]),
    ("Tabla Ticks", [("Skin", "Click", 7, 320), ("Skin", "Mallet", 0, 260), ("Wood", "Click", 19, 60)]),
    ("Input Snaps", [("Wood", "Input", 12, 150), ("Glass", "Input", 24, 300), ("Skin", "Input", 0, 200)]),
    ("Bamboo Rain", [("Tube", "Click", 24, 120), ("Wood", "Click", 31, 80), ("Tube", "Mallet", 17, 140)]),
    ("Log Drum Bounce", [("Wood", "Mallet", 0, 350), ("Wood", "Mallet", 7, 300), ("Wood", "Click", 19, 80)]),
    ("Bell Dust", [("Metal", "Mallet", 31, 900), ("Glass", "Click", 36, 300), ("Stone", "Click", 24, 40)]),
    ("Organic Kit", [("Wood", "Click", 12, 90), ("Skin", "Mallet", -12, 260), ("Glass", "Click", 24, 300)]),
]
for i, (n, voices) in enumerate(kits):
    p = add(P(n, CATS[1], "modal percussion on Euclidean lanes; plays on its own, the input passes through"))
    p.lane(1, 16, [5, 7, 3, 6, 9][i % 5], rot=i % 3, swing=0.15)
    p.lane(2, [12, 13, 7, 10, 14][i % 5], [5, 4, 3, 4, 5][i % 5], rot=1, swing=0.1)
    p.lane(3, [7, 5, 11, 9, 6][i % 5], [3, 2, 4, 2, 3][i % 5], div="1/16", hum=0.2)
    p.lane(4, 32, 3, div="1/16")
    for v, (model, exc, tune, dec) in enumerate(voices):
        p.perc(v + 1, v + 1, model, exc, tune, dec, mat=0.35 + 0.1 * v, inh=0.15 + 0.1 * (i % 3), pos=0.25 + 0.15 * v,
               level=[0.8, 0.6, 0.7][v], pan=[-0.3, 0.35, 0.0][v], hum=0.25, prob=[1, 0.8, 0.9][v])
    p.set(pc_level=0.8, in_level=0.8)
    p.glitch(4, 0.25, repeat=1, reverse=0.4, slice=0.3)
    p.chain(["Tape", "Delay", "Reverb", "Vinyl"])
    p.set(tp_drive=0.3, tp_hiss=0.05, dl_mode="Ping-Pong", dl_div="1/8D", dl_mix=0.15, dl_fb=0.35, dl_lp=4500,
          rv_mix=0.18, rv_decay=1.2, rv_size=0.35, vn_dust=0.15, vn_crackle=0.1, vn_rumble=0.05, vn_wear=0.1, vn_year=1995)
    p.set(rn1_mode="Smooth", rn1_div="1/4").mod("Random 1", "pc1_tune", 0.02).mod("Random 1", "pc2_decay", 0.2)
    p.lfo_(1, "Triangle", div="4 bar").mod("LFO 1", "pc1_mat", 0.3).mod("LFO 1", "pc3_pos", 0.4)
    p.lfo_(2, "S&H", div="1/16").mod("LFO 2", "pc2_pan", 0.4)
    macros(p)

# =============================================================================================== 3 Textures & Beds
tex = [
    ("Fog Bank", "Pink", 0.7, 0.5, 0.2, "Minor", "Input+Bed", 12, 160, 0.5, 0, 0, False),
    ("Granular Room", "Brown", 0.2, 0.3, 0.1, "Fifth", "Input", 30, 90, 0.6, 0, 3, False),
    ("Frozen Moment", "Pink", 0.3, 0.6, 0.2, "Sus4", "Input", 25, 200, 0.4, 0, 0, True),
    ("Resonant Hiss", "White", 0.9, 0.85, 0.0, "Minor 7", "Bed", 10, 120, 0.3, 0, 0, False),
    ("Drone Floor", "Pink", 0.3, 0.6, 0.6, "Octaves", "Bed", 6, 300, 0.3, 0, 0, False),
    ("Crackle Choir", "Crackle", 0.8, 0.9, 0.2, "Major", "Bed", 15, 120, 0.4, 12, 0, False),
    ("Octave Clouds", "Pink", 0.3, 0.5, 0.2, "Octaves", "Input+Bed", 40, 70, 0.7, 12, 0, False),
    ("Low Rumble Bed", "Brown", 0.8, 0.6, 0.3, "Fifth", "Bed", 8, 250, 0.3, -12, 0, False),
    ("Shimmer Dust", "White", 0.4, 0.7, 0.1, "Major", "Input", 60, 40, 0.8, 12, 5, False),
    ("Reverse Haze", "Pink", 0.4, 0.5, 0.2, "Minor", "Input+Bed", 20, 180, 0.6, 0, 2, False),
    ("Spectral Rain", "White", 0.6, 0.75, 0.0, "Scale", "Input+Bed", 90, 25, 0.9, 0, 7, False),
    ("Night Bus", "Brown", 0.5, 0.4, 0.4, "Minor 7", "Input+Bed", 15, 140, 0.5, -5, 0, False),
    ("Ice Field", "Pink", 0.4, 0.95, 0.1, "Sus4", "Bed", 12, 220, 0.5, 7, 0, False),
    ("Breathing Wall", "Pink", 0.7, 0.4, 0.3, "Minor", "Input+Bed", 18, 160, 0.5, 0, 0, False),
    ("Ghost Grains", "Pink", 0.1, 0.3, 0.0, "Unison", "Input", 35, 60, 0.9, 0, 12, False),
]
for i, (n, noise, bed, q, drone, chord, gsrc, dens, size, spray, pitch, prand, freeze) in enumerate(tex):
    p = add(P(n, CATS[2], "texture generator under the input; modulated grains and tuned noise"))
    p.set(tx_on=1, tx_level=0.75, tx_noise=noise, tx_bed=bed, tx_resq=q, tx_drone=drone, tx_chord=chord, tx_gsrc=gsrc,
          tx_dens=dens, tx_size=size, tx_spray=spray, tx_pitch=pitch, tx_prand=prand, tx_freeze=1 if freeze else 0,
          tx_grain=0.55, tx_spread=0.8, tx_rev=0.5 if "Reverse" in n else 0.15, tx_drift=0.4, tx_oct=0 if pitch >= 0 else -1,
          tu_scale="Minor" if chord in ("Minor", "Minor 7") else "Chromatic 12-TET", tu_root=["C", "D", "F", "A"][i % 4])
    p.no_glitch() if i % 3 else p.glitch(3, 0.2, freeze=1, reverse=0.5)
    p.lane(3, 16, 2, div="1/8")
    p.chain(["Chorus", "Delay", "Reverb", "Stereo"])
    p.set(ch_mix=0.4, dl_mode="Tape", dl_div="1/4D", dl_mix=0.2, dl_fb=0.55, dl_lp=3000, rv_mix=0.45, rv_decay=6 + i % 5,
          rv_size=0.85, rv_shim=0.25 if "Shimmer" in n or "Ice" in n else 0.0, st_width=1.4, in_level=0.9)
    p.lfo_(1, "Smooth Random", div="2 bar").mod("LFO 1", "tx_spray", 0.3).mod("LFO 1", "tx_pitch", 0.04 if prand else 0.0)
    p.lfo_(2, "Sine", div="8 bar").mod("LFO 2", "tx_resq", 0.2).mod("LFO 2", "tx_tilt", 0.4)
    p.set(rn1_mode="Drift", rn1_sync=0, rn1_rate=0.2).mod("Random 1", "tx_dens", 0.25).mod("Random 1", "tx_size", 0.2)
    p.mod("Follower 1", "tx_grain", -0.3)
    macros(p)

# =============================================================================================== 4 Tape & Vinyl
tv = [
    ("Dusty Shellac", 1932, 0.5, 0.6, 0.3, 0.2, 0.0, "78"),
    ("Basement 45", 1968, 0.35, 0.4, 0.15, 0.25, 0.1, "45"),
    ("Worn Promo 12in", 1992, 0.25, 0.3, 0.05, 0.4, 0.05, "33"),
    ("Spin Down Bar", 1985, 0.2, 0.2, 0.0, 0.2, 0.1, "33"),
    ("Cassette Deck", 1990, 0.0, 0.0, 0.0, 0.0, 0.0, "33"),
    ("Hot Reel", 1975, 0.0, 0.0, 0.0, 0.0, 0.0, "33"),
    ("Underbiased", 1979, 0.0, 0.0, 0.0, 0.0, 0.0, "33"),
    ("Warped Pressing", 1981, 0.3, 0.3, 0.05, 0.3, 0.6, "33"),
    ("Thrift Store Find", 1964, 0.6, 0.6, 0.2, 0.5, 0.2, "45"),
    ("Dropout Tape", 1988, 0.0, 0.0, 0.0, 0.0, 0.0, "33"),
    ("Mono Archive", 1950, 0.4, 0.5, 0.1, 0.35, 0.1, "78"),
    ("Brake Fills", 1998, 0.15, 0.15, 0.0, 0.1, 0.05, "33"),
    ("Clean Vinyl Warmth", 2005, 0.08, 0.05, 0.0, 0.05, 0.02, "33"),
    ("Tape + Vinyl Stack", 1979, 0.3, 0.3, 0.05, 0.3, 0.15, "33"),
    ("Half Speed Dub", 1977, 0.2, 0.2, 0.05, 0.2, 0.1, "33"),
]
for i, (n, year, dust, crack, scratch, wear, warp, rpm) in enumerate(tv):
    p = add(P(n, CATS[3], "tape machine and vinyl simulator"))
    vin = dust > 0
    p.chain(["Tape", "Vinyl", "Wobble", "Reverb"] if vin else ["Tape", "Wobble", "Delay", "Reverb"])
    p.set(tp_drive=[0.4, 0.75, 0.5][i % 3], tp_sat=0.6, tp_bias=0.25 if n == "Underbiased" else 0.65, tp_speed=["7.5 ips", "15 ips", "30 ips"][i % 3],
          tp_hiss=0.2 if not vin else 0.08, tp_comp=0.4, tp_drop=0.6 if "Dropout" in n else 0.03, tp_flut=0.25, tp_age=0.3 + 0.1 * (i % 3))
    if n == "Hot Reel": p.set(tp_drive=0.9, tp_sat=0.8)
    if n == "Cassette Deck": p.set(tp_speed="7.5 ips", tp_age=0.6, tp_hiss=0.3, tp_flut=0.45)
    p.set(vn_year=year, vn_dust=dust, vn_crackle=crack, vn_scratch=scratch, vn_wear=wear, vn_warp=warp, vn_rpm=rpm, vn_rumble=0.2,
          vn_hum=0.2 if year < 1960 else 0.0, vn_mono=0.6)
    p.set(wb_amt=0.15, wb_bal=0.4, dl_mode="Tape", dl_div="1/4D", dl_mix=0.15, rv_mix=0.12, rv_decay=1.5)
    p.no_glitch()
    if n == "Spin Down Bar":
        p.lfo_(1, "Saw Up", div="2 bar", uni=1).mod("LFO 1", "vn_brake", 0.9, curve="Exp")
    if n == "Brake Fills":
        p.lane(2, 16, 1, rot=12, div="1/4").set(en1_trig="Lane 2", en1_a=400, en1_h=200, en1_d=600, en1_s=0, en1_r=300)
        p.mod("Env 1", "vn_brake", 0.85)
    if n == "Half Speed Dub":
        p.set(gl_on=1, gl_lane="Lane 4", gl_prob=0.25).glitch(4, 0.25, stop=1).lane(4, 16, 1, rot=14, div="1/8")
    p.lfo_(2, "Smooth Random", div="1 bar").mod("LFO 2", "wb_amt", 0.2)
    macros(p)

# =============================================================================================== 5 Wobble / Lo-fi
wl = [
    ("Wow Only", 0.5, 0.0, 0.6, 12, 0.2), ("Flutter Only", 0.4, 1.0, 1.0, 14, 0.1), ("Seasick Keys", 0.75, 0.2, 0.4, 10, 0.6),
    ("Gentle Wobble", 0.25, 0.3, 0.8, 11, 0.2), ("Broken Walkman", 0.6, 0.6, 1.5, 16, 0.8), ("Lo-fi Rhodes", 0.35, 0.35, 0.7, 9, 0.3),
    ("Bitcrush Wobble", 0.3, 0.4, 0.9, 12, 0.3), ("Phased Memory", 0.3, 0.2, 0.5, 8, 0.2), ("Telephone Glitch", 0.2, 0.7, 1.2, 15, 0.4),
    ("Melted Loop", 0.9, 0.1, 0.3, 7, 0.5), ("Chorus Haze", 0.25, 0.3, 0.6, 10, 0.3), ("8 Bit Shuffle", 0.2, 0.5, 1.0, 12, 0.2),
    ("Formant Wobble", 0.35, 0.3, 0.8, 10, 0.3), ("Comb Drift", 0.3, 0.3, 0.6, 11, 0.4), ("Lo-fi Master", 0.18, 0.4, 0.8, 11, 0.25),
]
for i, (n, amt, bal, wow, flut, flux) in enumerate(wl):
    p = add(P(n, CATS[4], "RC-20 style wobble (wow / flutter / flux) with lo-fi colour"))
    extra = ["Crusher"] if "Bit" in n or "8 Bit" in n else (["Phaser"] if "Phased" in n else (["Chorus"] if "Chorus" in n or "Rhodes" in n else []))
    p.chain(["Wobble"] + extra + ["Filter", "Tape", "Reverb"])
    p.set(wb_amt=amt, wb_bal=bal, wb_wow=wow, wb_flut=flut, wb_flux=flux, wb_spread=0.3, tp_drive=0.35, tp_hiss=0.1, tp_age=0.5,
          ft_type="LP", ft_cut=[7000, 4500, 3000][i % 3], ft_res=0.15, rv_mix=0.12, cr_bits=8 if "8 Bit" in n else 10, cr_rate=0.35)
    if "Telephone" in n: p.set(ft_type="BP", ft_cut=1400, ft_res=0.45)
    if "Formant" in n: p.set(ft_type="Formant", ft_vowel=0.3).lfo_(3, "Triangle", div="1 bar").mod("LFO 3", "ft_vowel", 0.5)
    if "Comb" in n: p.set(ft_type="Comb", ft_cut=220, ft_res=0.6, ft_mix=0.5).lfo_(3, "Smooth Random", div="1/2").mod("LFO 3", "ft_cut", 0.3)
    p.glitch(1, 0.15, repeat=0.5, stop=0.3, crush=0.5).lane(1, 16, 3, div="1/8")
    p.mod("Random 2", "wb_amt", 0.15).set(rn2_mode="Drift", rn2_sync=0, rn2_rate=0.3)
    macros(p)

# =============================================================================================== 6 Dub Delays
dd = [
    ("Dub Siren Space", "Ping-Pong", "1/8D", 0.7, 0, "3"), ("Chain Reaction", "Tape", "1/4D", 0.75, 0, "3"),
    ("Octave Spiral", "Digital", "1/8", 0.65, 12, "3"), ("Fifth Spiral", "Ping-Pong", "1/8D", 0.6, 7, "3"),
    ("Down Spiral", "Tape", "1/4", 0.7, -12, "3"), ("Multitap 3", "Multitap", "1/2", 0.45, 0, "3"), ("Multitap 5", "Multitap", "1/1", 0.45, 0, "5"),
    ("Multitap 7", "Multitap", "1/1", 0.4, 0, "7"), ("Frozen Echo", "Tape", "1/8D", 0.6, 0, "3"), ("Ducked Echo", "Ping-Pong", "1/8D", 0.6, 0, "3"),
    ("Echo Chamber", "Tape", "1/4T", 0.6, 0, "3"), ("Self Oscillation", "Tape", "1/16D", 1.02, 0, "3"), ("Dub Chords", "Ping-Pong", "1/4D", 0.65, 0, "3"),
    ("Space Echo Wobble", "Tape", "1/8D", 0.68, 0, "3"), ("Gated Echoes", "Ping-Pong", "1/16", 0.55, 0, "3"),
]
for i, (n, mode, div, fb, pitch, taps) in enumerate(dd):
    p = add(P(n, CATS[5], "dub delays: feedback filters, saturation, wobble and pitch in the loop"))
    p.chain(["Filter", "Delay", "Reverb", "Tape"] if "Chords" not in n else ["Resonator", "Delay", "Reverb", "Tape"])
    p.set(dl_mode=mode, dl_div=div, dl_fb=fb, dl_pitch=pitch, dl_taps=taps, dl_mix=0.4, dl_lp=2800, dl_hp=300, dl_sat=0.4, dl_wob=0.3 if "Wobble" in n else 0.12,
          ft_type="HP", ft_cut=120, rv_mix=0.25, rv_decay=3.5, rv_size=0.7, tp_drive=0.3, tp_hiss=0.05)
    if "Ducked" in n: p.set(dl_duck=0.7, dl_mix=0.55)
    if "Chords" in n: p.set(rs_chord="Minor 7", rs_mix=0.35, rs_decay=0.6, tu_root="F")
    if "Frozen" in n:
        p.lane(2, 16, 1, rot=12, div="1/4").set(en1_trig="Lane 2", en1_a=1, en1_h=500, en1_d=10, en1_s=0, en1_r=10)
        p.mod("Env 1", "dl_fb", 0.4)
    if "Gated" in n: p.glitch(1, 0.6, gate=1).set(gl_gatediv="1/32", gl_gateshape="Ramp Down").lane(1, 16, 6)
    else: p.glitch(1, 0.15, repeat=1, reverse=0.5)
    p.lfo_(1, "Sine", div="4 bar").mod("LFO 1", "dl_lp", 0.35).mod("LFO 1", "dl_hp", 0.2)
    p.lfo_(2, "Smooth Random", div="1 bar").mod("LFO 2", "dl_wob", 0.2)
    p.set(m_2=0.3)
    macros(p)

# =============================================================================================== 7 Shepard Risers
sr = [
    ("Endless Rise", "Up", "Octave", "8 bar", True, "Filter"), ("Endless Fall", "Down", "Octave", "8 bar", True, "Filter"),
    ("Barberpole Phaser", "Up", "Octave", "4 bar", False, "Phaser"), ("Barberpole Flanger", "Up", "Octave", "4 bar", False, "Flanger"),
    ("Infinite Notch", "Down", "Octave", "4 bar", False, "Notch"), ("Fifth Staircase", "Up", "Fifth", "4 bar", True, "Filter"),
    ("Tritone Vortex", "Up", "Tritone", "2 bar", True, "Phaser"), ("Pendulum Tone", "Pendulum", "Octave", "4 bar", True, None),
    ("Risset Drone", "Down", "Octave", "16 bar", True, None), ("Fourth Ladder", "Down", "Fourth", "8 bar", True, "Flanger"),
    ("Third Spiral", "Up", "Major 3rd", "2 bar", True, None), ("Riser Build", "Up", "Octave", "2 bar", True, "Filter"),
    ("Noise Band Spiral", "Up", "Octave", "4 bar", True, "Notch"), ("Micro Shepard", "Up", "Custom", "4 bar", True, None),
    ("Endless Tension", "Up", "Octave", "1 bar", True, "Flanger"),
]
for i, (n, direction, iv, bars, gen, fx) in enumerate(sr):
    p = add(P(n, CATS[6], "Shepard / Risset: endless glissandi and endless filters"))
    if gen:
        p.set(sh_on=1, sh_level=0.35, sh_dir=direction, sh_interval=iv, sh_bars=bars, sh_partials=8, sh_base=[300, 500, 200][i % 3],
              sh_width=0.45, sh_wave="Noise Band" if "Noise" in n else ("Triangle" if i % 4 == 1 else "Sine"), sh_cents=550 if iv == "Custom" else 700)
    mods = ["Shepard FX"] if fx else []
    p.chain(mods + ["Delay", "Reverb"])
    if fx: p.set(sf_mode=fx, sf_dir="Down" if direction == "Down" else "Up", sf_bars=bars, sf_mix=0.75, sf_res=0.55, sf_bands=8)
    p.set(dl_mode="Ping-Pong", dl_div="1/4D", dl_mix=0.2, dl_fb=0.5, rv_mix=0.35, rv_decay=5, rv_size=0.8)
    p.no_glitch()
    p.lfo_(1, "Sine", div="8 bar").mod("LFO 1", "sh_width", 0.2)
    macros(p)

# =============================================================================================== 8 Microtonal
mt = [
    ("Quarter Tone Perc", "24-EDO (quarter tones)", "D"), ("Just Bells", "Just 7-limit", "C"), ("Pelog Wood", "Pelog", "E"),
    ("Slendro Blips", "Slendro", "G"), ("Rast Resonance", "Maqam Rast", "D"), ("Bayati Grains", "Maqam Bayati", "A"),
    ("19 EDO Cloud", "19-EDO", "C"), ("31 EDO Glass", "31-EDO", "F"), ("53 EDO Drone", "53-EDO", "C"),
    ("Partch Rain", "Partch 43", "G"), ("Bohlen-Pierce Clicks", "Bohlen-Pierce", "C"), ("Harmonic Series", "Harmonic 8-16", "A"),
    ("Just Major Dub", "Just 5-limit Major", "F"), ("Whole Tone Haze", "Whole Tone", "C"), ("Microtonal Glitch", "24-EDO (quarter tones)", "G"),
]
for i, (n, scale, root) in enumerate(mt):
    p = add(P(n, CATS[7], "everything pitched follows the scale (Snap); load your own .scl / .kbm in TUNING"))
    p.set(tu_scale=scale, tu_root=root, tu_snap=1)
    p.lane(1, 16, 5, swing=0.1).lane(2, 13, 5).lane(3, 7, 3)
    models = [("Glass", "Mallet"), ("Wood", "Click"), ("Metal", "Mallet")]
    for v in range(3):
        m, e = models[(i + v) % 3]
        p.perc(v + 1, v + 1, m, e, [12, 19, 5][v] + 0.5 * (i % 2), [400, 150, 500][v], mat=0.3, inh=0.05, level=0.6, pan=[-0.3, 0.3, 0][v])
    p.set(pc_level=0.75)
    p.mod("Random 1", "pc1_tune", 0.12).mod("Random 2", "pc3_tune", 0.1).set(rn1_mode="S&H", rn1_div="1/16", rn2_mode="S&H", rn2_div="1/8")
    if i % 3 == 0:
        p.set(tx_on=1, tx_level=0.5, tx_bed=0.4, tx_resq=0.8, tx_drone=0.3, tx_chord="Scale", tx_grain=0.3, tx_prand=7, tx_gsrc="Input+Bed")
    p.chain(["Resonator", "Delay", "Reverb"] if i % 2 == 0 else ["Delay", "Reverb", "Tape"])
    p.set(rs_chord="Scale", rs_mix=0.3, rs_decay=0.55, dl_mode="Ping-Pong", dl_div="1/8D", dl_mix=0.2, dl_pitch=[0, 7, 5, 12][i % 4],
          dl_fb=0.45, rv_mix=0.25, rv_shim=0.2, rv_shint=7, rv_decay=3)
    if "Glitch" in n: p.glitch(1, 0.45, pitch=1, repeat=0.6).set(gl_pitch=5.5, gl_rptpitch=3.5)
    else: p.glitch(4, 0.15, pitch=1, repeat=0.4).lane(4, 16, 2, div="1/8")
    macros(p)

# =============================================================================================== 9 Polyrhythm
pr = [
    ("Three Against Two", "3:2", "1:1", "4:3"), ("Five Against Four", "5:4", "1:1", "3:2"), ("Seven Eight Drift", "7:8", "1:1", "5:8"),
    ("Polymeter 16 13 7", "1:1", "1:1", "1:1"), ("Quintuplet Rain", "5:4", "5:3", "1:1"), ("Septuplet Rolls", "7:4", "1:1", "3:2"),
    ("Phase Drift", None, None, None), ("Euclid Rotation", "1:1", "1:1", "1:1"), ("Ratchet Garden", "1:1", "3:2", "1:1"),
    ("Conditional Fills", "1:1", "1:1", "1:1"), ("Transient Follower", "1:1", "1:1", "1:1"), ("Nine Eight Shuffle", "9:8", "1:1", "3:4"),
    ("Eleven Steps", "11:8", "1:1", "1:1"), ("Tuplet Glitch", "3:2", "5:4", "7:8"), ("Polyrhythm Kit", "4:3", "5:4", "1:1"),
]
for i, (n, r1, r2, r3) in enumerate(pr):
    p = add(P(n, CATS[8], "lanes with own length, tuplet ratio, swing, probability, ratchets and conditions"))
    if r1 is None:
        p.lane(1, 16, 5, sync=0, hz=8.0).lane(2, 16, 5, sync=0, hz=8.07).lane(3, 12, 4, sync=0, hz=6.0)
    else:
        p.lane(1, 16, 5, ratio="1:1").lane(2, [12, 10, 7, 13, 15][i % 5], 5, ratio=r1).lane(3, [9, 7, 5, 7, 11][i % 5], 3, ratio=r2)
        p.lane(4, 5, 2, ratio=r3, div="1/8")
    if "Rotation" in n: p.lfo_(1, "Saw Up", div="4 bar", uni=1).mod("LFO 1", "ln2_rot", 0.25).mod("LFO 1", "ln3_pulses", 0.15)
    if "Ratchet" in n: p.set(ln2_ratch=3, ln2_rprob=0.4, ln3_ratch=4, ln3_rprob=0.3)
    if "Conditional" in n: p.set(ln2_cond="1:2", ln3_cond="1:4", ln4_cond="Fill")
    if "Transient" in n: p.set(ln2_src="Transient", ln3_src="Transient", in_trigthr=-30)
    for v, (lane, model, tune, dec) in enumerate([(1, "Wood", 12, 90), (2, "Glass", 24, 300), (3, "Skin", -5, 220), (4, "Metal", 19, 400)]):
        p.perc(v + 1, lane, model, "Mallet" if model == "Skin" else "Click", tune, dec, level=0.65, pan=[-0.4, 0.4, 0.0, 0.2][v], hum=0.2)
    p.set(pc_level=0.75)
    p.glitch(3, 0.3, repeat=1, slice=0.6, reverse=0.3).set(gl_rptdiv="1/16T")
    p.chain(["Tape", "Delay", "Reverb"])
    p.set(tp_drive=0.3, dl_mode="Multitap", dl_taps=["3", "5", "7", "4"][i % 4], dl_div="1/2", dl_mix=0.15, dl_fb=0.3, rv_mix=0.15)
    p.lfo_(2, "Triangle", div="1/4 5let").mod("LFO 2", "pc2_pan", 0.5).lfo_(3, "Sine", div="1/4 7let").mod("LFO 3", "pc1_decay", 0.25)
    p.set(lf2_retrig="Lane 2", lf3_retrig="Lane 3")
    macros(p)

# =============================================================================================== 10 Performance
pf = [
    "Macro Playground", "Density Ride", "Build And Drop", "MIDI Glitch Pads", "Breakdown Space", "Filter Throw",
    "Dirty Dubplate", "Peak Time Chop", "After Hours Haze", "One Knob Lo-fi", "Sidechain Pump Glitch", "Glitch Cascade",
    "Live Remix Tool", "Freeze And Release", "Everything Machine",
]
for i, n in enumerate(pf):
    p = add(P(n, CATS[9], "four macros wired to many parameters; MIDI C1-G#1 = glitch types, C2-F2 = percussion"))
    p.glitch(1, 0.4, repeat=1, reverse=0.4, slice=0.6, stop=0.2, gate=0.3, freeze=0.3, drop=0.2, pitch=0.3).lane(1, 16, 7, swing=0.12)
    p.chain(["Filter", "Wobble", "Tape", "Vinyl", "Delay", "Reverb", "Comp", "Stereo"])
    p.set(ft_type="LP", ft_cut=18000, ft_res=0.25, wb_amt=0.0, tp_drive=0.3, vn_dust=0.0, vn_crackle=0.0, vn_rumble=0.0, vn_wear=0.0,
          vn_year=2015, vn_mono=0.3, dl_mode="Ping-Pong", dl_div="1/8D", dl_mix=0.0, dl_fb=0.55, rv_mix=0.0, rv_decay=4,
          cp_thr=-14, cp_ratio=3, cp_mix=0.6, st_width=1.15)
    p.mod("Macro 1", "g_density", 0.5).mod("Macro 1", "gl_prob", 0.5).mod("Macro 1", "gl_mix", 0.3).mod("Macro 1", "ln1_pulses", 0.12)
    p.mod("Macro 2", "dl_mix", 0.45).mod("Macro 2", "rv_mix", 0.4).mod("Macro 2", "dl_fb", 0.25).mod("Macro 2", "ft_cut", -0.25)
    p.mod("Macro 3", "wb_amt", 0.6).mod("Macro 3", "vn_dust", 0.6).mod("Macro 3", "vn_crackle", 0.5).mod("Macro 3", "vn_wear", 0.5).mod("Macro 3", "tp_drive", 0.4)
    p.mod("Macro 4", "ft_cut", -0.7).mod("Macro 4", "ft_res", 0.3)
    p.set(m_1=[0.5, 0.2, 0.0, 0.3][i % 4], m_2=0.2, m_3=0.2, m_4=0.0)
    if "Build" in n: p.lfo_(1, "Saw Up", div="8 bar", uni=1).mod("LFO 1", "g_density", 0.4).mod("LFO 1", "ft_cut", -0.3).mod("LFO 1", "rv_mix", 0.3)
    if "Pump" in n: p.mod("Follower 1", "g_out", -0.15).set(fo1_gain=12)
    if "Freeze" in n: p.set(rv_freeze=0).mod("Macro 1", "tx_grain", 0.5).set(tx_on=1, tx_bed=0, tx_drone=0, tx_grain=0.0, tx_freeze=1, tx_gsrc="Input")
    if "Everything" in n:
        p.set(pc_on=1, tx_on=1, tx_bed=0.3, tx_grain=0.2).perc(1, 2, "Wood", "Click", 12, 90).perc(2, 3, "Glass", "Click", 24, 300).lane(2, 13, 5).lane(3, 7, 3)
    if "MIDI" in n: p.set(gl_lane="MIDI only")
    p.lfo_(4, "S&H", div="1/16").mod("LFO 4", "gl_rptpitch", 0.1, via="Macro 1")

# ----------------------------------------------------------------------------------------------- write
out = {"categories": CATS, "presets": [p.out() for p in presets]}
assert len(presets) == 150, len(presets)
path = os.path.join(ROOT, "resources", "presets.json")
json.dump(out, open(path, "w"), indent=0)
print("%d presets -> %s" % (len(presets), path))
