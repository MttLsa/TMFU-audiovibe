#!/usr/bin/env python3
"""
TWINSYNTH factory library generator  ->  resources/presets.json

Every preset recreates (by synthesis, no samples) a famous synth sound from the 1960s to the 2020s.
  * Section A (analog engine) plays sounds whose original instrument is analog.
  * Section D (digital engine) plays sounds whose original instrument is digital (FM, wavetable, PCM/ROM, VA, software).
  * Sounds that came from an analog + digital combination play on A and D in layer.
`original` = the real factory preset name, when the sound is a documented factory preset; otherwise empty
(studio patches have no factory name, the preset is then named after the track / artist it refers to).
`synth`, `synthA`, `synthD` name the instruments; `skinA` / `skinD` choose the panel look shown by the GUI.

Optional calibration: tools/levels.json (written by `TwinsynthTest <dir> levels`) -> g_trim per preset so that
all presets play at a similar loudness.
"""
import json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'resources', 'presets.json')
LEVELS = os.path.join(HERE, 'levels.json')

GENRES = ["House", "Deep House", "Tech House", "Techno", "Acid", "Minimal", "Melodic Techno", "Progressive House",
          "Trance", "EDM / Big Room", "Electro / Electroclash", "Eurodance / 90s Dance", "Breakbeat / Rave",
          "Drum & Bass", "Dubstep / Bass", "Disco / Italo / Hi-NRG", "Synth-Pop / New Wave", "Electronic / Ambient",
          "Progressive Rock", "Pop / Rock", "Funk / Soul / R&B", "Hip-Hop", "Film / TV", "Synthwave"]
DANCE = set(GENRES[:16])
CATS = ["Bass", "Lead", "Pad", "Keys", "Piano", "Organ", "Stab", "Chord", "Pluck", "Brass", "Strings", "Bell", "Sequence", "Vox", "FX"]

# instrument -> (A = analog | D = digital, skin)
SYNTHS = {
    # analog
    "Moog modular": ("A", "moog-modular"), "Moog Minimoog Model D": ("A", "minimoog"), "Moog Taurus": ("A", "minimoog"),
    "Moog Source": ("A", "modern-analog"), "Moog Prodigy": ("A", "minimoog"), "Polymoog": ("A", "minimoog"),
    "Moog Sub 37": ("A", "modern-analog"), "Moog One": ("A", "modern-analog"), "Moog Voyager": ("A", "minimoog"),
    "Electro-Theremin": ("A", "ems"), "Radiophonic oscillators": ("A", "ems"), "EMS VCS3 / Synthi AKS": ("A", "ems"),
    "ARP 2600": ("A", "arp"), "ARP Odyssey": ("A", "arp"), "ARP 2500": ("A", "arp"), "ARP Solina String Ensemble": ("A", "solina"),
    "Sequential Prophet-5": ("A", "prophet"), "Sequential Pro-One": ("A", "prophet"), "Sequential Prophet-6": ("A", "prophet"),
    "Oberheim OB-X": ("A", "oberheim"), "Oberheim OB-Xa": ("A", "oberheim"), "Oberheim OB-8": ("A", "oberheim"), "Oberheim SEM": ("A", "oberheim"),
    "Yamaha CS-80": ("A", "cs80"), "Korg MS-20": ("A", "ms20"), "Korg Polysix": ("A", "polysix"), "Korg Mono/Poly": ("A", "polysix"),
    "Roland Jupiter-8": ("A", "jupiter"), "Roland Jupiter-4": ("A", "jupiter"), "Roland Juno-60": ("A", "juno"), "Roland Juno-106": ("A", "juno"),
    "Roland Alpha Juno-2": ("A", "alpha-juno"), "Roland SH-101": ("A", "sh101"), "Roland TB-303": ("A", "tb303"), "Roland MC-202": ("A", "mc202"),
    "Roland JX-8P": ("A", "alpha-juno"), "Roland JX-3P": ("A", "alpha-juno"), "Elka Synthex": ("A", "jupiter"),
    "Analog poly": ("A", "juno"), "Analog mono": ("A", "modern-analog"),
    # digital
    "Yamaha DX7": ("D", "dx7"), "Yamaha TX-816": ("D", "tx81z"), "Yamaha TX81Z": ("D", "tx81z"), "Yamaha DX21": ("D", "dx7"),
    "Yamaha DX100": ("D", "dx7"), "Casio CZ-101": ("D", "cz"), "Casio CZ-5000": ("D", "cz"), "Fairlight CMI": ("D", "fairlight"),
    "Synclavier": ("D", "synclavier"), "Korg M1": ("D", "m1"), "Korg Wavestation": ("D", "wavestation"), "Korg Triton": ("D", "rompler"),
    "Roland D-50": ("D", "d50"), "Roland JD-800": ("D", "jd800"), "Roland JV-1080": ("D", "rompler"), "Roland JP-8000": ("D", "jp8000"),
    "E-mu Proteus / Orbit": ("D", "rompler"), "Kurzweil K2000": ("D", "rompler"), "Access Virus": ("D", "virus"), "Clavia Nord Lead": ("D", "nord"),
    "Waldorf Microwave": ("D", "ppg"), "Novation Supernova": ("D", "virus"), "Korg MS2000": ("D", "rompler"),
    "NI Massive": ("D", "software"), "Xfer Serum": ("D", "software"), "LennarDigital Sylenth1": ("D", "software"),
    "Reveal Spire": ("D", "software"), "Vital": ("D", "software"), "Digital VA": ("D", "jp8000"), "Digital rompler": ("D", "rompler"),
    "Software synth": ("D", "software"),
    # hybrid (digital oscillators + analog filters) -> A + D in layer
    "PPG Wave 2.3": ("AD", "ppg"), "Ensoniq ESQ-1": ("AD", "rompler"), "Korg DW-8000": ("AD", "polysix"),
    "Sequential Prophet VS": ("AD", "prophet"),
}

# ------------------------------------------------------------------------------------------------ building blocks
def A(**kw):
    d = {"a_on": 1, "d_on": 0}
    d.update({("a_" + k if not k.startswith(("g_", "a_", "d_")) else k): v for k, v in kw.items()})
    return d

def D(**kw):
    d = {"a_on": 0, "d_on": 1}
    d.update({("d_" + k if not k.startswith(("g_", "a_", "d_")) else k): v for k, v in kw.items()})
    return d

def L(a, d, **g):
    """layer: A part + D part"""
    r = {}
    r.update({k: v for k, v in a.items()})
    r.update({k: v for k, v in d.items()})
    r["a_on"] = 1; r["d_on"] = 1
    r.update(g)
    return r

def G(p, **g):
    p = dict(p); p.update({("g_" + k if not k.startswith("g_") else k): v for k, v in g.items()}); return p

# ----- analog archetypes (A) -----
def moog_bass(cut=380, res=0.25, env=0.55, dec=380, o2=-1, fine=6, glide=0, mono=True, sus=0.15, drive=0.35, **k):
    p = A(o1wave="Saw", o1oct=-1, o2wave="Saw", o2oct=o2, o2fine=fine, o2lvl=0.85, flttype="LP24", cut=cut, res=res,
          env=env, fa=1, fd=dec, fs=sus, fr=120, aa=1, ad=900, **{"as": 0.85}, ar=90, drive=drive, key=0.35, velf=0.25, drift=0.25)
    p.update(g_mode="Mono" if mono else "Poly", g_glide=glide)
    return upd(p, k)

def acid303(cut=300, res=0.82, env=0.62, dec=230, wave="Saw", accent=0.85, drive=0.45, glide=70, **k):
    p = A(o1wave=wave, o1oct=-1, o1pw=0.5, flttype="LP18", cut=cut, res=res, env=env, fa=0.5, fd=dec, fs=0.0, fr=60,
          aa=0.5, ad=1300, **{"as": 0.55}, ar=25, accent=accent, drive=drive, key=0.2, velf=0.15, drift=0.05)
    p.update(g_mode="Legato", g_glide=glide, g_vel=0.35)
    return upd(p, k)

def poly_pad(wave="Pulse", pw=0.42, pwm=0.45, cut=1500, res=0.15, env=0.25, att=900, rel=1800, chorus="II", sub=0.25, **k):
    p = A(o1wave=wave, o1pw=pw, pwm=pwm, sub=sub, o2wave="Saw", o2lvl=0.0, flttype="LP24", cut=cut, res=res, env=env,
          fa=att * 1.2, fd=2500, fs=0.6, fr=rel, aa=att, ad=1500, **{"as": 1.0}, ar=rel, lforate=0.6, lfoshape="Triangle", drift=0.3)
    p.update(g_chomode=chorus, g_chomix=0.7, g_revmix=0.18, g_revsize=0.7)
    return upd(p, k)

def poly_stab(cut=900, res=0.25, env=0.55, dec=260, chorus="I", wave="Saw", o2="Pulse", **k):
    p = A(o1wave=wave, o2wave=o2, o2lvl=0.6, o2fine=7, o2pw=0.35, flttype="LP24", cut=cut, res=res, env=env, fa=1, fd=dec, fs=0.0, fr=dec,
          aa=1, ad=dec * 1.6, **{"as": 0.0}, ar=dec * 0.8, drift=0.2, key=0.5, velf=0.35)
    p.update(g_chomode=chorus, g_chomix=0.55)
    return upd(p, k)

def brass(cut=700, res=0.18, env=0.55, att=40, dec=700, sus=0.55, o2fine=9, chorus="Off", **k):
    p = A(o1wave="Saw", o2wave="Saw", o2lvl=0.9, o2fine=o2fine, flttype="LP24", cut=cut, res=res, env=env, fa=att, fd=dec, fs=sus,
          fr=300, aa=8, ad=900, **{"as": 0.9}, ar=280, drift=0.3, velf=0.4, key=0.5, uni=2, detune=0.12)
    p.update(g_chomode=chorus, g_chomix=0.5)
    return upd(p, k)

def lead(wave="Saw", cut=2400, res=0.3, env=0.35, glide=60, vib=0.0, o2fine=8, o2lvl=0.7, **k):
    p = A(o1wave=wave, o2wave="Saw", o2lvl=o2lvl, o2fine=o2fine, flttype="LP24", cut=cut, res=res, env=env, fa=2, fd=600, fs=0.5,
          fr=200, aa=3, ad=600, **{"as": 0.9}, ar=180, lforate=5.5, lfoshape="Sine", lfopitch=vib, lfodelay=350 if vib else 0, drift=0.2, drive=0.3)
    p.update(g_mode="Mono", g_glide=glide, g_dlymix=0.12)
    return upd(p, k)

def sync_lead(semi=7, penv=19, pdec=500, cut=4500, **k):
    p = A(o1wave="Saw", o2wave="Saw", o2lvl=1.0, o1lvl=0.0, o2semi=semi, sync=1, penv=0, flttype="LP24", cut=cut, res=0.15, env=0.25,
          fa=1, fd=500, fs=0.6, fr=200, aa=2, ad=500, **{"as": 0.9}, ar=200, drift=0.15)
    # pitch env applied to the slave only would be ideal; we sweep the osc2 offset with the pitch envelope of the whole section
    p["a_penv"] = penv; p["a_pdec"] = pdec
    p.update(g_mode="Mono", g_glide=30)
    return upd(p, k)

def strings(cut=3200, att=500, rel=1500, chorus="Ensemble", **k):
    p = A(o1wave="Saw", o2wave="Saw", o2oct=1, o2lvl=0.5, o2fine=4, flttype="LP12", cut=cut, res=0.05, env=0.1, fa=att, fd=1000,
          fs=0.8, fr=rel, aa=att, ad=1000, **{"as": 1.0}, ar=rel, drift=0.25)
    p.update(g_chomode=chorus, g_chomix=0.85, g_revmix=0.2)
    return upd(p, k)

def hoover(**k):
    # Alpha Juno "What The": pulse-saw, heavy PWM, chorus, pitch bend dives, portamento
    p = A(o1wave="Pulse", o1pw=0.3, pwm=0.7, o2wave="Saw", o2lvl=0.8, o2oct=-1, o2fine=-12, sub=0.4, uni=5, detune=0.55,
          flttype="LP24", cut=4200, res=0.1, env=0.15, fa=1, fd=900, fs=0.8, fr=400, aa=2, ad=700, **{"as": 1.0}, ar=350,
          lforate=7.0, lfoshape="Triangle", penv=-12, pdec=180, drift=0.3)
    p.update(g_mode="Mono", g_glide=180, g_chomode="I+II", g_chomix=0.8, g_bend=12)
    return upd(p, k)

def sh_bass(cut=600, res=0.35, env=0.5, dec=220, sub=0.7, wave="Pulse", **k):
    p = A(o1wave=wave, o1oct=-1, o1pw=0.5, sub=sub, flttype="LP24", cut=cut, res=res, env=env, fa=0.5, fd=dec, fs=0.1, fr=80,
          aa=0.5, ad=600, **{"as": 0.7}, ar=60, drive=0.3, key=0.3, velf=0.3, drift=0.08)
    p.update(g_mode="Mono")
    return upd(p, k)

def pluck_a(cut=700, res=0.3, env=0.7, dec=180, **k):
    p = A(o1wave="Saw", o2wave="Pulse", o2lvl=0.5, o2fine=6, flttype="LP24", cut=cut, res=res, env=env, fa=0.5, fd=dec, fs=0.0,
          fr=dec, aa=0.5, ad=dec * 2.5, **{"as": 0.0}, ar=dec, drift=0.1, velf=0.4, key=0.6)
    p.update(g_dlymix=0.18, g_dlydiv="1/8D", g_revmix=0.12)
    return upd(p, k)

def zap(**k):
    p = A(o1wave="Sine", penv=36, pdec=90, flttype="LP12", cut=12000, aa=0.5, ad=250, **{"as": 0.0}, ar=150, env=0.0)
    return upd(p, k)

def noise_sweep(**k):
    p = A(o1lvl=0.0, noise=1.0, flttype="BP12", cut=400, res=0.6, env=0.8, fa=3000, fd=3000, fs=0.0, fr=2000, aa=1500, ad=3000,
          **{"as": 0.0}, ar=2000)
    p.update(g_revmix=0.35, g_dlymix=0.2)
    return upd(p, k)

# ----- digital archetypes (D) -----
def fm(alg, ratios, levels, decays, sus, fb=0.0, vel=0.6, atk=1, **k):
    p = D(engine="FM", alg=alg, fb=fb, fmvel=vel, fmatk=atk, fltt=None)
    p.pop("d_fltt")
    for i in range(4):
        p["d_r%d" % (i + 1)] = ratios[i]; p["d_l%d" % (i + 1)] = levels[i]
        p["d_e%d" % (i + 1)] = decays[i]; p["d_s%d" % (i + 1)] = sus[i]
    p.update(d_flttype="Off", d_aa=1, d_ad=3000, d_as=1.0, d_ar=350)
    return upd(p, k)

def fm_epiano(bright=0.5, **k):
    return fm("5 Two Pairs", [1, 1, 1, 14], [1.0, 0.55 + 0.2 * bright, 0.75, 0.42 + 0.2 * bright], [4200, 1400, 2600, 260],
              [0.0, 0.12, 0.0, 0.0], vel=0.85, **upd(dict(d_uni=2, d_detune=0.03, d_ar=450, g_chomode="I", g_chomix=0.35, g_vel=0.7), k))

def fm_bass(alg="1 Stack", r=(0.5, 0.5, 1, 1), l=(1.0, 0.72, 0.35, 0.0), e=(1200, 420, 160, 200), s=(0.6, 0.15, 0.0, 0.0), fb=0.35, **k):
    return fm(alg, list(r), list(l), list(e), list(s), fb=fb, vel=0.5,
              **upd(dict(d_aa=0.5, d_ad=900, d_as=0.45, d_ar=90, g_mode="Mono"), k))

def fm_bell(r2=3.5, **k):
    return fm("5 Two Pairs", [1, r2, 1, 7.0], [1.0, 0.62, 0.55, 0.4], [6000, 3500, 2500, 900], [0.0, 0.0, 0.0, 0.0], vel=0.6,
              **upd(dict(d_ad=8000, d_as=0.0, d_ar=2500, g_revmix=0.25), k))

def fm_brass(**k):
    return fm("1 Stack", [1, 1, 1, 1], [1.0, 0.6, 0.45, 0.3], [2000, 1500, 1200, 800], [0.9, 0.55, 0.4, 0.3], fb=0.45, vel=0.5, atk=60,
              **upd(dict(d_aa=25, d_ar=250, d_uni=2, d_detune=0.06), k))

def fm_pluck(r=(1, 3, 1, 1), **k):
    return fm("1 Stack", list(r), [1.0, 0.6, 0.3, 0.0], [400, 140, 90, 50], [0.0, 0.0, 0.0, 0.0], fb=0.2, vel=0.7,
              **upd(dict(d_ad=500, d_as=0.0, d_ar=150, g_dlymix=0.2, g_dlydiv="1/8D"), k))

def wt(table, pos=0.0, wenv=0.0, cut=6000, res=0.1, env=0.2, att=2, dec=600, sus=0.8, rel=300, uni=1, det=0.2, **k):
    p = D(engine="Wavetable", wt=table, wpos=pos, wenv=wenv, flttype="LP24", cut=cut, res=res, env=env, fa=att, fd=dec, fs=0.4, fr=rel,
          aa=att, ad=dec, **{"as": sus}, ar=rel, uni=uni, detune=det)
    return upd(p, k)

def supersaw(det=0.45, mix=0.65, cut=9000, att=2, rel=400, sus=1.0, dec=600, **k):
    p = D(engine="Supersaw", ssdet=det, ssmix=mix, flttype="LP12", cut=cut, res=0.1, env=0.15, fa=att, fd=dec, fs=0.6, fr=rel,
          aa=att, ad=dec, **{"as": sus}, ar=rel)
    p.update(g_width=0.85, g_revmix=0.2, g_dlymix=0.12)
    return upd(p, k)

def pd(wave="Saw", amt=0.6, penv=0.0, cut=20000, res=0.0, flt="Off", uni=1, det=0.1, dec=600, sus=0.8, att=1, rel=200, **k):
    p = D(engine="Phase Dist", pdw=wave, pdamt=amt, pdenv=penv, flttype=flt, cut=cut, res=res, uni=uni, detune=det,
          fa=1, fd=dec, fs=0.2, fr=rel, aa=att, ad=dec, **{"as": sus}, ar=rel)
    return upd(p, k)

def add(model, bright=0.5, tilt=0.5, inh=0.0, even=1.0, att=1, dec=3000, sus=1.0, rel=300, cut=20000, flt="Off", **k):
    p = D(engine="Additive", addm=model, bright=bright, tilt=tilt, inh=inh, even=even, aa=att, ad=dec, **{"as": sus}, ar=rel,
          flttype=flt, cut=cut)
    return upd(p, k)

def upd(p, k):
    p = dict(p)
    for kk, v in k.items():
        if kk.startswith(("g_", "a_", "d_")): p[kk] = v
        else:
            pre = "a_" if p.get("a_on") and not p.get("d_on") else "d_"
            p[pre + kk] = v
    return p

# ------------------------------------------------------------------------------------------------ the library
P = []
def add_p(name, year, genre, synth, cat, ref, params, original="", notes=""):
    P.append(dict(name=name, year=year, genre=genre, synth=synth, cat=cat, ref=ref, p=params, original=original, notes=notes))

UNC = "Attribuzione dello strumento non documentata con certezza: suono ricreato nello stile del brano."
STY = "Patch di genere (stile), non un preset di fabbrica."

# =========================================================================================== 1960s
add_p("Switched-On Lead", 1968, "Electronic / Ambient", "Moog modular", "Lead", "Wendy Carlos - Switched-On Bach (1968)",
      lead(wave="Pulse", cut=2600, res=0.2, glide=0, vib=0.15, o2lvl=0.0, a_o1pw=0.3, g_mode="Poly", g_revmix=0.15))
add_p("Bach Moog Bass", 1968, "Electronic / Ambient", "Moog modular", "Bass", "Wendy Carlos - Switched-On Bach (1968)",
      moog_bass(cut=600, res=0.1, env=0.4, dec=300, mono=False))
add_p("Tannerin Glide", 1966, "Pop / Rock", "Electro-Theremin", "Lead", "The Beach Boys - Good Vibrations (1966)",
      lead(wave="Sine", o2lvl=0.0, cut=6000, res=0.0, env=0.0, glide=260, vib=0.35, a_lforate=6.0, a_drive=0.0, g_revmix=0.2))
add_p("Radiophonic Bass", 1963, "Film / TV", "Radiophonic oscillators", "Bass", "BBC Radiophonic Workshop - Doctor Who theme (1963), stile",
      A(o1wave="Triangle", o1oct=-1, o2wave="Sine", o2lvl=0.4, o2oct=-1, flttype="LP12", cut=900, res=0.3, env=0.3, fd=200, fs=0.3,
        aa=1, ad=300, **{"as": 0.6}, ar=80, g_mode="Mono", g_dlymix=0.15), notes=STY)
add_p("Abbey Road Moog", 1969, "Pop / Rock", "Moog modular", "Lead", "The Beatles - Here Comes the Sun / Because (1969)",
      lead(wave="Saw", o2lvl=0.0, cut=1800, res=0.15, glide=40, vib=0.25, a_aa=60, g_revmix=0.25))
add_p("Kaleidoscopic Blip", 1967, "Electronic / Ambient", "Moog modular", "Sequence", "Perrey & Kingsley - Kaleidoscopic Vibrations (1967)",
      pluck_a(cut=1200, res=0.5, dec=120, a_o1wave="Pulse", a_o2lvl=0.0, g_dlymix=0.2))
add_p("Space Odyssey Drone", 1969, "Film / TV", "Moog modular", "FX", "Musica elettronica di fine anni '60 (stile)",
      A(o1wave="Saw", o2wave="Saw", o2lvl=1, o2fine=-35, uni=3, detune=0.6, flttype="LP24", cut=300, res=0.6, lfocut=0.6, lforate=0.08,
        aa=3000, ad=4000, **{"as": 1}, ar=5000, g_revmix=0.5, g_revsize=0.95), notes=STY)

# =========================================================================================== 1970s
add_p("Lucky Man Solo", 1970, "Progressive Rock", "Moog modular", "Lead", "Emerson, Lake & Palmer - Lucky Man (1970)",
      lead(wave="Saw", o2wave="Pulse", cut=2000, res=0.35, env=0.45, glide=180, vib=0.3, a_lforate=5.2, g_dlymix=0.18))
add_p("Autobahn Bass", 1974, "Electronic / Ambient", "Moog Minimoog Model D", "Bass", "Kraftwerk - Autobahn (1974)",
      moog_bass(cut=320, res=0.2, env=0.45, dec=300, fine=0))
add_p("Autobahn Lead", 1974, "Electronic / Ambient", "Moog Minimoog Model D", "Lead", "Kraftwerk - Autobahn (1974)",
      lead(wave="Triangle", o2wave="Triangle", cut=3000, res=0.1, glide=90, vib=0.2))
add_p("Chameleon Bass", 1973, "Funk / Soul / R&B", "ARP Odyssey", "Bass", "Herbie Hancock - Chameleon (1973)",
      moog_bass(cut=450, res=0.45, env=0.6, dec=260, o2=-1, fine=0, a_o2wave="Pulse", a_flttype="LP24", g_glide=20))
add_p("On the Run Sequence", 1973, "Progressive Rock", "EMS VCS3 / Synthi AKS", "Sequence", "Pink Floyd - On the Run (1973)",
      A(o1wave="Saw", o2wave="Pulse", o2lvl=0.5, o2fine=10, flttype="LP12", cut=900, res=0.55, env=0.4, fd=130, fs=0.1, aa=0.5, ad=160,
        **{"as": 0.2}, ar=60, lfocut=0.35, lforate=0.25, g_dlymix=0.3, g_dlydiv="1/16", g_dlyfb=0.55, g_mode="Mono"))
add_p("Welcome Machine Pad", 1975, "Progressive Rock", "EMS VCS3 / Synthi AKS", "Pad", "Pink Floyd - Welcome to the Machine (1975)",
      A(o1wave="Saw", o2wave="Saw", o2lvl=1, o2fine=-20, flttype="LP24", cut=500, res=0.55, lfocut=0.5, lforate=0.12, aa=1500, ad=2000,
        **{"as": 1}, ar=3000, g_revmix=0.4, g_revsize=0.85))
add_p("Flash Light Bass", 1977, "Funk / Soul / R&B", "Moog Minimoog Model D", "Bass", "Parliament - Flash Light (1977), Bernie Worrell",
      moog_bass(cut=280, res=0.35, env=0.7, dec=420, o2=-1, fine=4, glide=35, a_o2wave="Pulse"))
add_p("Living City Bass", 1973, "Funk / Soul / R&B", "Moog modular", "Bass", "Stevie Wonder - Living for the City (1973, TONTO)",
      moog_bass(cut=500, res=0.25, env=0.5, dec=500))
add_p("I Feel Love Bass", 1977, "Disco / Italo / Hi-NRG", "Moog modular", "Bass", "Donna Summer - I Feel Love (1977), Giorgio Moroder",
      moog_bass(cut=420, res=0.3, env=0.6, dec=160, o2=0, fine=0, sus=0.0, a_ad=220, a_as=0.0, a_ar=60, mono=False, g_dlymix=0.22, g_dlydiv="1/16", g_dlyfb=0.2, g_dlyping=0))
add_p("Chase Sequence", 1978, "Disco / Italo / Hi-NRG", "Moog modular", "Sequence", "Giorgio Moroder - Chase (1978)",
      moog_bass(cut=650, res=0.45, env=0.55, dec=140, o2=0, sus=0.0, a_ad=180, a_as=0.0, mono=False, g_dlymix=0.15, g_dlydiv="1/16"))
add_p("Supernature Arp", 1977, "Disco / Italo / Hi-NRG", "ARP 2600", "Sequence", "Cerrone - Supernature (1977), stile",
      pluck_a(cut=900, res=0.55, dec=150, a_o2wave="Saw"), notes=UNC)
add_p("Oxygene Strings", 1976, "Electronic / Ambient", "ARP Solina String Ensemble", "Strings", "Jean-Michel Jarre - Oxygène (1976), string machine",
      strings(cut=2600, att=700, rel=2500, g_revmix=0.4, g_dlymix=0.15))
add_p("Oxygene Lead", 1976, "Electronic / Ambient", "ARP 2600", "Lead", "Jean-Michel Jarre - Oxygène IV (1976), stile",
      lead(wave="Pulse", a_o1pw=0.25, o2lvl=0.4, cut=2200, res=0.3, vib=0.3, glide=90, g_dlymix=0.3, g_revmix=0.3, g_chomode="I"), notes=UNC)
add_p("Close Encounters 5", 1977, "Film / TV", "ARP 2500", "Lead", "John Williams - Close Encounters of the Third Kind (1977)",
      lead(wave="Saw", o2lvl=0.5, cut=1600, res=0.2, glide=0, vib=0.0, g_mode="Poly", a_aa=40, g_revmix=0.35))
add_p("Cars Lead", 1979, "Synth-Pop / New Wave", "Moog Minimoog Model D", "Lead", "Gary Numan - Cars (1979)",
      lead(wave="Saw", o2wave="Saw", o2lvl=0.9, o2fine=0, a_o2oct=-1, cut=900, res=0.35, env=0.45, glide=0, a_fd=900))
add_p("Vox Humana", 1979, "Synth-Pop / New Wave", "Polymoog", "Pad", "Gary Numan - Cars (1979), Polymoog",
      poly_pad(wave="Saw", pwm=0.0, cut=1800, res=0.1, env=0.15, att=60, rel=600, chorus="Ensemble", sub=0.0, a_o2wave="Saw", a_o2lvl=0.8, a_o2fine=-14),
      original="Vox Humana")
add_p("Trans-Europe Lead", 1977, "Electronic / Ambient", "Moog Minimoog Model D", "Lead", "Kraftwerk - Trans-Europe Express (1977), stile",
      lead(wave="Pulse", a_o1pw=0.4, o2lvl=0.0, cut=1500, res=0.2, glide=0, g_mode="Poly", g_revmix=0.25), notes=UNC)
add_p("The Model Arp", 1978, "Synth-Pop / New Wave", "Moog Minimoog Model D", "Sequence", "Kraftwerk - The Model (1978), stile",
      pluck_a(cut=1200, res=0.25, dec=200, a_o2lvl=0.0), notes=UNC)
add_p("Rydeen Brass", 1980, "Synth-Pop / New Wave", "Sequential Prophet-5", "Brass", "Yellow Magic Orchestra - Rydeen (1980), stile",
      brass(cut=900, env=0.5, att=20, dec=400, sus=0.6), notes=UNC)
add_p("Phaedra Sequence", 1974, "Electronic / Ambient", "Moog modular", "Sequence", "Tangerine Dream - Phaedra (1974)",
      A(o1wave="Saw", o2wave="Saw", o2lvl=0.7, o2fine=8, flttype="LP24", cut=500, res=0.65, env=0.5, fd=140, fs=0.0, aa=0.5, ad=200, **{"as": 0.0},
        ar=100, lfocut=0.6, lforate=0.07, g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.25))
add_p("Funk Moog Lead", 1975, "Funk / Soul / R&B", "Moog Minimoog Model D", "Lead", "Synth-funk anni '70 (stile)",
      lead(wave="Saw", cut=1500, res=0.45, env=0.55, glide=50, a_fd=350), notes=STY)
add_p("Taurus Pedal Bass", 1977, "Progressive Rock", "Moog Taurus", "Bass", "Genesis / Rush - pedaliera Moog Taurus (fine anni '70)",
      moog_bass(cut=200, res=0.1, env=0.3, dec=1500, sus=0.6, a_ad=3000, a_as=0.8, a_ar=900, o2=-2, fine=3))
add_p("Solina Disco Strings", 1978, "Disco / Italo / Hi-NRG", "ARP Solina String Ensemble", "Strings", "Disco anni '70 - string machine (stile)",
      strings(cut=3800, att=120, rel=900), original="", notes=STY)
add_p("Prophet Brass 5", 1979, "Pop / Rock", "Sequential Prophet-5", "Brass", "Prophet-5 brass (fine anni '70)",
      brass(cut=800, env=0.6, att=60, dec=600, sus=0.5))
add_p("Space Funk Odyssey", 1976, "Funk / Soul / R&B", "ARP Odyssey", "Lead", "ARP Odyssey lead (stile funk/fusion)",
      lead(wave="Pulse", a_o1pw=0.3, cut=1700, res=0.5, env=0.5, glide=40, a_drive=0.4), notes=STY)
add_p("Magic Fly Lead", 1977, "Disco / Italo / Hi-NRG", "Moog Minimoog Model D", "Lead", "Space - Magic Fly (1977), stile",
      lead(wave="Saw", cut=2500, res=0.3, vib=0.25, glide=80, g_chomode="I", g_dlymix=0.2), notes=UNC)

# =========================================================================================== 1980s pop / film / synth-pop
add_p("Jump Brass", 1983, "Pop / Rock", "Oberheim OB-Xa", "Brass", "Van Halen - Jump (1983)",
      brass(cut=1600, res=0.08, env=0.35, att=5, dec=1200, sus=0.75, o2fine=12, a_uni=3, a_detune=0.2, a_aa=2, g_chomode="I", g_chomix=0.3))
add_p("1999 Stabs", 1982, "Funk / Soul / R&B", "Oberheim OB-Xa", "Stab", "Prince - 1999 (1982)",
      poly_stab(cut=1400, res=0.15, env=0.5, dec=320, chorus="Off", a_uni=2, a_detune=0.15))
add_p("Axel F Lead", 1984, "Film / TV", "Roland Jupiter-8", "Lead", "Harold Faltermeyer - Axel F (1984)",
      A(o1wave="Pulse", o1pw=0.5, o2wave="Saw", o2lvl=0.35, o2oct=1, flttype="LP24", cut=2600, res=0.15, env=0.3, fa=1, fd=220, fs=0.5,
        aa=1, ad=300, **{"as": 0.75}, ar=120, g_mode="Poly", g_dlymix=0.18, g_dlydiv="1/8D", g_revmix=0.15))
add_p("Axel F Bass", 1984, "Film / TV", "Moog Minimoog Model D", "Bass", "Harold Faltermeyer - Axel F (1984)",
      moog_bass(cut=500, res=0.25, env=0.45, dec=200, o2=0, fine=7))
add_p("Chariots Brass", 1981, "Film / TV", "Yamaha CS-80", "Brass", "Vangelis - Chariots of Fire (1981)",
      brass(cut=1200, res=0.15, env=0.45, att=140, dec=1600, sus=0.7, a_lforate=5.0, a_lfopitch=0.15, a_lfodelay=600, g_revmix=0.45, g_revsize=0.9))
add_p("Blade Runner Brass", 1982, "Film / TV", "Yamaha CS-80", "Brass", "Vangelis - Blade Runner, End Titles (1982)",
      brass(cut=900, res=0.25, env=0.55, att=350, dec=2500, sus=0.6, a_aa=200, a_ar=1500, a_lforate=5.5, a_lfopitch=0.2, a_lfodelay=800,
            g_revmix=0.55, g_revsize=0.95, g_dlymix=0.15))
add_p("Blade Runner Blues", 1982, "Film / TV", "Yamaha CS-80", "Pad", "Vangelis - Blade Runner Blues (1982)",
      poly_pad(wave="Saw", pwm=0.0, cut=1100, res=0.2, env=0.3, att=400, rel=2500, chorus="Ensemble", a_lfopitch=0.12, a_lforate=5,
               g_revmix=0.5, g_revsize=0.95))
add_p("In the Air Pad", 1981, "Pop / Rock", "Sequential Prophet-5", "Pad", "Phil Collins - In the Air Tonight (1981)",
      poly_pad(wave="Saw", pwm=0.0, cut=900, res=0.3, env=0.2, att=300, rel=2000, chorus="I", a_o2wave="Pulse", a_o2lvl=0.6, a_o2fine=8,
               g_revmix=0.35))
add_p("Blue Monday Bass", 1983, "Synth-Pop / New Wave", "Moog Source", "Bass", "New Order - Blue Monday (1983)",
      sh_bass(cut=700, res=0.2, env=0.45, dec=180, sub=0.0, wave="Saw", g_mode="Poly"))
add_p("Sweet Dreams Riff", 1983, "Synth-Pop / New Wave", "Oberheim OB-X", "Sequence", "Eurythmics - Sweet Dreams (1983)",
      pluck_a(cut=1300, res=0.2, dec=260, a_o2wave="Saw", a_o2fine=10, g_dlymix=0.1), notes=UNC)
add_p("Take On Me Riff", 1985, "Synth-Pop / New Wave", "Roland Juno-60", "Lead", "a-ha - Take On Me (1985)",
      A(o1wave="Pulse", o1pw=0.45, o2lvl=0.0, sub=0.3, flttype="LP24", cut=3200, res=0.1, env=0.2, fd=300, fs=0.6, aa=1, ad=400, **{"as": 0.8},
        ar=180, g_chomode="I", g_chomix=0.6), notes=UNC)
add_p("Tom Sawyer Growl", 1981, "Progressive Rock", "Oberheim OB-X", "Lead", "Rush - Tom Sawyer (1981)",
      A(o1wave="Saw", o2wave="Saw", o2lvl=1, o2fine=-15, o1oct=-1, flttype="LP24", cut=250, res=0.55, env=0.75, fa=600, fd=1200, fs=0.3,
        aa=5, ad=900, **{"as": 1.0}, ar=400, g_mode="Mono", g_glide=150, a_drive=0.4))
add_p("Synthpop Juno Bass", 1984, "Synth-Pop / New Wave", "Roland Juno-106", "Bass", "Synth-pop anni '80 (stile)",
      sh_bass(cut=900, res=0.2, env=0.35, dec=300, sub=0.6, g_chomode="I", g_chomix=0.4), notes=STY)
add_p("Jupiter Pad", 1982, "Synth-Pop / New Wave", "Roland Jupiter-8", "Pad", "Jupiter-8 pad (Duran Duran, Tears for Fears, anni '80)",
      poly_pad(wave="Saw", pwm=0.0, cut=1600, res=0.15, att=500, rel=2000, chorus="II", a_o2wave="Saw", a_o2lvl=0.8, a_o2fine=-9))
add_p("Final Countdown Brass", 1986, "Pop / Rock", "Yamaha TX-816 + Roland JX-8P", "Brass", "Europe - The Final Countdown (1986): FM + analog layer",
      L(brass(cut=1400, env=0.4, att=30, dec=900, sus=0.7, a_level=0.6),
        fm_brass(d_level=0.7), g_chomode="I", g_chomix=0.35, g_revmix=0.25))
add_p("E.PIANO 1", 1983, "Pop / Rock", "Yamaha DX7", "Keys", "Ballate pop anni '80 (Whitney Houston, Chicago, Phil Collins...)",
      fm_epiano(), original="E.PIANO 1")
add_p("BASS 1", 1983, "Synth-Pop / New Wave", "Yamaha DX7", "Bass", "Synth-pop e funk anni '80",
      fm_bass(alg="1 Stack", r=(0.5, 0.5, 1.0, 1.0), l=(1.0, 0.78, 0.5, 0.3), e=(1600, 500, 250, 120), s=(0.4, 0.1, 0.0, 0.0), fb=0.5),
      original="BASS 1")
add_p("Take My Breath Bass", 1986, "Synth-Pop / New Wave", "Yamaha DX7", "Bass", "Berlin - Take My Breath Away (1986)",
      fm_bass(alg="1 Stack", r=(0.5, 0.5, 1.0, 1.0), l=(1.0, 0.55, 0.25, 0.0), e=(3000, 900, 300, 200), s=(0.7, 0.3, 0.0, 0.0), fb=0.15,
              d_as=0.8, d_ar=250, g_mode="Poly", g_chomode="I", g_chomix=0.3))
add_p("TUB BELLS", 1983, "Pop / Rock", "Yamaha DX7", "Bell", "DX7 ROM - campane tubolari (anni '80)", fm_bell(r2=3.5), original="TUB BELLS")
add_p("BRASS 1", 1983, "Pop / Rock", "Yamaha DX7", "Brass", "DX7 ROM - ottoni synth anni '80", fm_brass(), original="BRASS 1")
add_p("MARIMBA", 1983, "Pop / Rock", "Yamaha DX7", "Bell", "DX7 ROM - marimba FM (pop anni '80)",
      fm("5 Two Pairs", [1, 4, 1, 10], [1.0, 0.45, 0.5, 0.3], [600, 60, 400, 40], [0, 0, 0, 0], vel=0.6, d_ad=900, d_as=0.0, d_ar=200),
      original="MARIMBA")
add_p("CLAV 1", 1983, "Funk / Soul / R&B", "Yamaha DX7", "Keys", "DX7 ROM - clavinet FM",
      fm("1 Stack", [1, 3, 1, 1], [1.0, 0.7, 0.4, 0.2], [700, 300, 150, 80], [0.3, 0.15, 0.0, 0.0], fb=0.4, vel=0.7, d_ad=900, d_as=0.4, d_ar=60),
      original="CLAV 1")
add_p("Tainted Love Bink", 1981, "Synth-Pop / New Wave", "Synclavier", "FX", "Soft Cell - Tainted Love (1981)",
      fm("1 Stack", [1, 1.41, 1, 1], [1.0, 0.65, 0.0, 0.0], [180, 90, 50, 50], [0, 0, 0, 0], vel=0.3, d_ad=250, d_as=0.0, d_ar=120, d_oct=1,
         g_revmix=0.15))
add_p("ORCH5", 1982, "Hip-Hop", "Fairlight CMI", "Stab", "Afrika Bambaataa - Planet Rock (1982), orchestra hit",
      add("Brass", bright=0.8, att=1, dec=420, sus=0.0, rel=200, d_bits=0.55, d_srr=0.35, d_oct=-1, d_uni=1, g_revmix=0.2), original="ORCH5",
      notes="Fairlight CMI = campionatore digitale a 8 bit: il colpo d'orchestra è sintetizzato in additivo con riduzione di bit e di frequenza.")
add_p("Running Up Cello", 1985, "Pop / Rock", "Fairlight CMI", "Strings", "Kate Bush - Running Up That Hill (1985)",
      add("Strings", bright=0.35, att=40, dec=2000, sus=1.0, rel=500, d_bits=0.4, d_srr=0.2, d_oct=-1, d_flttype="LP12", d_cut=2500,
          d_lfopitch=0.12, d_lforate=5.5))
add_p("Fairlight Choir", 1984, "Synth-Pop / New Wave", "Fairlight CMI", "Vox", "Coro campionato del Fairlight (Art of Noise, Yes, anni '80)",
      add("Choir", bright=0.4, att=250, rel=1200, d_bits=0.45, d_srr=0.25, g_revmix=0.35))
add_p("PPG Wave Pad", 1982, "Synth-Pop / New Wave", "PPG Wave 2.3", "Pad", "Pad PPG (Thomas Dolby, Tangerine Dream, primi anni '80)",
      L(poly_pad(wave="Saw", pwm=0.0, cut=900, res=0.35, att=400, rel=1600, chorus="Off", a_level=0.45, a_sub=0.0),
        wt("PPG Digital", pos=0.15, wenv=0.4, cut=4000, att=300, dec=2500, sus=0.9, rel=1600, uni=2, det=0.1, d_bits=0.3), g_revmix=0.3),
      notes="PPG Wave: oscillatori wavetable digitali + filtro analogico, quindi suona su D (wavetable) e A (corpo analogico filtrato) in layer.")
add_p("PPG Digital Bell", 1983, "Synth-Pop / New Wave", "PPG Wave 2.3", "Bell", "Campane digitali PPG (stile)",
      L(pluck_a(cut=1500, res=0.35, dec=400, a_level=0.35, g_dlymix=0.1),
        wt("Bell", pos=0.7, wenv=-0.5, cut=8000, att=1, dec=1600, sus=0.0, rel=800, d_bits=0.35)), notes=STY)
add_p("CZ Brass", 1985, "Synth-Pop / New Wave", "Casio CZ-101", "Brass", "Casio CZ-101 - ottoni a distorsione di fase",
      pd(wave="Saw", amt=0.25, penv=0.55, dec=700, att=30, rel=250, uni=2, det=0.08, d_fa=60))
add_p("CZ Solid Bass", 1985, "Synth-Pop / New Wave", "Casio CZ-101", "Bass", "Casio CZ - basso PD risonante",
      pd(wave="Reso Saw", amt=0.4, penv=0.4, dec=300, sus=0.5, rel=80, d_oct=-1, g_mode="Mono"))
add_p("Fantasia", 1987, "Pop / Rock", "Roland D-50", "Pad", "Roland D-50 - preset di fabbrica (fine anni '80)",
      fm("7 Pair+2", [1, 2, 3.5, 7], [0.8, 0.45, 0.7, 0.55], [8000, 8000, 900, 400], [1.0, 1.0, 0.0, 0.0], vel=0.5,
         d_aa=30, d_ad=3000, d_as=0.9, d_ar=1800, d_uni=2, d_detune=0.06, g_chomode="Ensemble", g_chomix=0.5, g_revmix=0.4),
      original="Fantasia")
add_p("Pizzagogo", 1988, "Pop / Rock", "Roland D-50", "Pluck", "Enya - Orinoco Flow (1988)",
      fm("5 Two Pairs", [1, 2, 2, 5], [1.0, 0.5, 0.6, 0.45], [500, 200, 300, 90], [0, 0, 0, 0], vel=0.6, d_ad=700, d_as=0.0, d_ar=400,
         g_revmix=0.4, g_dlymix=0.15), original="Pizzagogo")
add_p("Digital Native Dance", 1987, "Electronic / Ambient", "Roland D-50", "Sequence", "Roland D-50 - preset di fabbrica (loop ritmico)",
      wt("Bit Steps", pos=0.4, cut=5000, att=1, dec=1200, sus=0.6, rel=600, d_lfoshape="S&H", d_lforate=8, d_wlfo=0.8, d_lfocut=0.35,
         g_dlymix=0.3, g_dlydiv="1/8", g_revmix=0.3), original="Digital Native Dance")
add_p("Staccato Heaven", 1987, "Pop / Rock", "Roland D-50", "Pluck", "Roland D-50 - preset di fabbrica",
      fm_pluck(r=(1, 2, 1, 1), d_level=0.9, g_revmix=0.35, g_dlymix=0.25), original="Staccato Heaven")
add_p("Synth-Pop Sync", 1981, "Synth-Pop / New Wave", "Sequential Prophet-5", "Lead", "Lead sync Prophet-5 (The Cars, primi anni '80)",
      sync_lead(semi=12, penv=0, cut=5000, a_o2semi=12, a_lfopitch=0, a_xmod=0.15), notes=STY)
add_p("Italo Bass", 1983, "Disco / Italo / Hi-NRG", "Roland Jupiter-8", "Bass", "Italo disco: basso in ottava (stile)",
      moog_bass(cut=900, res=0.15, env=0.35, dec=160, o2=0, fine=0, sus=0.0, a_ad=200, a_as=0.0, mono=False), notes=STY)
add_p("Hi-NRG Octave Bass", 1982, "Disco / Italo / Hi-NRG", "Sequential Prophet-5", "Bass", "Patrick Cowley / Bobby O - Hi-NRG (stile)",
      sh_bass(cut=1000, res=0.25, env=0.4, dec=140, sub=0.5, wave="Saw", g_mode="Poly", a_ad=180, a_as=0.0), notes=STY)
add_p("Italo Lead", 1984, "Disco / Italo / Hi-NRG", "Roland Juno-60", "Lead", "Italo disco lead (stile)",
      A(o1wave="Pulse", o1pw=0.3, pwm=0.3, o2lvl=0.0, flttype="LP24", cut=3500, res=0.1, aa=1, ad=300, **{"as": 0.8}, ar=200,
        lforate=0.8, g_chomode="II", g_dlymix=0.25, g_dlydiv="1/8D", g_revmix=0.2), notes=STY)
add_p("Electro Funk Bass", 1983, "Hip-Hop", "Roland SH-101", "Bass", "Electro-funk / old school hip-hop (stile)",
      sh_bass(cut=500, res=0.4, env=0.55, dec=220, sub=0.5, wave="Saw"), notes=STY)
add_p("Polysix Chords", 1982, "Synth-Pop / New Wave", "Korg Polysix", "Chord", "Korg Polysix - accordi ensemble (anni '80)",
      poly_stab(cut=1800, res=0.1, env=0.3, dec=900, chorus="Ensemble", wave="Pulse", o2="Pulse", a_o1pw=0.4, a_pwm=0.25))
add_p("MS-20 Squelch", 1982, "Synth-Pop / New Wave", "Korg MS-20", "Lead", "Korg MS-20 - lead con filtro saturo",
      lead(wave="Saw", cut=1100, res=0.75, env=0.45, a_flttype="LP12", a_drive=0.7, glide=40))
add_p("Synthex Lead", 1983, "Synth-Pop / New Wave", "Elka Synthex", "Lead", "Jean-Michel Jarre - Rendez-Vous (1986), stile",
      lead(wave="Saw", cut=3500, res=0.15, vib=0.25, glide=50, a_uni=3, a_detune=0.2, g_chomode="I", g_dlymix=0.25), notes=UNC)
add_p("ESQ Hybrid Pad", 1986, "Pop / Rock", "Ensoniq ESQ-1", "Pad", "Ensoniq ESQ-1: onde digitali + filtri analogici",
      L(poly_pad(wave="Saw", pwm=0.0, cut=1200, att=300, rel=1500, chorus="Off", a_level=0.5),
        wt("Vocal", pos=0.2, d_wlfo=0.3, d_lforate=0.3, cut=5000, att=300, dec=2000, sus=0.9, rel=1500, d_level=0.7), g_revmix=0.3))
add_p("DW-8000 Bell Pad", 1985, "Synth-Pop / New Wave", "Korg DW-8000", "Pad", "Korg DW-8000: forme d'onda digitali + VCF analogico",
      L(poly_pad(wave="Pulse", cut=1400, att=200, rel=1500, chorus="I", a_level=0.5),
        wt("Bell", pos=0.4, cut=6000, att=5, dec=2500, sus=0.5, rel=1500, d_level=0.6), g_revmix=0.3))
add_p("Prophet VS Vector", 1986, "Electronic / Ambient", "Sequential Prophet VS", "Pad", "Prophet VS: vector synthesis digitale + filtri analogici",
      L(poly_pad(wave="Saw", pwm=0.0, cut=1000, att=600, rel=2000, chorus="Off", a_level=0.45),
        wt("Analog Morph", pos=0.0, d_wlfo=1.0, d_lforate=0.15, d_lfoshape="Triangle", cut=7000, att=600, dec=3000, sus=1.0, rel=2000, d_level=0.7),
        g_revmix=0.35))

# =========================================================================================== 1980s house / techno / acid
add_p("Acid Tracks", 1987, "Acid", "Roland TB-303", "Bass", "Phuture - Acid Tracks (1987)",
      acid303(cut=260, res=0.88, env=0.7, dec=260, a_lfocut=0.0, g_dlymix=0.08))
add_p("Acid Square", 1988, "Acid", "Roland TB-303", "Bass", "Acid house 1988 - onda quadra (stile)",
      acid303(cut=380, res=0.8, env=0.55, dec=180, wave="Pulse"), notes=STY)
add_p("Can You Feel It Pad", 1986, "Deep House", "Roland Juno-60", "Pad", "Mr. Fingers (Larry Heard) - Can You Feel It (1986)",
      poly_pad(wave="Pulse", pw=0.45, pwm=0.35, cut=1700, res=0.12, env=0.15, att=350, rel=1500, chorus="II", sub=0.3))
add_p("Mystery of Love Bass", 1985, "Deep House", "Roland Juno-60", "Bass", "Mr. Fingers - Mystery of Love (1985), stile",
      sh_bass(cut=550, res=0.25, env=0.4, dec=350, sub=0.6, wave="Pulse", g_chomode="I", g_chomix=0.3), notes=UNC)
add_p("Your Love Arp", 1987, "House", "Roland Juno-106", "Sequence", "Frankie Knuckles & Jamie Principle - Your Love (1987), stile",
      pluck_a(cut=1600, res=0.2, dec=380, a_o1wave="Pulse", a_o2lvl=0.0, a_sub=0.3, g_chomode="I", g_chomix=0.5, g_dlymix=0.15, g_revmix=0.25), notes=UNC)
add_p("Just Want Another Chance", 1988, "Techno", "Casio CZ-5000", "Bass", "Reese (Kevin Saunderson) - Just Want Another Chance (1988): il 'Reese bass'",
      pd(wave="Saw", amt=0.55, uni=2, det=0.12, flt="LP24", cut=900, res=0.15, dec=2000, sus=1.0, rel=150, d_oct=-1, g_mode="Mono", d_env=0.15,
         d_lfoshape="Sine", d_lforate=0.3, d_lfotim=0.2))
add_p("Strings of Life Stab", 1987, "Techno", "Digital rompler", "Stab", "Rhythim Is Rhythim (Derrick May) - Strings of Life (1987)",
      add("Strings", bright=0.6, att=5, dec=420, sus=0.0, rel=300, d_bits=0.3, g_revmix=0.25), notes=UNC)
add_p("Big Fun Lead", 1988, "House", "Roland Juno-106", "Lead", "Inner City - Big Fun (1988), stile",
      A(o1wave="Pulse", o1pw=0.35, pwm=0.25, o2lvl=0.0, sub=0.2, flttype="LP24", cut=2600, res=0.2, env=0.3, fd=400, fs=0.5, aa=2,
        ad=400, **{"as": 0.8}, ar=250, lforate=0.5, g_chomode="I", g_dlymix=0.2, g_dlydiv="1/8D"), notes=UNC)
add_p("Good Life Pad", 1988, "House", "Roland Juno-106", "Chord", "Inner City - Good Life (1988), stile",
      poly_stab(cut=2000, res=0.1, env=0.3, dec=700, chorus="II", wave="Pulse", o2="Saw", a_pwm=0.3), notes=UNC)
add_p("Juno House Bass", 1987, "House", "Roland Juno-106", "Bass", "Chicago house 1986-88 - basso Juno (stile)",
      sh_bass(cut=700, res=0.3, env=0.45, dec=260, sub=0.8, wave="Pulse", g_chomode="Off"), notes=STY)
add_p("Chicago Organ Stab", 1988, "House", "Korg M1", "Organ", "House anni '80-'90: stab d'organo (stile)",
      add("House Organ", bright=0.6, tilt=0.6, att=1, dec=380, sus=0.3, rel=120), notes=STY)
add_p("French Kiss Sweep", 1989, "House", "Roland SH-101", "Bass", "Lil Louis - French Kiss (1989), stile",
      sh_bass(cut=300, res=0.6, env=0.3, dec=500, sub=0.3, wave="Saw", a_lfocut=0.6, a_lforate=0.06, a_lfoshape="Triangle"), notes=UNC)
add_p("Pacific State Lead", 1989, "House", "Roland Juno-106", "Lead", "808 State - Pacific State (1989), stile",
      A(o1wave="Pulse", o1pw=0.5, o2lvl=0.0, flttype="LP24", cut=4000, res=0.05, env=0.0, aa=30, ad=500, **{"as": 0.9}, ar=500,
        lforate=5.2, lfopitch=0.15, lfodelay=300, g_chomode="I", g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.4), notes=UNC)
add_p("Detroit Strings", 1988, "Techno", "Roland Juno-106", "Strings", "Detroit techno 1987-89 - archi sintetici (stile)",
      strings(cut=2500, att=250, rel=1200, chorus="II", g_dlymix=0.15), notes=STY)
add_p("Nude Photo Chord", 1987, "Techno", "Yamaha DX100", "Chord", "Rhythim Is Rhythim - Nude Photo (1987), stile",
      fm("5 Two Pairs", [1, 1, 2, 3], [1.0, 0.5, 0.6, 0.35], [900, 400, 700, 200], [0.2, 0.1, 0.1, 0.0], vel=0.5, d_ad=900, d_as=0.2,
         d_ar=300, g_revmix=0.2, g_dlymix=0.15), notes=UNC)
add_p("Lately Bass", 1987, "House", "Yamaha TX81Z", "Bass", "Il basso FM della house anni '90",
      fm_bass(alg="2 Y-Stack", r=(0.5, 0.5, 1.0, 1.0), l=(1.0, 0.68, 0.42, 0.38), e=(2500, 600, 300, 180), s=(0.55, 0.18, 0.0, 0.0), fb=0.55),
      original="Lately Bass")
add_p("Solid Bass", 1986, "House", "Yamaha TX81Z", "Bass", "DX21 / TX81Z - basso FM classico della house",
      fm_bass(alg="1 Stack", r=(0.5, 1.0, 0.5, 1.0), l=(1.0, 0.62, 0.48, 0.25), e=(1800, 380, 260, 150), s=(0.5, 0.12, 0.05, 0.0), fb=0.3),
      original="Solid Bass")
add_p("TX Wood Piano", 1988, "House", "Yamaha TX81Z", "Keys", "Piano FM 'legnoso' della house di fine anni '80 (stile)",
      fm("5 Two Pairs", [1, 1, 1, 5], [1.0, 0.5, 0.6, 0.35], [1800, 500, 900, 120], [0, 0, 0, 0], vel=0.7, d_ad=2200, d_as=0.0, d_ar=300), notes=STY)
add_p("Warehouse Organ", 1986, "House", "Korg Polysix", "Organ", "Chicago warehouse - organo synth (stile)",
      A(o1wave="Pulse", o1pw=0.5, o2wave="Pulse", o2oct=1, o2lvl=0.6, flttype="LP12", cut=3000, res=0.05, env=0.2, fd=200, fs=0.6, aa=1,
        ad=250, **{"as": 0.6}, ar=100, g_chomode="I", g_chomix=0.3), notes=STY)
add_p("Voodoo Ray Acid", 1988, "Acid", "Roland TB-303", "Bass", "A Guy Called Gerald - Voodoo Ray (1988), stile",
      acid303(cut=420, res=0.7, env=0.45, dec=200, a_drive=0.25), notes=STY)
add_p("Acid Trax Chord", 1988, "Acid", "Roland Juno-106", "Chord", "Acid house: accordo minore filtrato (stile)",
      poly_stab(cut=700, res=0.45, env=0.55, dec=300, chorus="I"), notes=STY)

# =========================================================================================== 1990s house
add_p("Organ 2", 1993, "House", "Korg M1", "Organ", "Robin S - Show Me Love (Stonebridge, 1993): basso d'organo",
      add("House Organ", bright=0.55, tilt=0.55, att=1, dec=900, sus=0.55, rel=120, d_oct=-1, g_mode="Poly"), original="Organ 2")
add_p("Organ 2 Stab", 1991, "House", "Korg M1", "Organ", "Crystal Waters - Gypsy Woman (1991), stile",
      add("House Organ", bright=0.6, tilt=0.6, att=1, dec=500, sus=0.35, rel=100), notes=UNC)
add_p("Piano 16'", 1990, "House", "Korg M1", "Piano", "Madonna - Vogue (1990); Black Box - Ride on Time (1989)",
      add("Piano", bright=0.72, tilt=0.4, inh=0.15, att=1, dec=4000, sus=0.0, rel=400, g_chomode="I", g_chomix=0.15),
      original="Piano 16'")
add_p("Ride on Time Piano", 1989, "Eurodance / 90s Dance", "Korg M1", "Piano", "Black Box - Ride on Time (1989)",
      add("Piano", bright=0.8, tilt=0.45, inh=0.2, att=1, dec=3500, sus=0.0, rel=350, g_chomode="I", g_chomix=0.2))
add_p("Universe", 1990, "House", "Korg M1", "Pad", "Korg M1 - pad di fabbrica (house/pop primi anni '90)",
      wt("Strings", pos=0.5, wenv=0.0, cut=4500, att=400, dec=3000, sus=1.0, rel=1800, uni=3, det=0.18, d_wlfo=0.25, d_lforate=0.2,
         g_chomode="Ensemble", g_chomix=0.4, g_revmix=0.35), original="Universe")
add_p("Lore", 1990, "Electronic / Ambient", "Korg M1", "Pad", "Korg M1 - pad di fabbrica",
      wt("Vocal", pos=0.1, d_wlfo=0.4, d_lforate=0.25, cut=5000, att=500, dec=3000, sus=1.0, rel=2200, uni=2, det=0.1,
         g_revmix=0.45, g_revsize=0.85), original="Lore")
add_p("M1 Choir", 1990, "House", "Korg M1", "Vox", "Coro M1 nella house dei primi anni '90 (stile)",
      add("Choir", bright=0.5, att=200, rel=1200, g_revmix=0.3), notes=STY)
add_p("MK Organ Bass", 1992, "House", "Korg M1", "Bass", "MK (Marc Kinchen) - dub anni '90, stile",
      add("House Organ", bright=0.45, tilt=0.7, att=1, dec=420, sus=0.25, rel=80, d_oct=-1, g_mode="Mono"), notes=UNC)
add_p("Kerri Deep Chord", 1994, "Deep House", "Roland Juno-106", "Chord", "Kerri Chandler - deep house anni '90 (stile)",
      poly_stab(cut=1300, res=0.18, env=0.35, dec=650, chorus="II", wave="Saw", o2="Pulse", a_pwm=0.2, g_revmix=0.2), notes=STY)
add_p("Deep Rhodes FM", 1993, "Deep House", "Yamaha DX7", "Keys", "Deep house anni '90 - e.piano FM con chorus (stile)",
      fm_epiano(bright=0.25, g_chomode="II", g_chomix=0.5, g_revmix=0.25), notes=STY)
add_p("NY Garage Organ", 1993, "House", "Korg M1", "Organ", "New York garage (Masters at Work, Todd Terry) - stile",
      add("Drawbar Organ", bright=0.5, att=1, dec=600, sus=0.8, rel=150, g_chomode="I", g_chomix=0.3), notes=STY)
add_p("Garage Sub Bass", 1994, "House", "Roland Juno-106", "Bass", "NY / UK garage - basso sub (stile)",
      sh_bass(cut=350, res=0.1, env=0.25, dec=400, sub=0.9, wave="Pulse", a_o1lvl=0.4), notes=STY)
add_p("French Touch Filter", 1997, "House", "Roland Juno-106", "Chord", "French touch - accordi filtrati (Daft Punk, Cassius, 1996-98), stile",
      poly_stab(cut=500, res=0.4, env=0.3, dec=1200, chorus="I", a_lfocut=0.6, a_lforate=0.12, a_drive=0.4, g_drive=0.2), notes=STY)
add_p("Da Funk Lead", 1995, "House", "Roland TB-303", "Lead", "Daft Punk - Da Funk (1995)",
      acid303(cut=700, res=0.55, env=0.4, dec=500, wave="Saw", drive=0.9, a_o1oct=0, g_drive=0.45, glide=40))
add_p("Around the World Bass", 1997, "House", "Roland SH-101", "Bass", "Daft Punk - Around the World (1997), stile",
      sh_bass(cut=400, res=0.35, env=0.6, dec=180, sub=0.6, wave="Saw", a_drive=0.4), notes=UNC)
add_p("Disco House Strings", 1998, "House", "ARP Solina String Ensemble", "Strings", "Filter disco / French touch - archi (stile)",
      strings(cut=3000, att=200, rel=900, a_lfocut=0.3, a_lforate=0.1), notes=STY)
add_p("Children Piano", 1995, "Trance", "Korg M1", "Piano", "Robert Miles - Children (1995), stile",
      add("Piano", bright=0.55, tilt=0.5, att=1, dec=4000, sus=0.0, rel=600, g_revmix=0.4, g_dlymix=0.2, g_dlydiv="1/8D"), notes=UNC)
add_p("Dream Lead", 1996, "Trance", "Roland JV-1080", "Lead", "Dream house 1995-96 (stile)",
      wt("Analog Morph", pos=0.7, cut=4500, att=20, dec=1000, sus=0.9, rel=500, uni=3, det=0.15, g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.4),
      notes=STY)
add_p("Insomnia Pluck", 1995, "Trance", "Digital VA", "Pluck", "Faithless - Insomnia (1995): riff pizzicato (preset 'Insomnia' presente su molti synth successivi)",
      wt("Analog Morph", pos=0.66, wenv=-0.3, cut=1800, res=0.3, env=0.55, att=1, dec=260, sus=0.0, rel=200, uni=2, det=0.1,
         d_fs=0.0, d_fd=220, g_dlymix=0.25, g_dlydiv="1/8D", g_revmix=0.25), notes=UNC)
add_p("Born Slippy Stab", 1996, "Techno", "Roland Juno-106", "Chord", "Underworld - Born Slippy .NUXX (1996), stile",
      poly_stab(cut=2200, res=0.1, env=0.3, dec=1500, chorus="I", g_dlymix=0.25, g_revmix=0.3), notes=UNC)
add_p("Garage Organ Bass 2-Step", 1999, "House", "Korg M1", "Bass", "UK garage / 2-step 1998-2000 (stile)",
      add("House Organ", bright=0.35, tilt=0.8, att=1, dec=300, sus=0.2, rel=60, d_oct=-1, g_mode="Mono", g_glide=40), notes=STY)
add_p("Speed Garage Bass", 1997, "House", "Roland Juno-106", "Bass", "Speed garage 1997 - basso 'wobble' (stile)",
      sh_bass(cut=380, res=0.45, env=0.35, dec=400, sub=0.6, wave="Saw", a_lfocut=0.5, a_lforate=4, a_drive=0.5, g_glide=60), notes=STY)
add_p("Funky House Clav", 1999, "House", "Yamaha DX7", "Keys", "Funky house fine anni '90 (stile)",
      fm("1 Stack", [1, 3, 1, 1], [1.0, 0.65, 0.35, 0.2], [500, 250, 120, 80], [0.2, 0.1, 0, 0], fb=0.35, vel=0.7, d_ad=700, d_as=0.3, d_ar=60,
         g_chomode="I"), notes=STY)
add_p("Hip House Stab", 1989, "House", "Roland D-50", "Stab", "Hip house 1989 (stile)",
      add("Brass", bright=0.7, att=1, dec=300, sus=0.0, rel=150, d_bits=0.2, g_revmix=0.15), notes=STY)

# =========================================================================================== 1990s techno / rave / eurodance / trance
add_p("What The", 1991, "Breakbeat / Rave", "Roland Alpha Juno-2", "Lead", "Joey Beltram / Second Phase - Mentasm (1991): l'hoover",
      hoover(), original="What The")
add_p("Mentasm Hoover", 1991, "Techno", "Roland Alpha Juno-2", "Lead", "Second Phase (Joey Beltram) - Mentasm (1991)",
      hoover(a_penv=-24, a_pdec=260, g_glide=240))
add_p("Dominator Hoover", 1991, "Breakbeat / Rave", "Roland Alpha Juno-2", "Lead", "Human Resource - Dominator (1991), stile",
      hoover(a_cut=6000, a_res=0.2, g_drive=0.3), notes=STY)
add_p("Charly Rave Stab", 1991, "Breakbeat / Rave", "Roland Alpha Juno-2", "Stab", "The Prodigy - Charly / Everybody in the Place (1991-92), stile",
      hoover(a_ad=350, a_as=0.0, a_ar=200, g_mode="Poly", g_glide=0, a_penv=-5, a_pdec=60), notes=UNC)
add_p("Rave Piano", 1992, "Breakbeat / Rave", "Korg M1", "Piano", "Rave / hardcore 1991-92 - piano house (stile)",
      add("Piano", bright=0.85, tilt=0.5, inh=0.25, att=1, dec=2500, sus=0.0, rel=300, g_revmix=0.15), notes=STY)
add_p("Energy Flash Acid", 1990, "Techno", "Roland TB-303", "Bass", "Joey Beltram - Energy Flash (1990), stile",
      acid303(cut=330, res=0.75, env=0.5, dec=180, a_drive=0.6, g_dlymix=0.15), notes=STY)
add_p("Acperience", 1992, "Acid", "Roland TB-303", "Bass", "Hardfloor - Acperience 1 (1992)",
      acid303(cut=240, res=0.9, env=0.75, dec=320, a_lfocut=0.25, a_lforate=0.05, a_drive=0.55, g_dlymix=0.2, g_dlydiv="1/8D"))
add_p("Higher State 303", 1995, "Acid", "Roland TB-303", "Bass", "Josh Wink - Higher State of Consciousness (1995)",
      acid303(cut=200, res=0.93, env=0.8, dec=150, a_lfocut=0.7, a_lforate=0.03, a_drive=0.7, g_drive=0.3))
add_p("Everybody Needs a 303", 1997, "Breakbeat / Rave", "Roland TB-303", "Bass", "Fatboy Slim - Everybody Needs a 303 (1997)",
      acid303(cut=450, res=0.72, env=0.55, dec=240, a_drive=0.5))
add_p("Sheet One 303", 1993, "Minimal", "Roland TB-303", "Bass", "Plastikman - Sheet One (1993), stile",
      acid303(cut=180, res=0.85, env=0.45, dec=120, a_drive=0.2, g_dlymix=0.25, g_dlydiv="1/16", g_dlyfb=0.6), notes=STY)
add_p("The Bells", 1996, "Techno", "Yamaha DX100", "Bell", "Jeff Mills - The Bells (1996)",
      fm("5 Two Pairs", [1, 3.5, 1, 2.0], [1.0, 0.55, 0.6, 0.5], [900, 600, 700, 400], [0, 0, 0, 0], vel=0.4, d_ad=1200, d_as=0.0, d_ar=400,
         g_revmix=0.15), notes="Strumento originale non documentato con certezza (citati DX100, K2000, campionatore): ricreato in FM.")
add_p("Detroit Chord Stab", 1995, "Techno", "Roland Juno-106", "Chord", "Detroit techno anni '90 - stab di accordo minore (stile)",
      poly_stab(cut=1800, res=0.15, env=0.4, dec=420, chorus="I", g_dlymix=0.25, g_dlydiv="1/8D", g_revmix=0.25), notes=STY)
add_p("Knights Jaguar Strings", 1999, "Techno", "Roland JV-1080", "Strings", "DJ Rolando / UR - Knights of the Jaguar (1999), stile",
      add("Strings", bright=0.55, att=120, dec=2000, sus=1.0, rel=900, d_uni=1, g_chomode="Ensemble", g_chomix=0.45, g_revmix=0.3), notes=UNC)
add_p("Minimal Nation Stab", 1994, "Minimal", "Roland SH-101", "Stab", "Robert Hood - Minimal Nation (1994), stile",
      A(o1wave="Pulse", o1pw=0.3, o2lvl=0.0, flttype="LP24", cut=1500, res=0.3, env=0.4, fd=90, fs=0.0, aa=0.5, ad=140, **{"as": 0.0}, ar=60,
        g_dlymix=0.2, g_dlydiv="1/8D"), notes=STY)
add_p("Dub Techno Chord", 1993, "Techno", "Analog poly", "Chord", "Basic Channel / Maurizio (1993-94): accordo dub (stile)",
      poly_stab(cut=600, res=0.3, env=0.45, dec=380, chorus="Off", a_lfocut=0.15, a_lforate=0.07, g_dlymix=0.45, g_dlyfb=0.7,
                g_dlydiv="1/4D", g_revmix=0.45, g_revsize=0.9), notes=STY)
add_p("Hard Techno Stab", 1997, "Techno", "Roland JP-8000", "Stab", "Techno di fine anni '90 - stab saturo (stile)",
      supersaw(det=0.3, mix=0.5, cut=3000, dec=200, sus=0.0, rel=150, d_env=0.4, g_drive=0.4, g_revmix=0.15), notes=STY)
add_p("Crispy Bacon Sweep", 1997, "Techno", "Roland MC-202", "Bass", "Laurent Garnier - Crispy Bacon (1997), stile",
      sh_bass(cut=250, res=0.7, env=0.4, dec=300, sub=0.3, wave="Saw", a_lfocut=0.8, a_lforate=0.04, a_drive=0.6, g_drive=0.25), notes=UNC)
add_p("Rush Acid 202", 1993, "Acid", "Roland MC-202", "Bass", "MC-202 acid / electro (anni '90)",
      acid303(cut=380, res=0.65, env=0.5, dec=220, wave="Pulse", a_flttype="LP24", a_sub=0.3))
add_p("Rhythm Dancer Lead", 1992, "Eurodance / 90s Dance", "Korg M1", "Lead", "Snap! - Rhythm Is a Dancer (1992), stile",
      wt("PPG Digital", pos=0.55, cut=6000, att=2, dec=800, sus=0.8, rel=300, uni=2, det=0.12, g_dlymix=0.2, g_revmix=0.25), notes=UNC)
add_p("What Is Love Stab", 1993, "Eurodance / 90s Dance", "Roland JD-800", "Stab", "Haddaway - What Is Love (1993), stile",
      wt("Analog Morph", pos=0.66, cut=3500, res=0.2, env=0.4, att=1, dec=350, sus=0.0, rel=200, uni=3, det=0.18, g_revmix=0.2), notes=UNC)
add_p("Eurodance Bass", 1994, "Eurodance / 90s Dance", "Korg M1", "Bass", "Eurodance 1993-95 - basso in ottava (stile)",
      fm_bass(alg="1 Stack", r=(0.5, 1.0, 1.0, 1.0), l=(1.0, 0.5, 0.2, 0.0), e=(800, 200, 120, 100), s=(0.3, 0.1, 0, 0), fb=0.4, d_ad=300, d_as=0.0,
              g_mode="Poly"), notes=STY)
add_p("Eurodance Piano", 1994, "Eurodance / 90s Dance", "Korg M1", "Piano", "Eurodance - piano house (2 Unlimited, Corona, Culture Beat), stile",
      add("Piano", bright=0.85, tilt=0.4, inh=0.2, att=1, dec=3000, sus=0.0, rel=300, g_chomode="I", g_chomix=0.2), notes=STY)
add_p("Get Ready Hoover", 1992, "Eurodance / 90s Dance", "Roland Alpha Juno-2", "Lead", "2 Unlimited - Get Ready for This (1991), stile",
      hoover(a_cut=5000, g_glide=120), notes=UNC)
add_p("Blue Da Ba Dee Lead", 1999, "Eurodance / 90s Dance", "Korg M1", "Lead", "Eiffel 65 - Blue (Da Ba Dee) (1999), stile",
      wt("Pulse Width", pos=0.3, cut=5000, att=1, dec=300, sus=0.6, rel=150, uni=2, det=0.1, g_dlymix=0.15), notes=UNC)
add_p("JP Supersaw", 1997, "Trance", "Roland JP-8000", "Lead", "Roland JP-8000 - forma d'onda Super Saw (trance 1997-2003)",
      supersaw(det=0.5, mix=0.7, cut=10000, dec=800), original="")
add_p("For an Angel Pluck", 1998, "Trance", "Roland JP-8000", "Pluck", "Paul van Dyk - For an Angel (1998), stile",
      supersaw(det=0.35, mix=0.55, cut=1500, att=1, dec=280, sus=0.0, rel=250, d_env=0.45, d_fd=250, d_fs=0.0, g_dlymix=0.3, g_dlydiv="1/8D"), notes=UNC)
add_p("Sandstorm Lead", 1999, "Trance", "Digital VA", "Lead", "Darude - Sandstorm (1999), stile",
      supersaw(det=0.28, mix=0.45, cut=5500, att=1, dec=150, sus=0.85, rel=80, d_env=0.25, g_drive=0.2, g_dlymix=0.15), notes=UNC)
add_p("9PM Lead", 1998, "Trance", "Access Virus", "Lead", "ATB - 9 PM (Till I Come) (1998), stile",
      wt("Analog Morph", pos=0.6, cut=3000, res=0.25, env=0.35, att=1, dec=300, sus=0.5, rel=200, uni=3, det=0.2, d_fs=0.2,
         g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.25, g_mode="Mono", g_glide=80), notes=UNC)
add_p("Goa Squelch", 1996, "Trance", "Clavia Nord Lead", "Sequence", "Goa / psytrance 1995-97 (stile)",
      wt("Reso Sweep", pos=0.3, wenv=0.6, cut=2500, res=0.5, env=0.45, att=1, dec=180, sus=0.1, rel=80, g_dlymix=0.25, g_dlydiv="1/16"), notes=STY)
add_p("Nord Lead Trance Pad", 1998, "Trance", "Clavia Nord Lead", "Pad", "Pad trance fine anni '90 (stile)",
      supersaw(det=0.4, mix=0.6, cut=3500, att=600, rel=1800, dec=2000, d_lfocut=0.2, d_lforate=0.15, g_revmix=0.4), notes=STY)
add_p("Gated Trance Pad", 1999, "Trance", "Access Virus", "Pad", "Trance 1999-2001 - pad 'gated' (stile)",
      supersaw(det=0.45, mix=0.65, cut=6000, att=2, dec=2000, rel=200, d_lfoshape="Square", d_lforate=8.0, d_lfocut=0.6, g_revmix=0.25), notes=STY)
add_p("Big Beat Acid", 1997, "Breakbeat / Rave", "Roland TB-303", "Bass", "Big beat 1996-98 (Chemical Brothers, Prodigy), stile",
      acid303(cut=500, res=0.7, env=0.5, dec=200, drive=0.9, g_drive=0.5), notes=STY)
add_p("Jungle Reese", 1995, "Drum & Bass", "Casio CZ-101", "Bass", "Jungle / drum & bass 1994-97: Reese (stile)",
      pd(wave="Saw", amt=0.6, uni=3, det=0.18, flt="LP24", cut=700, res=0.2, dec=2000, sus=1.0, rel=150, d_oct=-1, g_mode="Mono",
         d_lfocut=0.25, d_lforate=0.2), notes=STY)
add_p("Jungle Sub", 1994, "Drum & Bass", "Roland SH-101", "Bass", "Jungle - sub bass sinusoidale con glide (stile)",
      A(o1wave="Sine", o1oct=-1, o2lvl=0.0, flttype="LP12", cut=2000, env=0.0, aa=2, ad=1000, **{"as": 1.0}, ar=120, penv=-3, pdec=60,
        g_mode="Mono", g_glide=90), notes=STY)
add_p("Hardcore Stab", 1992, "Breakbeat / Rave", "Roland JD-800", "Stab", "Hardcore rave 1992 - stab di accordo (stile)",
      wt("Analog Morph", pos=0.66, cut=4000, res=0.15, env=0.35, att=1, dec=300, sus=0.0, rel=200, uni=4, det=0.25, d_bits=0.25), notes=STY)

# =========================================================================================== 2000s
add_p("Satisfaction Bass", 2002, "Electro / Electroclash", "Software synth", "Bass", "Benny Benassi - Satisfaction (2002), stile",
      wt("Pulse Width", pos=0.35, cut=900, res=0.45, env=0.45, att=1, dec=400, sus=0.6, rel=80, d_oct=-1, d_drive=None, g_drive=0.65, g_mode="Mono")
      if False else D(engine="Wavetable", wt="Pulse Width", wpos=0.35, flttype="LP24", cut=900, res=0.45, env=0.45, fd=400, fs=0.3, aa=1, ad=400,
                      **{"as": 0.7}, ar=80, oct=-1, g_drive=0.7, g_mode="Mono"), notes=UNC)
add_p("Electroclash Buzz", 2003, "Electro / Electroclash", "Korg MS-20", "Bass", "Electroclash 2001-04 (Fischerspooner, Miss Kittin), stile",
      sh_bass(cut=900, res=0.55, env=0.4, dec=250, sub=0.2, wave="Saw", a_flttype="LP12", a_drive=0.8, g_drive=0.3), notes=STY)
add_p("D.A.N.C.E. Bass", 2007, "Electro / Electroclash", "Software synth", "Bass", "Justice - electro 2007, stile",
      wt("Growl", pos=0.4, wenv=0.3, cut=1500, res=0.3, env=0.4, att=1, dec=250, sus=0.4, rel=80, d_oct=-1, g_drive=0.6, g_mode="Mono"), notes=STY)
add_p("Virus Hypersaw", 2004, "Trance", "Access Virus", "Lead", "Access Virus - Hypersaw (trance anni 2000)",
      supersaw(det=0.6, mix=0.75, cut=9000, dec=900, d_lfopitch=0.05, d_lforate=5))
add_p("Uplifting Trance Lead", 2005, "Trance", "Access Virus", "Lead", "Uplifting trance anni 2000 (stile)",
      supersaw(det=0.55, mix=0.7, cut=7000, dec=1000, g_dlymix=0.25, g_dlydiv="1/8D", g_revmix=0.35), notes=STY)
add_p("Progressive Pluck", 2006, "Progressive House", "Access Virus", "Pluck", "Progressive house anni 2000 (stile)",
      wt("Analog Morph", pos=0.66, cut=1500, res=0.25, env=0.55, att=1, dec=220, sus=0.0, rel=200, uni=2, det=0.12, d_fs=0.0, d_fd=200,
         g_dlymix=0.35, g_dlydiv="1/8D", g_revmix=0.3), notes=STY)
add_p("Strobe Pluck", 2009, "Progressive House", "LennarDigital Sylenth1", "Pluck", "deadmau5 - Strobe (2009), stile",
      wt("Analog Morph", pos=0.66, cut=1100, res=0.3, env=0.5, att=1, dec=400, sus=0.0, rel=400, uni=3, det=0.15, d_fs=0.0, d_fd=350,
         g_dlymix=0.35, g_dlydiv="1/8D", g_revmix=0.4), notes=UNC)
add_p("Minimal Blip", 2005, "Minimal", "Software synth", "Pluck", "Minimal techno 2004-08 (Richie Hawtin, Ricardo Villalobos), stile",
      fm("1 Stack", [1, 2, 1, 1], [1.0, 0.4, 0.0, 0.0], [120, 60, 50, 50], [0, 0, 0, 0], vel=0.5, d_ad=150, d_as=0.0, d_ar=80,
         g_dlymix=0.3, g_dlydiv="1/8D", g_dlyfb=0.5), notes=STY)
add_p("Minimal Click Bass", 2006, "Minimal", "Software synth", "Bass", "Minimal / microhouse - basso 'click' (stile)",
      fm_bass(alg="1 Stack", r=(1.0, 1.0, 1, 1), l=(1.0, 0.35, 0, 0), e=(400, 40, 50, 50), s=(0.3, 0.0, 0, 0), fb=0.0, d_oct=-1, d_ad=300, d_as=0.0),
      notes=STY)
add_p("Rominimal Sub", 2010, "Minimal", "Analog mono", "Bass", "Rominimal (Rhadoo, Petre Inspirescu) - sub rotondo (stile)",
      A(o1wave="Triangle", o1oct=-1, o2lvl=0.0, sub=0.4, flttype="LP24", cut=300, res=0.1, env=0.2, fd=200, fs=0.2, aa=2, ad=500, **{"as": 0.4},
        ar=100, g_mode="Mono"), notes=STY)
add_p("Microhouse Pluck", 2002, "Minimal", "Software synth", "Pluck", "Microhouse 2000-03 (Akufen, Isolée), stile",
      fm_pluck(r=(1, 5, 1, 1), d_level=0.8, g_dlymix=0.3), notes=STY)
add_p("Tech House Organ Stab", 2004, "Tech House", "Korg M1", "Organ", "Tech house anni 2000 - stab d'organo (stile)",
      add("House Organ", bright=0.5, tilt=0.7, att=1, dec=250, sus=0.15, rel=80, g_dlymix=0.2, g_dlydiv="1/8D"), notes=STY)
add_p("Tech House Rolling Bass", 2005, "Tech House", "Roland SH-101", "Bass", "Tech house londinese 2000s (Terry Francis, Mr. C), stile",
      sh_bass(cut=320, res=0.35, env=0.5, dec=160, sub=0.7, wave="Saw"), notes=STY)
add_p("Hey Hey Bass", 2009, "Deep House", "Software synth", "Bass", "Dennis Ferrer - Hey Hey (2009), stile",
      fm_bass(alg="2 Y-Stack", r=(0.5, 0.5, 1, 1), l=(1.0, 0.5, 0.3, 0.2), e=(1500, 400, 200, 150), s=(0.5, 0.15, 0, 0), fb=0.3), notes=UNC)
add_p("Electro House Fidget", 2008, "Electro / Electroclash", "NI Massive", "Bass", "Electro / fidget house 2007-09, stile",
      wt("Growl", pos=0.5, wenv=0.35, cut=2500, res=0.35, env=0.4, att=1, dec=200, sus=0.5, rel=60, uni=2, det=0.1, d_oct=-1,
         d_lfotim=0.6, d_lforate=6, g_drive=0.5, g_mode="Mono"), notes=STY)
add_p("Wobble Bass", 2009, "Dubstep / Bass", "NI Massive", "Bass", "Dubstep 2008-11 - wobble (stile)",
      wt("Growl", pos=0.3, cut=500, res=0.4, env=0.0, att=1, dec=1000, sus=1.0, rel=80, uni=2, det=0.1, d_oct=-1, d_lfoshape="Sine",
         d_lforate=3.0, d_lfocut=0.8, d_wlfo=0.6, g_drive=0.5, g_mode="Mono"), notes=STY)
add_p("Scary Monsters Growl", 2010, "Dubstep / Bass", "NI Massive", "Bass", "Skrillex - Scary Monsters and Nice Sprites (2010), stile",
      wt("Growl", pos=0.7, wenv=0.3, cut=3500, res=0.45, env=0.35, att=1, dec=300, sus=0.8, rel=60, uni=3, det=0.15, d_oct=-1, d_lforate=8,
         d_lfoshape="S&H", d_lfotim=0.7, d_lfocut=0.4, g_drive=0.7, g_mode="Mono"), notes=UNC)
add_p("Neuro Reese", 2007, "Drum & Bass", "NI Massive", "Bass", "Neurofunk 2005-10 - reese (stile)",
      wt("Growl", pos=0.2, cut=1200, res=0.3, env=0.1, att=1, dec=1000, sus=1.0, rel=100, uni=4, det=0.3, d_oct=-1, d_lfocut=0.3, d_lforate=0.3,
         g_drive=0.45, g_mode="Mono"), notes=STY)
add_p("Liquid DnB Pad", 2008, "Drum & Bass", "Roland Juno-106", "Pad", "Liquid drum & bass (stile)",
      poly_pad(wave="Saw", pwm=0.0, cut=2000, att=600, rel=2500, chorus="II", g_revmix=0.4), notes=STY)
add_p("Call On Me Filter", 2004, "House", "Roland Juno-106", "Chord", "Filter house 2004 (stile)",
      poly_stab(cut=600, res=0.45, env=0.4, dec=1000, chorus="I", a_lfocut=0.7, a_lforate=0.05, g_drive=0.3), notes=STY)
add_p("Big Room Stab", 2009, "EDM / Big Room", "LennarDigital Sylenth1", "Stab", "Electro / progressive house 2008-10 (stile)",
      supersaw(det=0.5, mix=0.7, cut=8000, dec=300, sus=0.0, rel=200, d_env=0.25, g_revmix=0.25), notes=STY)
add_p("Cafe del Mar Pad", 2001, "Electronic / Ambient", "Korg Triton", "Pad", "Chill-out / Ibiza anni 2000 (stile)",
      wt("Vocal", pos=0.4, d_wlfo=0.5, d_lforate=0.12, cut=4000, att=800, dec=3000, sus=1.0, rel=3000, uni=3, det=0.12, g_revmix=0.5,
         g_chomode="Ensemble"), notes=STY)
add_p("Noisy Stabber", 2000, "EDM / Big Room", "Korg Triton", "Stab", "Korg Triton - stab di fabbrica (dance anni 2000)",
      wt("Bit Steps", pos=0.4, cut=6000, res=0.2, env=0.3, att=1, dec=280, sus=0.0, rel=150, uni=3, det=0.25, d_noise=0.2), original="Noisy Stabber",
      notes="Nome del preset di fabbrica riportato dalla comunità Triton; suono ricreato.")
add_p("Daft Vocoder Lead", 2001, "House", "Roland SH-101", "Vox", "French house / filter disco - lead 'vocale' (stile)",
      A(o1wave="Saw", o2wave="Pulse", o2lvl=0.5, flttype="BP12", cut=1000, res=0.6, env=0.2, lfocut=0.4, lforate=0.3, aa=2, ad=500, **{"as": 0.9},
        ar=150, g_mode="Mono", g_glide=50, g_chomode="I"), notes=STY)

# =========================================================================================== 2010s
add_p("Levels Lead", 2011, "EDM / Big Room", "LennarDigital Sylenth1", "Lead", "Avicii - Levels (2011), stile",
      supersaw(det=0.45, mix=0.6, cut=6000, dec=250, sus=0.7, rel=200, d_env=0.25, d_fd=250, g_revmix=0.3, g_dlymix=0.15), notes=UNC)
add_p("Animals Drop", 2013, "EDM / Big Room", "LennarDigital Sylenth1", "Lead", "Martin Garrix - Animals (2013), stile",
      wt("Pulse Width", pos=0.5, cut=3000, res=0.2, env=0.35, att=1, dec=150, sus=0.6, rel=80, uni=4, det=0.25, g_drive=0.4, g_revmix=0.25,
         d_oct=-1), notes=UNC)
add_p("Opus Arp", 2015, "Progressive House", "Software synth", "Sequence", "Eric Prydz - Opus (2015), stile",
      supersaw(det=0.35, mix=0.55, cut=3000, att=1, dec=200, sus=0.1, rel=200, d_env=0.4, g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.45), notes=UNC)
add_p("Hungry Bass", 2011, "Tech House", "Software synth", "Bass", "Jamie Jones - Hungry for the Power (2011), stile",
      fm_bass(alg="1 Stack", r=(0.5, 1.0, 1, 1), l=(1.0, 0.45, 0.2, 0), e=(900, 300, 200, 100), s=(0.4, 0.1, 0, 0), fb=0.25, d_lfotim=0.0), notes=UNC)
add_p("Be Sharp Bass", 2014, "Tech House", "Software synth", "Bass", "Patrick Topping - Be Sharp Say Nowt (2014), stile",
      wt("Analog Morph", pos=0.6, cut=600, res=0.4, env=0.45, att=1, dec=180, sus=0.2, rel=60, d_oct=-1, g_mode="Mono", g_drive=0.25), notes=UNC)
add_p("Cola Bass", 2017, "Tech House", "Software synth", "Bass", "CamelPhat & Elderbrook - Cola (2017), stile",
      wt("Analog Morph", pos=0.66, cut=450, res=0.3, env=0.35, att=1, dec=300, sus=0.5, rel=80, d_oct=-1, uni=2, det=0.08, g_mode="Mono",
         d_lfocut=0.2, d_lforate=0.2), notes=UNC)
add_p("Losing It Bass", 2018, "Tech House", "Xfer Serum", "Bass", "FISHER - Losing It (2018), stile",
      wt("Growl", pos=0.25, wenv=0.25, cut=700, res=0.15, env=0.4, att=1, dec=160, sus=0.35, rel=60, d_oct=-1, g_drive=0.45, g_mode="Mono"),
      notes="Synth originale non documentato: basso tech house ricreato nello stile del brano.")
add_p("Turn Off the Lights Bass", 2018, "Tech House", "Xfer Serum", "Bass", "Chris Lake - tech house 2018-19 (stile)",
      wt("Reso Sweep", pos=0.25, wenv=0.4, cut=900, res=0.3, env=0.35, att=1, dec=140, sus=0.25, rel=60, d_oct=-1, g_mode="Mono", g_drive=0.3), notes=STY)
add_p("Latch Chords", 2013, "House", "Roland Juno-106", "Chord", "Disclosure - Latch / UK house 2013 (stile)",
      poly_stab(cut=1600, res=0.15, env=0.4, dec=450, chorus="II", g_revmix=0.2), notes=UNC)
add_p("UK House Organ Bass", 2013, "House", "Korg M1", "Bass", "UK house revival 2012-14 (Disclosure, Duke Dumont, MK) - organ bass",
      add("House Organ", bright=0.5, tilt=0.65, att=1, dec=550, sus=0.35, rel=90, d_oct=-1, g_mode="Mono"), notes=STY)
add_p("Deep House Pluck", 2014, "Deep House", "Software synth", "Pluck", "Deep house 2013-15 (stile)",
      fm_pluck(r=(1, 2, 1, 1), d_l2=0.45, g_dlymix=0.3, g_revmix=0.3, g_chomode="I"), notes=STY)
add_p("Deep House Organ Chord", 2014, "Deep House", "Korg M1", "Chord", "Deep house 2013-15 - accordi d'organo (stile)",
      add("Drawbar Organ", bright=0.35, tilt=0.6, att=5, dec=800, sus=0.4, rel=300, d_flttype="LP12", d_cut=2000, g_chomode="I", g_revmix=0.25), notes=STY)
add_p("Powers of Ten Lead", 2015, "Melodic Techno", "Moog Sub 37", "Lead", "Stephan Bodzin - Powers of Ten (2015)",
      lead(wave="Saw", cut=900, res=0.45, env=0.55, glide=120, a_fd=900, a_fs=0.3, g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.35))
add_p("Melodic Techno Arp", 2017, "Melodic Techno", "Moog Sub 37", "Sequence", "Melodic techno (Afterlife, Tale Of Us), stile",
      pluck_a(cut=600, res=0.5, env=0.6, dec=220, a_o2wave="Saw", g_dlymix=0.35, g_revmix=0.4), notes=STY)
add_p("Afterlife Pad", 2018, "Melodic Techno", "Moog One", "Pad", "Melodic techno - pad cinematico (stile)",
      poly_pad(wave="Saw", pwm=0.0, cut=900, res=0.25, att=1200, rel=3000, chorus="Off", a_uni=3, a_detune=0.3, g_revmix=0.55, g_revsize=0.92), notes=STY)
add_p("Melodic Techno Bass", 2019, "Melodic Techno", "Moog Sub 37", "Bass", "Melodic techno - basso rotolante (stile)",
      moog_bass(cut=300, res=0.35, env=0.5, dec=200, sus=0.1), notes=STY)
add_p("Kolsch Plucks", 2016, "Melodic Techno", "Software synth", "Pluck", "Kölsch / Kompakt - pluck melodico (stile)",
      wt("Hollow", pos=0.5, cut=2500, res=0.2, env=0.4, att=1, dec=400, sus=0.0, rel=400, d_fs=0.0, g_dlymix=0.3, g_revmix=0.4), notes=STY)
add_p("Nightcall Bass", 2010, "Synthwave", "Analog mono", "Bass", "Kavinsky - Nightcall (2010), stile",
      moog_bass(cut=600, res=0.2, env=0.4, dec=500, sus=0.4, a_chomode=None) if False else moog_bass(cut=600, res=0.2, env=0.4, dec=500, sus=0.4),
      notes=STY)
add_p("Stranger Things Arp", 2016, "Synthwave", "Sequential Prophet-5", "Sequence", "S U R V I V E - Stranger Things theme (2016), stile",
      pluck_a(cut=900, res=0.4, env=0.5, dec=260, a_o2wave="Saw", g_dlymix=0.25, g_revmix=0.35), notes=STY)
add_p("Synthwave Brass", 2014, "Synthwave", "Roland Jupiter-8", "Brass", "Synthwave / outrun 2010s (stile)",
      brass(cut=1400, env=0.45, att=20, dec=900, sus=0.65, chorus="I", g_revmix=0.35, g_dlymix=0.2), notes=STY)
add_p("Hardstyle Screech", 2012, "EDM / Big Room", "Software synth", "Lead", "Hardstyle 2010s - lead distorto (stile)",
      supersaw(det=0.3, mix=0.6, cut=5000, att=1, dec=300, sus=0.8, rel=100, g_drive=0.8, d_penv=-7, d_pdec=80), notes=STY)
add_p("Future Bass Chord", 2016, "EDM / Big Room", "Xfer Serum", "Chord", "Future bass 2015-17 (stile)",
      supersaw(det=0.4, mix=0.6, cut=7000, att=30, dec=500, sus=0.8, rel=300, d_lfoshape="Saw Down", d_lforate=4, d_lfocut=0.0,
               d_level=0.8, g_revmix=0.3), notes=STY)
add_p("Tropical Pluck", 2015, "EDM / Big Room", "Software synth", "Pluck", "Tropical house 2014-16 (stile)",
      add("Mallet", bright=0.6, att=1, dec=600, sus=0.0, rel=300, g_dlymix=0.25, g_revmix=0.3), notes=STY)

# =========================================================================================== 2020s
add_p("Blinding Lights Lead", 2019, "Synthwave", "Analog poly", "Lead", "The Weeknd - Blinding Lights (2019), stile",
      A(o1wave="Pulse", o1pw=0.35, pwm=0.2, o2wave="Saw", o2lvl=0.4, o2fine=8, flttype="LP24", cut=3200, res=0.1, env=0.2, aa=2, ad=500, **{"as": 0.8},
        ar=300, g_chomode="I", g_dlymix=0.25, g_dlydiv="1/8D", g_revmix=0.35), notes=UNC)
add_p("Nanana Piano", 2023, "House", "Korg M1", "Piano", "Peggy Gou - (It Goes Like) Nanana (2023): revival del piano house anni '90",
      add("Piano", bright=0.78, tilt=0.45, inh=0.18, att=1, dec=3200, sus=0.0, rel=350, g_chomode="I", g_chomix=0.15, g_revmix=0.12), notes=UNC)
add_p("90s Revival Organ", 2022, "House", "Korg M1", "Organ", "House revival anni 2020 - organo M1 (stile)",
      add("House Organ", bright=0.6, tilt=0.5, att=1, dec=700, sus=0.45, rel=110), notes=STY)
add_p("Where You Are Bass", 2023, "Tech House", "Xfer Serum", "Bass", "John Summit - tech house 2023, stile",
      wt("Analog Morph", pos=0.7, cut=520, res=0.3, env=0.5, att=1, dec=150, sus=0.25, rel=60, d_oct=-1, g_mode="Mono", g_drive=0.3), notes=UNC)
add_p("Drugs from Amsterdam Bass", 2022, "Tech House", "Xfer Serum", "Bass", "Mau P - Drugs from Amsterdam (2022), stile",
      wt("Growl", pos=0.35, wenv=0.45, cut=800, res=0.2, env=0.35, att=1, dec=120, sus=0.3, rel=50, d_oct=-1, g_mode="Mono", g_drive=0.5), notes=UNC)
add_p("Rhyme Dust Bass", 2023, "Tech House", "Xfer Serum", "Bass", "Dom Dolla - tech house 2023, stile",
      wt("Pulse Width", pos=0.4, wenv=0.25, cut=650, res=0.35, env=0.45, att=1, dec=140, sus=0.3, rel=50, d_oct=-1, g_mode="Mono", g_drive=0.35), notes=UNC)
add_p("Michael Bibi Bass", 2021, "Tech House", "Software synth", "Bass", "Tech house 2020-22 (Michael Bibi, Solardo), stile",
      fm_bass(alg="2 Y-Stack", r=(0.5, 1.0, 0.5, 2.0), l=(1.0, 0.5, 0.35, 0.2), e=(700, 220, 150, 90), s=(0.35, 0.1, 0.0, 0.0), fb=0.4,
              g_drive=0.25), notes=STY)
add_p("Move Afro Pluck", 2024, "Deep House", "Software synth", "Pluck", "Adam Port / Keinemusik - Move (2024): afro house, stile",
      add("Mallet", bright=0.45, att=1, dec=500, sus=0.0, rel=250, g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.3), notes=UNC)
add_p("Afro House Pad", 2022, "Deep House", "Moog One", "Pad", "Afro / organic house 2020s (stile)",
      poly_pad(wave="Saw", pwm=0.0, cut=1200, att=900, rel=2500, chorus="II", a_lfocut=0.2, a_lforate=0.1, g_revmix=0.45), notes=STY)
add_p("Acid Revival 303", 2021, "Acid", "Roland TB-303", "Bass", "Charlotte de Witte / Amelie Lens - acid techno 2020s, stile",
      acid303(cut=300, res=0.86, env=0.65, dec=200, drive=0.8, g_drive=0.4, g_dlymix=0.15), notes=STY)
add_p("Peak Time Rave Stab", 2022, "Techno", "Roland Alpha Juno-2", "Stab", "Peak time / rave techno 2020s (I Hate Models, Kobosil), stile",
      hoover(a_ad=500, a_as=0.0, a_ar=300, g_mode="Poly", g_glide=0, a_penv=-3, a_pdec=40, g_revmix=0.3, g_drive=0.3), notes=STY)
add_p("Hardgroove Organ", 2023, "Techno", "Korg M1", "Organ", "Hardgroove / techno 2020s - stab d'organo (stile)",
      add("House Organ", bright=0.65, tilt=0.75, att=1, dec=200, sus=0.1, rel=60, g_drive=0.3, g_dlymix=0.2, g_dlydiv="1/8D"), notes=STY)
add_p("Eurotrance Revival", 2023, "Trance", "Software synth", "Lead", "Trance revival 2022-24 (stile)",
      supersaw(det=0.5, mix=0.7, cut=9000, dec=700, g_dlymix=0.25, g_dlydiv="1/8D", g_revmix=0.35), notes=STY)
add_p("Fred Again Chord", 2022, "House", "Analog poly", "Chord", "UK house / 'emotional' 2020s - accordi morbidi (stile)",
      poly_stab(cut=2200, res=0.05, env=0.15, dec=1500, chorus="I", wave="Triangle", o2="Saw", a_o2lvl=0.3, g_revmix=0.45), notes=STY)
add_p("Disco Pop Bass", 2020, "Disco / Italo / Hi-NRG", "Analog mono", "Bass", "Nu-disco pop 2020 (es. Dua Lipa - Don't Start Now), stile",
      moog_bass(cut=900, res=0.15, env=0.35, dec=180, o2=0, fine=6, sus=0.3), notes=UNC)
add_p("Hyperpop Bell", 2021, "Pop / Rock", "Software synth", "Bell", "Hyperpop 2020s (stile)",
      fm_bell(r2=5.0, d_bits=0.3, g_drive=0.3), notes=STY)
add_p("Melodic Techno 2020s Lead", 2024, "Melodic Techno", "Software synth", "Lead", "Melodic techno 2023-24 (Anyma, Adriatique), stile",
      wt("Hollow", pos=0.7, wenv=0.2, cut=2500, res=0.25, env=0.35, att=5, dec=800, sus=0.7, rel=500, uni=3, det=0.18, g_dlymix=0.3,
         g_revmix=0.45, g_mode="Mono", g_glide=90), notes=STY)
add_p("G-Funk Whistle", 1992, "Hip-Hop", "Moog Minimoog Model D", "Lead", "Dr. Dre - G-funk 1992-93 (Nuthin' but a 'G' Thang): il 'whistle' Minimoog",
      lead(wave="Sine", o2lvl=0.0, cut=6000, res=0.0, env=0.0, glide=140, vib=0.3, a_lforate=5.5, a_drive=0.0, g_revmix=0.2))
add_p("G-Funk Bass", 1993, "Hip-Hop", "Moog Minimoog Model D", "Bass", "G-funk 1992-94 (stile)",
      moog_bass(cut=350, res=0.15, env=0.35, dec=500, sus=0.5, glide=60), notes=STY)
add_p("Trap 808 Glide", 2015, "Hip-Hop", "Software synth", "Bass", "Trap - basso 808 con glide (stile)",
      add("Soft Pad", bright=0.2, att=1, dec=2500, sus=0.0, rel=200, d_oct=-1, d_penv=-2, d_pdec=50, g_mode="Legato", g_glide=120, g_drive=0.35),
      notes=STY)

# =========================================================================================== DANCE / HOUSE / TECHNO / TECH HOUSE palette
# Extra genre presets built on the instruments that defined each scene (marked as styles, not factory presets).
dance_extra = [
    # (name, year, genre, synth, cat, ref, params)
    ("Jack Your Body Bass", 1986, "House", "Roland Juno-106", "Bass", "Chicago jack 1986 (stile)", sh_bass(cut=650, res=0.3, env=0.5, dec=200, sub=0.6)),
    ("Chicago Piano Stab", 1988, "House", "Yamaha DX7", "Piano", "Chicago house 1987-89 - piano FM (stile)",
     fm("5 Two Pairs", [1, 1, 1, 7], [1.0, 0.55, 0.7, 0.4], [1500, 500, 900, 120], [0, 0, 0, 0], vel=0.7, d_ad=1800, d_as=0.0, d_ar=250)),
    ("Classic House Sax-ish", 1990, "House", "Korg M1", "Lead", "Garage / house 1990 - lead 'sax' sintetico (stile)",
     add("Brass", bright=0.55, att=20, dec=1000, sus=0.85, rel=150, d_lfopitch=0.12, d_lforate=5.5, d_lfodelay=300)),
    ("House Stab Minor 7", 1992, "House", "Korg M1", "Stab", "House anni '90 - stab di accordo min7 (stile)",
     add("Drawbar Organ", bright=0.65, att=1, dec=280, sus=0.0, rel=120, d_flttype="LP12", d_cut=5000)),
    ("Classic House Strings", 1991, "House", "Korg M1", "Strings", "House primi anni '90 - archi M1 (stile)",
     add("Strings", bright=0.5, att=150, dec=2000, sus=1.0, rel=700, g_chomode="Ensemble", g_chomix=0.4, g_revmix=0.25)),
    ("Disco Loop Bass", 1999, "House", "Moog Minimoog Model D", "Bass", "Disco house 1997-2000 - basso in ottava (stile)",
     moog_bass(cut=700, res=0.2, env=0.4, dec=200, o2=0, fine=0, sus=0.3, mono=False)),
    ("Deep Sub Pluck", 1996, "Deep House", "Roland SH-101", "Bass", "Deep house anni '90 - basso morbido (stile)",
     sh_bass(cut=380, res=0.15, env=0.35, dec=350, sub=0.8, wave="Pulse", a_o1lvl=0.5)),
    ("Deep Warm Pad", 1995, "Deep House", "Roland Juno-106", "Pad", "Deep house anni '90 - pad caldo (stile)",
     poly_pad(cut=1300, att=500, rel=1800, chorus="II")),
    ("Deep Jazzy Chords", 1997, "Deep House", "Yamaha DX7", "Keys", "Deep house jazzy (St Germain, Moodymann) - e.piano (stile)",
     fm_epiano(bright=0.3, g_chomode="I", g_chomix=0.45, g_revmix=0.3)),
    ("Lo-Fi House Chord", 2016, "Deep House", "Roland Juno-106", "Chord", "Lo-fi house 2015-17 (stile)",
     poly_stab(cut=1100, res=0.2, env=0.3, dec=800, chorus="II", a_drift=0.6, g_drive=0.3)),
    ("Deep Tech Organ Bass", 2012, "Deep House", "Korg M1", "Bass", "Deep tech 2011-13 (stile)",
     add("House Organ", bright=0.3, tilt=0.8, att=1, dec=350, sus=0.2, rel=70, d_oct=-1, d_flttype="LP12", d_cut=1500, g_mode="Mono")),
    ("Tech House Vocal Formant", 2016, "Tech House", "Xfer Serum", "Vox", "Tech house 2015-18 - formante vocale (stile)",
     wt("Vocal", pos=0.3, wenv=0.4, cut=4000, res=0.2, env=0.2, att=1, dec=200, sus=0.3, rel=80, d_oct=-1, g_mode="Mono")),
    ("Tech House Plucky FM", 2017, "Tech House", "Yamaha TX81Z", "Bass", "Tech house - basso FM corto (stile)",
     fm_bass(alg="1 Stack", r=(0.5, 1.5, 1, 1), l=(1.0, 0.55, 0.2, 0), e=(500, 160, 100, 80), s=(0.25, 0.05, 0, 0), fb=0.3, d_ad=250, d_as=0.2)),
    ("Tech House Stab Chord", 2015, "Tech House", "Software synth", "Stab", "Tech house - stab di accordo (stile)",
     wt("Analog Morph", pos=0.66, cut=2000, res=0.2, env=0.4, att=1, dec=180, sus=0.0, rel=120, uni=3, det=0.12, g_dlymix=0.25, g_dlydiv="1/8D")),
    ("Tech House Wobble Sub", 2019, "Tech House", "Roland SH-101", "Bass", "Tech house - sub con LFO (stile)",
     sh_bass(cut=280, res=0.3, env=0.3, dec=300, sub=0.9, wave="Saw", a_lfocut=0.4, a_lforate=2.0)),
    ("Tech House Acid Line", 2020, "Tech House", "Roland TB-303", "Bass", "Tech house con linea acid (stile)",
     acid303(cut=420, res=0.7, env=0.45, dec=160, drive=0.5)),
    ("Tech House Perc Synth", 2018, "Tech House", "Software synth", "FX", "Tech house - percussione sintetica intonata (stile)",
     fm("1 Stack", [1, 1.5, 1, 1], [1.0, 0.6, 0, 0], [150, 50, 50, 50], [0, 0, 0, 0], vel=0.5, d_ad=180, d_as=0.0, d_ar=80, d_penv=12, d_pdec=30)),
    ("Tech House Donk", 2019, "Tech House", "Software synth", "Bass", "Tech house / bassline - 'donk' (stile)",
     fm("1 Stack", [1, 2.0, 1, 1], [1.0, 0.5, 0, 0], [220, 70, 50, 50], [0, 0, 0, 0], vel=0.4, d_ad=250, d_as=0.0, d_ar=60, d_oct=-1)),
    ("Tech House Rave Organ", 2021, "Tech House", "Korg M1", "Organ", "Tech house 2020s - organo rave (stile)",
     add("House Organ", bright=0.7, tilt=0.6, att=1, dec=300, sus=0.2, rel=80, g_drive=0.2)),
    ("Bassline House Bass", 2008, "Tech House", "Software synth", "Bass", "Bassline / niche (UK 2006-09) - stile",
     wt("Pulse Width", pos=0.5, cut=1200, res=0.4, env=0.5, att=1, dec=200, sus=0.4, rel=60, d_oct=-1, d_lfotim=0.3, d_lforate=8, g_mode="Mono", g_glide=40)),
    ("Techno Rumble Sub", 2016, "Techno", "Analog mono", "Bass", "Techno 2010s - rumble (stile)",
     A(o1wave="Sine", o1oct=-1, o2lvl=0.0, flttype="LP24", cut=200, res=0.2, aa=1, ad=900, **{"as": 0.0}, ar=600, penv=5, pdec=40, drive=0.8,
       g_revmix=0.5, g_revsize=0.8, g_revdamp=0.9, g_drive=0.4)),
    ("Techno Industrial Stab", 2018, "Techno", "Korg MS-20", "Stab", "Industrial techno 2015-20 (stile)",
     A(o1wave="Saw", o2wave="Pulse", o2lvl=0.8, o2semi=7, flttype="HP12", cut=500, res=0.5, env=0.3, aa=0.5, ad=200, **{"as": 0.0}, ar=150,
       drive=0.9, g_drive=0.6, g_revmix=0.3)),
    ("Techno Hypnotic Seq", 2014, "Techno", "Roland SH-101", "Sequence", "Techno ipnotica 2010s (stile)",
     sh_bass(cut=600, res=0.55, env=0.5, dec=120, sub=0.2, wave="Pulse", a_lfocut=0.4, a_lforate=0.08, g_dlymix=0.2, g_dlydiv="1/8D")),
    ("Techno Detroit Pad", 2001, "Techno", "Roland JP-8000", "Pad", "Detroit techno anni 2000 (stile)",
     supersaw(det=0.25, mix=0.5, cut=2200, att=700, rel=2000, dec=2000, d_lfocut=0.25, d_lforate=0.1)),
    ("Techno FM Ping", 2003, "Techno", "Yamaha DX7", "Pluck", "Techno minimale - ping FM (stile)",
     fm("1 Stack", [1, 3.5, 1, 1], [1.0, 0.5, 0, 0], [250, 80, 50, 50], [0, 0, 0, 0], vel=0.5, d_ad=300, d_as=0.0, d_ar=150, g_dlymix=0.3, g_dlydiv="1/8D")),
    ("Techno Noise Riser", 2015, "Techno", "Analog mono", "FX", "Techno - riser di rumore (stile)", noise_sweep()),
    ("Techno Laser Zap", 1998, "Techno", "Roland SH-101", "FX", "Techno / electro - laser zap (stile)", zap()),
    ("Schranz Distort Lead", 2002, "Techno", "Access Virus", "Lead", "Schranz / hard techno 2000-04 (stile)",
     supersaw(det=0.2, mix=0.5, cut=3000, att=1, dec=200, sus=0.6, rel=80, g_drive=0.85)),
    ("Acid Techno Sweep", 1996, "Acid", "Roland TB-303", "Bass", "Acid techno anni '90 (Liberator, Chris Liberator), stile",
     acid303(cut=260, res=0.88, env=0.7, dec=220, drive=0.9, a_lfocut=0.5, a_lforate=0.06, g_drive=0.4)),
    ("Acid Squelch Lead", 2008, "Acid", "Roland TB-303", "Lead", "Acid lead (stile)",
     acid303(cut=600, res=0.8, env=0.55, dec=180, a_o1oct=0, drive=0.6)),
    ("Acid 202 Dub", 1992, "Acid", "Roland MC-202", "Bass", "MC-202 acid dub (stile)",
     acid303(cut=350, res=0.7, env=0.5, dec=260, wave="Pulse", a_flttype="LP24", g_dlymix=0.3, g_dlydiv="1/8D", g_dlyfb=0.6)),
    ("Minimal Dub Chord", 2007, "Minimal", "Analog poly", "Chord", "Minimal / dub 2005-08 (stile)",
     poly_stab(cut=700, res=0.25, env=0.4, dec=250, chorus="Off", g_dlymix=0.4, g_dlyfb=0.65, g_dlydiv="1/4D", g_revmix=0.4)),
    ("Minimal Wooden Perc", 2006, "Minimal", "Software synth", "FX", "Minimal - percussione legnosa sintetica (stile)",
     add("Mallet", bright=0.3, tilt=0.8, att=1, dec=200, sus=0.0, rel=80)),
    ("Progressive House Bass", 2005, "Progressive House", "Access Virus", "Bass", "Progressive house anni 2000 - basso (stile)",
     wt("Analog Morph", pos=0.66, cut=500, res=0.3, env=0.4, att=1, dec=220, sus=0.3, rel=80, d_oct=-1, uni=2, det=0.1, g_mode="Mono")),
    ("Progressive Sweep Pad", 2003, "Progressive House", "Access Virus", "Pad", "Progressive (Sasha, John Digweed) - pad in movimento (stile)",
     wt("PPG Digital", pos=0.2, d_wlfo=0.6, d_lforate=0.08, cut=2500, res=0.3, att=1000, dec=3000, sus=1.0, rel=2500, uni=3, det=0.15, g_revmix=0.45)),
    ("Big Room Lead", 2014, "EDM / Big Room", "Reveal Spire", "Lead", "Big room 2013-15 (stile)",
     supersaw(det=0.55, mix=0.75, cut=7000, dec=200, sus=0.8, rel=120, d_penv=-12, d_pdec=25, g_revmix=0.35, g_drive=0.25)),
    ("EDM Sidechain Pad", 2012, "EDM / Big Room", "LennarDigital Sylenth1", "Pad", "EDM - pad 'pompato' (stile)",
     supersaw(det=0.45, mix=0.6, cut=5000, att=2, dec=2000, rel=300, d_lfoshape="Saw Up", d_lforate=2.0, d_lfocut=0.0, g_revmix=0.3)),
    ("Trance Gate Pluck", 2001, "Trance", "Roland JP-8000", "Pluck", "Trance 2000-02 (stile)",
     supersaw(det=0.4, mix=0.6, cut=2000, att=1, dec=180, sus=0.0, rel=150, d_env=0.5, d_fd=180, d_fs=0.0, g_dlymix=0.35, g_dlydiv="1/8D")),
    ("Trance Acid Lead", 1999, "Trance", "Roland TB-303", "Lead", "Trance acid (stile)",
     acid303(cut=500, res=0.75, env=0.5, dec=200, a_o1oct=0, g_dlymix=0.3, g_dlydiv="1/8D", g_revmix=0.3)),
    ("Eurodance Stab", 1995, "Eurodance / 90s Dance", "Roland JD-800", "Stab", "Eurodance - stab (stile)",
     supersaw(det=0.3, mix=0.5, cut=4500, dec=300, sus=0.0, rel=180, g_revmix=0.2)),
]
for (name, year, genre, synth, cat, ref, params) in dance_extra:
    add_p(name, year, genre, synth, cat, ref, params, notes=STY)

# ---------------------------------------------------------------- systematic house / techno / tech house kit (per decade)
# Same musical role, different era instrument: lets you hear how each scene's core sounds evolved.
eras = [
    (1988, "Roland Juno-106", "Korg M1"), (1993, "Roland SH-101", "Korg M1"), (1998, "Roland MC-202", "Roland JP-8000"),
    (2004, "Moog Voyager", "Access Virus"), (2010, "Moog Sub 37", "NI Massive"), (2016, "Sequential Prophet-6", "Xfer Serum"),
    (2022, "Moog One", "Vital"),
]
for (year, an, dg) in eras:
    tag = "%d" % year
    add_p("House Bass " + tag, year, "House", an, "Bass", "House %ds - basso (stile)" % (year // 10 * 10),
          sh_bass(cut=550 + (year - 1988) * 6, res=0.25, env=0.45, dec=230, sub=0.7, wave="Saw" if year > 1995 else "Pulse"), notes=STY)
    add_p("House Chord " + tag, year, "House", an, "Chord", "House %ds - accordo (stile)" % (year // 10 * 10),
          poly_stab(cut=1500, res=0.15, env=0.4, dec=500, chorus="II" if year < 2000 else "I"), notes=STY)
    add_p("Techno Bass " + tag, year, "Techno", an, "Bass", "Techno %ds - basso (stile)" % (year // 10 * 10),
          moog_bass(cut=300, res=0.4, env=0.55, dec=150, o2=0, sus=0.0, a_ad=200, a_as=0.0, drive=0.6), notes=STY)
    add_p("Techno Stab " + tag, year, "Techno", dg, "Stab", "Techno %ds - stab digitale (stile)" % (year // 10 * 10),
          wt("Analog Morph" if year < 2000 else "Growl", pos=0.6, cut=2500, res=0.3, env=0.4, att=1, dec=200, sus=0.0, rel=150, uni=2,
             det=0.15, g_dlymix=0.25, g_dlydiv="1/8D", g_revmix=0.2), notes=STY)
    add_p("Tech House Bass " + tag, year, "Tech House", dg, "Bass", "Tech house %ds - basso (stile)" % (year // 10 * 10),
          fm_bass(alg="1 Stack", r=(0.5, 1.0, 1, 1), l=(1.0, 0.45 + (year - 1988) * 0.004, 0.2, 0), e=(800, 250, 150, 100), s=(0.4, 0.1, 0, 0),
                  fb=0.3) if year < 2005 else
          wt("Analog Morph", pos=0.66, cut=500, res=0.3, env=0.45, att=1, dec=160, sus=0.3, rel=60, d_oct=-1, g_mode="Mono", g_drive=0.25), notes=STY)
    add_p("Tech House Layer " + tag, year, "Tech House", an + " + " + dg, "Bass", "Tech house %ds - sub analogico + top digitale (layer)" % (year // 10 * 10),
          L(sh_bass(cut=250, res=0.1, env=0.2, dec=300, sub=0.9, wave="Pulse", a_o1lvl=0.3, a_level=0.7),
            fm_pluck(r=(1, 3, 1, 1), d_level=0.5, d_oct=0, g_dlymix=0.0), g_mode="Mono"), notes=STY)
    add_p("Deep House Keys " + tag, year, "Deep House", an + " + " + dg, "Keys", "Deep house %ds - keys (layer analogico + digitale)" % (year // 10 * 10),
          L(poly_pad(cut=1100, att=10, rel=600, chorus="Off", a_level=0.35, a_ad=1200, a_as=0.3),
            fm_epiano(bright=0.3, d_level=0.7), g_chomode="II", g_chomix=0.4, g_revmix=0.25), notes=STY)
    add_p("Acid Line " + tag, year, "Acid", "Roland TB-303", "Bass", "Acid %ds (stile)" % (year // 10 * 10),
          acid303(cut=250 + (year - 1988) * 5, res=0.85, env=0.6, dec=200, drive=0.3 + (year - 1988) * 0.015), notes=STY)

# ------------------------------------------------------------------------------------------------ resolve metadata
def instruments(synth):
    parts = [s.strip() for s in synth.split("+")]
    kinds, skins, names = [], [], []
    for p in parts:
        if p not in SYNTHS: raise SystemExit("unknown synth: %r" % p)
        k, sk = SYNTHS[p]
        kinds.append(k); skins.append(sk); names.append(p)
    return names, kinds, skins

levels = {}
if os.path.exists(LEVELS):
    levels = json.load(open(LEVELS))

out, seen = [], set()
for pr in P:
    names, kinds, skins = instruments(pr["synth"])
    p = dict(pr["p"])
    usesA, usesD = bool(p.get("a_on", 0)), bool(p.get("d_on", 0))
    t = "AD" if usesA and usesD else ("A" if usesA else "D")
    want = "AD" if ("AD" in kinds or ("A" in kinds and "D" in kinds)) else kinds[0]
    if want != t:
        raise SystemExit("section mismatch for %s: instrument=%s engine=%s" % (pr["name"], want, t))
    sA = sD = ""; nA = nD = ""
    for n, k, sk in zip(names, kinds, skins):
        if k in ("A", "AD") and not sA: sA, nA = sk, n
        if k in ("D", "AD") and not sD: sD, nD = sk, n
    if pr["name"] in seen: raise SystemExit("duplicate name " + pr["name"])
    seen.add(pr["name"])
    if pr["genre"] not in GENRES: raise SystemExit("genre " + pr["genre"])
    if pr["cat"] not in CATS: raise SystemExit("cat " + pr["cat"])
    p = {k: v for k, v in p.items() if v is not None}
    if pr["name"] in levels:
        p["g_trim"] = round(max(-24.0, min(24.0, levels[pr["name"]])), 1)
    out.append(dict(name=pr["name"], original=pr["original"], year=pr["year"], decade="%ds" % (pr["year"] // 10 * 10),
                    genre=pr["genre"], dance=pr["genre"] in DANCE, synth=pr["synth"], synthA=nA, synthD=nD, skinA=sA, skinD=sD,
                    type=t, cat=pr["cat"], ref=pr["ref"], notes=pr["notes"], p=p))

order = {g: i for i, g in enumerate(GENRES)}
out.sort(key=lambda x: (x["year"], order[x["genre"]], x["name"]))
decades = sorted({o["decade"] for o in out})
json.dump(dict(version=1, decades=decades, genres=GENRES, categories=CATS, presets=out), open(OUT, "w"), indent=1, ensure_ascii=False)

from collections import Counter
print("presets:", len(out))
print("dance family:", sum(o["dance"] for o in out), " house/deep/tech house/techno/acid/minimal:",
      sum(o["genre"] in ("House", "Deep House", "Tech House", "Techno", "Acid", "Minimal", "Melodic Techno") for o in out))
print("types:", Counter(o["type"] for o in out))
print("decades:", Counter(o["decade"] for o in out))
print("factory names:", sum(1 for o in out if o["original"]))

# ------------------------------------------------------------------------------------------------ docs/PRESETS.md
md = ["# TWINSYNTH - libreria dei preset (%d)\n" % len(out),
      "Generata da `tools/gen_presets.py`. **Sez.** = sezione che suona il preset (A analogica, D digitale, A+D layer). ",
      "**Nome di fabbrica** = nome reale del preset sullo strumento originale, quando il suono è un preset di fabbrica documentato; ",
      "vuoto = patch creata in studio o suono di genere (il preset prende il nome dal brano / stile).\n"]
for dec in decades:
    rows = [o for o in out if o["decade"] == dec]
    md.append("\n## %s (%d)\n" % (dec, len(rows)))
    md.append("| Preset | Nome di fabbrica | Strumento originale | Sez. | Anno | Genere | Tipo | Riferimento |")
    md.append("|---|---|---|---|---|---|---|---|")
    for o in rows:
        md.append("| %s | %s | %s | %s | %d | %s | %s | %s |" % (o["name"], o["original"], o["synth"], "A+D" if o["type"] == "AD" else o["type"],
                                                         o["year"], o["genre"], o["cat"], o["ref"].replace("|", "/")))
os.makedirs(os.path.join(HERE, "..", "docs"), exist_ok=True)
open(os.path.join(HERE, "..", "docs", "PRESETS.md"), "w").write("\n".join(md) + "\n")
