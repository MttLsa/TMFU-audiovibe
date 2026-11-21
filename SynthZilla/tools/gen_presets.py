#!/usr/bin/env python3
"""
SYNTHZILLA factory library generator  ->  resources/presets.json  +  docs/PRESETS.md

Every preset recreates by synthesis (no samples) a famous synth sound and is tied to the most famous song that used it.
The library is ordered only by year. A preset is made of 1..5 modules (A..E); each module names the instrument it
recreates:
  * analog original instrument  -> module with the Analog engine
  * digital original (FM, wavetable, ROM/PCM, VA, software) -> module with a digital engine
  * analog + digital combination (hybrid synth, or two instruments layered on the record) -> two modules in layer
`original` = the real factory preset name when the sound is a documented factory preset.
Levels: tools/levels.json (written by `SynthZillaTest <dir> levels`) -> g_trim so every preset peaks at -1 dBFS.
"""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'resources', 'presets.json')
DOC = os.path.join(HERE, '..', 'docs', 'PRESETS.md')
LEVELS = os.path.join(HERE, 'levels.json')

GENRES = ["Electronic", "Pop", "Rock", "Prog Rock", "Funk / Soul", "R&B", "Disco", "Synth-Pop", "New Wave", "Film / TV",
          "Hip-Hop", "House", "Deep House", "Acid House", "Techno", "Tech House", "Trance", "Eurodance", "Rave", "Drum & Bass",
          "Dubstep", "EDM", "Electro", "Ambient", "Synthwave", "Indie"]
CATS = ["Bass", "Lead", "Pad", "Keys", "Piano", "Organ", "Stab", "Chord", "Pluck", "Brass", "Strings", "Bell", "Sequence", "Vox", "FX"]

# instrument -> (A analog | D digital, skin, maker, year)
MODELS = [
    ("Moog modular", "A", "moog-modular", "Moog", 1964), ("Electro-Theremin", "A", "ems", "Tannerin", 1958),
    ("BBC Radiophonic oscillators", "A", "ems", "BBC", 1958), ("EMS VCS3 / Synthi AKS", "A", "ems", "EMS", 1969),
    ("ARP 2500", "A", "arp", "ARP", 1970), ("ARP 2600", "A", "arp", "ARP", 1971), ("Moog Minimoog Model D", "A", "minimoog", "Moog", 1970),
    ("ARP Odyssey", "A", "arp", "ARP", 1972), ("ARP Solina String Ensemble", "A", "solina", "ARP / Eminent", 1974),
    ("Polymoog", "A", "minimoog", "Moog", 1975), ("Moog Taurus", "A", "minimoog", "Moog", 1976), ("Yamaha CS-80", "A", "cs80", "Yamaha", 1977),
    ("Sequential Prophet-5", "A", "prophet", "Sequential", 1978), ("Oberheim OB-X", "A", "oberheim", "Oberheim", 1979),
    ("Korg MS-20", "A", "ms20", "Korg", 1978), ("Roland Jupiter-8", "A", "jupiter", "Roland", 1981), ("Oberheim OB-Xa", "A", "oberheim", "Oberheim", 1981),
    ("Roland TB-303", "A", "tb303", "Roland", 1981), ("Moog Source", "A", "modern-analog", "Moog", 1981), ("Korg Polysix", "A", "polysix", "Korg", 1981),
    ("Roland Juno-60", "A", "juno", "Roland", 1982), ("Roland SH-101", "A", "sh101", "Roland", 1982), ("Roland MC-202", "A", "mc202", "Roland", 1983),
    ("Roland Juno-106", "A", "juno", "Roland", 1984), ("Roland JX-8P", "A", "alpha-juno", "Roland", 1985),
    ("Roland Alpha Juno-2", "A", "alpha-juno", "Roland", 1986), ("Moog Sub 37", "A", "modern-analog", "Moog", 2014),
    ("Yamaha DX7", "D", "dx7", "Yamaha", 1983), ("Yamaha TX-816", "D", "tx81z", "Yamaha", 1984), ("Synclavier", "D", "synclavier", "New England Digital", 1977),
    ("Fairlight CMI", "D", "fairlight", "Fairlight", 1979), ("PPG Wave 2.3", "D", "ppg", "PPG", 1984), ("Casio CZ-5000", "D", "cz", "Casio", 1985),
    ("Yamaha DX21", "D", "dx7", "Yamaha", 1985), ("Yamaha TX81Z", "D", "tx81z", "Yamaha", 1987), ("Roland D-50", "D", "d50", "Roland", 1987),
    ("Korg M1", "D", "m1", "Korg", 1988), ("Korg Wavestation", "D", "wavestation", "Korg", 1990), ("Roland JD-800", "D", "jd800", "Roland", 1991),
    ("Roland JP-8000", "D", "jp8000", "Roland", 1996), ("Roland JP-8080", "D", "jp8000", "Roland", 1998), ("Access Virus", "D", "virus", "Access", 1997),
    ("Clavia Nord Lead", "D", "nord", "Clavia", 1995), ("Korg Triton", "D", "rompler", "Korg", 1999), ("NI Massive", "D", "software", "Native Instruments", 2007),
    ("LennarDigital Sylenth1", "D", "software", "LennarDigital", 2006), ("Xfer Serum", "D", "software", "Xfer Records", 2014),
]
KIND = {m[0]: m[1] for m in MODELS}

# ================================================================================================ module builders
FLT = {"LP24": "LP24 Ladder", "LP18": "LP 303 Diode", "LP12": "LP12", "BP12": "BP12", "HP12": "HP12", "CLEAN": "LP24 Clean", "OFF": "Off"}

def M(**k):
    return dict(k)

def upd(p, k):
    p = dict(p); p.update(k); return p

def an(**k):
    p = dict(engine="Analog"); p.update(k); return p

def moog_bass(cut=380, res=0.25, env=0.55, dec=380, o2=-1, fine=6, glide=0, mono=True, sus=0.15, drive=0.35, **k):
    p = an(oct=-1, o1wave="Saw", o2wave="Saw", o2oct=o2 + 1, o2fine=fine, o2lvl=0.85, flt=FLT["LP24"], cut=cut, res=res,
           env=env, fa=1, fd=dec, fs=sus, fr=120, aa=1, ad=900, **{"as": 0.85}, ar=90, fdrive=drive, key=0.35, velf=0.25, drift=0.25,
           g_mode="Mono" if mono else "Poly", g_glide=glide)
    return upd(p, k)

def acid303(cut=300, res=0.82, env=0.62, dec=230, wave="Saw", accent=0.85, drive=0.35, glide=70, dist="Off", dgain=0.5, dtone=0.5, **k):
    p = an(oct=-1, o1wave=wave, o1pw=0.5, flt=FLT["LP18"], cut=cut, res=res, env=env, fa=0.5, fd=dec, fs=0.0, fr=60,
           aa=0.5, ad=1300, **{"as": 0.55}, ar=25, accent=accent, fdrive=drive, key=0.2, velf=0.15, drift=0.05,
           dist=dist, dgain=dgain, dtone=dtone, g_mode="Legato", g_glide=glide, g_vel=0.35)
    return upd(p, k)

def poly_pad(wave="Pulse", pw=0.42, pwm=0.45, cut=1500, res=0.15, env=0.25, att=900, rel=1800, chorus="II", sub=0.25, **k):
    p = an(o1wave=wave, o1pw=pw, pwm=pwm, sub=sub, o2wave="Saw", o2lvl=0.0, flt=FLT["LP24"], cut=cut, res=res, env=env,
           fa=att * 1.2, fd=2500, fs=0.6, fr=rel, aa=att, ad=1500, **{"as": 1.0}, ar=rel, lforate=0.6, lfoshape="Triangle", lfotim=0.0, drift=0.3,
           g_chomode=chorus, g_chomix=0.7, g_revmix=0.18, g_revsize=0.7)
    return upd(p, k)

def poly_stab(cut=900, res=0.25, env=0.55, dec=260, chorus="I", wave="Saw", o2="Pulse", **k):
    p = an(o1wave=wave, o2wave=o2, o2lvl=0.6, o2fine=7, o2pw=0.35, flt=FLT["LP24"], cut=cut, res=res, env=env, fa=1, fd=dec, fs=0.0, fr=dec,
           aa=1, ad=dec * 1.6, **{"as": 0.0}, ar=dec * 0.8, drift=0.2, key=0.5, velf=0.35, g_chomode=chorus, g_chomix=0.55)
    return upd(p, k)

def brass(cut=700, res=0.18, env=0.55, att=40, dec=700, sus=0.55, o2fine=9, chorus="Off", **k):
    p = an(o1wave="Saw", o2wave="Saw", o2lvl=0.9, o2fine=o2fine, flt=FLT["LP24"], cut=cut, res=res, env=env, fa=att, fd=dec, fs=sus,
           fr=300, aa=8, ad=900, **{"as": 0.9}, ar=280, drift=0.3, velf=0.4, key=0.5, uni=2, detune=0.12, g_chomode=chorus, g_chomix=0.5)
    return upd(p, k)

def lead(wave="Saw", cut=2400, res=0.3, env=0.35, glide=60, vib=0.0, o2fine=8, o2lvl=0.7, **k):
    p = an(o1wave=wave, o2wave="Saw", o2lvl=o2lvl, o2fine=o2fine, flt=FLT["LP24"], cut=cut, res=res, env=env, fa=2, fd=600, fs=0.5,
           fr=200, aa=3, ad=600, **{"as": 0.9}, ar=180, lforate=5.5, lfoshape="Sine", lfopitch=vib, lfodelay=350 if vib else 0, drift=0.2,
           fdrive=0.3, g_mode="Mono", g_glide=glide, dlymix=0.12)
    return upd(p, k)

def strings(cut=3200, att=500, rel=1500, chorus="Ensemble", **k):
    p = an(o1wave="Saw", o2wave="Saw", o2oct=1, o2lvl=0.5, o2fine=4, flt=FLT["LP12"], cut=cut, res=0.05, env=0.1, fa=att, fd=1000,
           fs=0.8, fr=rel, aa=att, ad=1000, **{"as": 1.0}, ar=rel, drift=0.25, g_chomode=chorus, g_chomix=0.85, g_revmix=0.2)
    return upd(p, k)

def hoover(**k):
    p = an(o1wave="Pulse", o1pw=0.3, pwm=0.7, o2wave="Saw", o2lvl=0.8, o2oct=-1, o2fine=-12, sub=0.4, uni=5, detune=0.55,
           flt=FLT["LP24"], cut=4200, res=0.1, env=0.15, fa=1, fd=900, fs=0.8, fr=400, aa=2, ad=700, **{"as": 1.0}, ar=350,
           lforate=7.0, lfoshape="Triangle", penv=-12, pdec=180, drift=0.3, g_mode="Mono", g_glide=180, g_chomode="I+II", g_chomix=0.8, g_bend=12)
    return upd(p, k)

def sh_bass(cut=600, res=0.35, env=0.5, dec=220, sub=0.7, wave="Pulse", **k):
    p = an(oct=-1, o1wave=wave, o1pw=0.5, sub=sub, flt=FLT["LP24"], cut=cut, res=res, env=env, fa=0.5, fd=dec, fs=0.1, fr=80,
           aa=0.5, ad=600, **{"as": 0.7}, ar=60, fdrive=0.3, key=0.3, velf=0.3, drift=0.08, g_mode="Mono")
    return upd(p, k)

def pluck_a(cut=700, res=0.3, env=0.7, dec=180, **k):
    p = an(o1wave="Saw", o2wave="Pulse", o2lvl=0.5, o2fine=6, flt=FLT["LP24"], cut=cut, res=res, env=env, fa=0.5, fd=dec, fs=0.0,
           fr=dec, aa=0.5, ad=dec * 2.5, **{"as": 0.0}, ar=dec, drift=0.1, velf=0.4, key=0.6, dlymix=0.18, dlydiv="1/8D", g_revmix=0.12)
    return upd(p, k)

def sync_lead(semi=12, penv=0, cut=4500, **k):
    p = an(o1wave="Saw", o2wave="Saw", o2lvl=1.0, o1lvl=0.0, o2semi=semi, sync=1, flt=FLT["LP24"], cut=cut, res=0.15, env=0.25,
           fa=1, fd=500, fs=0.6, fr=200, aa=2, ad=500, **{"as": 0.9}, ar=200, drift=0.15, penv=penv, pdec=500, g_mode="Mono", g_glide=30)
    return upd(p, k)

# ----- digital builders
def fm6(alg, ops, fb=0.0, vel=0.6, **k):
    """ops: up to 6 tuples (coarse, fine, level, attack ms, decay ms, sustain, release ms); missing ops are off"""
    p = dict(engine="FM 6-op", alg=str(alg), fb=fb, fmvel=vel, flt="Off", aa=1, ad=3000, **{"as": 1.0}, ar=350)
    for i in range(6):
        c, f, l, a, d, s, r = ops[i] if i < len(ops) else (1, 0, 0, 1, 1000, 1, 300)
        p.update({"op%dcrs" % (i + 1): c, "op%dfin" % (i + 1): f, "op%dlvl" % (i + 1): l, "op%datk" % (i + 1): a,
                  "op%ddec" % (i + 1): d, "op%dsus" % (i + 1): s, "op%drel" % (i + 1): r})
    return upd(p, k)

def fm_epiano(bright=0.5, **k):
    # DX7 E.PIANO 1 structure: algorithm 5, three carrier/modulator pairs; op4 at 14x = the "tine"
    return fm6(5, [(1, 0, 1.0, 1, 4200, 0.0, 450), (1, 0, 0.55 + 0.2 * bright, 1, 1400, 0.12, 400),
                   (1, 0.01, 0.75, 1, 2600, 0.0, 450), (14, 0, 0.40 + 0.2 * bright, 1, 260, 0.0, 200),
                   (1, 0, 0.35, 1, 3000, 0.0, 450), (1, 0, 0.30, 1, 2000, 0.0, 300)],
               fb=0.2, vel=0.85, **upd(dict(uni=2, detune=0.03, ar=450, g_chomode="I", g_chomix=0.35, g_vel=0.7), k))

def fm_bass(alg=16, ratios=(0, 0, 1, 1, 1, 1), lv=(1.0, 0.72, 0.4, 0.3, 0.0, 0.0), dec=(1200, 420, 300, 160, 200, 200), sus=(0.6, 0.15, 0.0, 0.0, 0, 0), fb=0.35, **k):
    ops = [(ratios[i], 0, lv[i], 0.5, dec[i], sus[i], 90) for i in range(6)]
    return fm6(alg, ops, fb=fb, vel=0.5, **upd(dict(aa=0.5, ad=900, **{"as": 0.45}, ar=90, g_mode="Mono"), k))

def fm_bell(r2=3.5, **k):
    return fm6(5, [(1, 0, 1.0, 1, 6000, 0, 2500), (3, 0.5, 0.62, 1, 3500, 0, 2000), (1, 0, 0.55, 1, 2500, 0, 2000), (7, 0, 0.4, 1, 900, 0, 800),
                   (2, 0, 0.35, 1, 4000, 0, 2000), (r2 if r2 >= 1 else 1, 0, 0.3, 1, 1500, 0, 900)], vel=0.6,
               **upd(dict(ad=8000, **{"as": 0.0}, ar=2500, g_revmix=0.25), k))

def fm_brass(**k):
    return fm6(22, [(1, 0, 1.0, 30, 2000, 0.9, 250), (1, 0, 0.6, 60, 1500, 0.55, 250), (1, 0.005, 0.8, 30, 2000, 0.9, 250),
                    (1, 0, 0.7, 30, 2000, 0.9, 250), (1, 0, 0.6, 30, 2000, 0.9, 250), (1, 0, 0.55, 80, 1200, 0.4, 250)], fb=0.45, vel=0.5,
               **upd(dict(aa=25, ar=250, uni=2, detune=0.06), k))

def fm_pluck(r=3, **k):
    return fm6(1, [(1, 0, 1.0, 0.5, 400, 0, 150), (r, 0, 0.6, 0.5, 140, 0, 100), (1, 0, 0.6, 0.5, 300, 0, 150), (1, 0, 0.3, 0.5, 90, 0, 80)],
               fb=0.2, vel=0.7, **upd(dict(ad=500, **{"as": 0.0}, ar=150, dlymix=0.2, dlydiv="1/8D"), k))

def wt(table, pos=0.0, wenv=0.0, cut=6000, res=0.1, env=0.2, att=2, dec=600, sus=0.8, rel=300, uni=1, det=0.2, **k):
    p = dict(engine="Wavetable", wt=table, wpos=pos, wenv=wenv, flt=FLT["CLEAN"], cut=cut, res=res, env=env, fa=att, fd=dec, fs=0.4, fr=rel,
             aa=att, ad=dec, **{"as": sus}, ar=rel, uni=uni, detune=det)
    return upd(p, k)

def supersaw(det=0.45, mix=0.65, cut=9000, att=2, rel=400, sus=1.0, dec=600, **k):
    p = dict(engine="Supersaw", ssdet=det, ssmix=mix, flt="LP12", cut=cut, res=0.1, env=0.15, fa=att, fd=dec, fs=0.6, fr=rel,
             aa=att, ad=dec, **{"as": sus}, ar=rel, g_width=0.85, g_revmix=0.2, dlymix=0.12)
    return upd(p, k)

def pd(wave="Saw", amt=0.6, penv=0.0, cut=20000, res=0.0, flt="Off", uni=1, det=0.1, dec=600, sus=0.8, att=1, rel=200, **k):
    p = dict(engine="Phase Dist", pdw=wave, pdamt=amt, pdenv=penv, flt=flt, cut=cut, res=res, uni=uni, detune=det,
             fa=1, fd=dec, fs=0.2, fr=rel, aa=att, ad=dec, **{"as": sus}, ar=rel)
    return upd(p, k)

def add(model, bright=0.5, tilt=0.5, inh=0.0, even=1.0, att=1, dec=3000, sus=1.0, rel=300, cut=20000, flt="Off", **k):
    p = dict(engine="Additive", addm=model, bright=bright, tilt=tilt, inh=inh, even=even, aa=att, ad=dec, **{"as": sus}, ar=rel, flt=flt, cut=cut)
    return upd(p, k)

# ================================================================================================ the library
P = []
def S(name, year, genre, song, artist, cat, modules, original="", notes=""):
    """modules: list of (instrument, params) -> slots A, B, C ..."""
    if isinstance(modules, tuple): modules = [modules]
    P.append(dict(name=name, year=year, genre=genre, song=song, artist=artist, cat=cat, modules=modules, original=original, notes=notes))

UNC = "Strumento attribuito da fonti di settore / comunità, non documentato ufficialmente."
DEB = "Attribuzione discussa: il suono è ricreato nello stile del brano."

# ---------------------------------------------------------------------------------------- 1960s
S("Radiophonic Bassline", 1963, "Film / TV", "Doctor Who (theme)", "Ron Grainer / Delia Derbyshire", "Bass",
  ("BBC Radiophonic oscillators", an(oct=-1, o1wave="Triangle", o2wave="Sine", o2lvl=0.4, flt="LP12", cut=900, res=0.3, env=0.3, fd=200, fs=0.3,
     aa=1, ad=300, **{"as": 0.6}, ar=80, g_mode="Mono", dlymix=0.15)), notes="Oscillatori a valvole e nastro, non un synth commerciale.")
S("Tannerin Glide", 1966, "Pop", "Good Vibrations", "The Beach Boys", "Lead",
  ("Electro-Theremin", lead(wave="Sine", o2lvl=0.0, cut=6000, res=0.0, env=0.0, glide=260, vib=0.35, lforate=6.0, fdrive=0.0, g_revmix=0.2)))
S("Brandenburg Moog", 1968, "Electronic", "Brandenburg Concerto No. 3 (Switched-On Bach)", "Wendy Carlos", "Lead",
  ("Moog modular", lead(wave="Pulse", cut=2600, res=0.2, glide=0, vib=0.15, o2lvl=0.0, o1pw=0.3, g_mode="Poly", g_revmix=0.15)))
S("Save the Life Bass", 1968, "Pop", "Save the Life of My Child", "Simon & Garfunkel", "Bass",
  ("Moog modular", moog_bass(cut=500, res=0.35, env=0.6, dec=500, mono=True, dist="Tube", dgain=0.3)))
S("Here Comes the Moog", 1969, "Rock", "Here Comes the Sun", "The Beatles", "Lead",
  ("Moog modular", lead(wave="Saw", o2lvl=0.0, cut=1800, res=0.15, glide=40, vib=0.25, aa=60, g_revmix=0.25)))
S("Because Moog Pad", 1969, "Rock", "Because", "The Beatles", "Pad",
  ("Moog modular", lead(wave="Saw", o2lvl=0.6, cut=1200, res=0.1, glide=0, vib=0.2, aa=250, ar=900, g_mode="Poly", g_revmix=0.3)))

# ---------------------------------------------------------------------------------------- 1970s
S("Lucky Man Solo", 1970, "Prog Rock", "Lucky Man", "Emerson, Lake & Palmer", "Lead",
  ("Moog modular", lead(wave="Saw", o2wave="Pulse", cut=2000, res=0.35, env=0.45, glide=180, vib=0.3, lforate=5.2, dlymix=0.18)))
S("Popcorn Pluck", 1972, "Pop", "Popcorn", "Hot Butter", "Pluck",
  ("Moog modular", pluck_a(cut=1800, res=0.25, dec=160, o1wave="Pulse", o1pw=0.35, o2lvl=0.0, dlymix=0.0, g_revmix=0.1)))
S("Frankenstein Lead", 1972, "Rock", "Frankenstein", "The Edgar Winter Group", "Lead",
  ("ARP 2600", lead(wave="Saw", cut=1300, res=0.5, env=0.55, glide=80, vib=0.2, fdrive=0.6, dist="Tube", dgain=0.4)))
S("Rollin' Stone Bass", 1972, "Funk / Soul", "Papa Was a Rollin' Stone", "The Temptations", "Bass",
  ("ARP 2600", moog_bass(cut=420, res=0.3, env=0.4, dec=600, sus=0.3)), notes=UNC)
S("On the Run", 1973, "Prog Rock", "On the Run", "Pink Floyd", "Sequence",
  ("EMS VCS3 / Synthi AKS", an(o1wave="Saw", o2wave="Pulse", o2lvl=0.5, o2fine=10, flt="LP12", cut=900, res=0.55, env=0.4, fd=130, fs=0.1, aa=0.5,
     ad=160, **{"as": 0.2}, ar=60, lfocut=0.35, lforate=0.25, dlymix=0.3, dlydiv="1/16", dlyfb=0.55, g_mode="Mono")))
S("Chameleon Bass", 1973, "Funk / Soul", "Chameleon", "Herbie Hancock", "Bass",
  ("ARP Odyssey", moog_bass(cut=450, res=0.45, env=0.6, dec=260, o2=-1, fine=0, o2wave="Pulse", g_glide=20)))
S("Living for the City Bass", 1973, "Funk / Soul", "Living for the City", "Stevie Wonder", "Bass",
  ("Moog modular", moog_bass(cut=500, res=0.25, env=0.5, dec=500)), notes="Moog III / TONTO.")
S("Autobahn Bass", 1974, "Electronic", "Autobahn", "Kraftwerk", "Bass", ("Moog Minimoog Model D", moog_bass(cut=320, res=0.2, env=0.45, dec=300, fine=0)))
S("Autobahn Lead", 1974, "Electronic", "Autobahn", "Kraftwerk", "Lead", ("Moog Minimoog Model D", lead(wave="Triangle", o2wave="Triangle", cut=3000, res=0.1, glide=90, vib=0.2)))
S("Shine On Minimoog", 1975, "Prog Rock", "Shine On You Crazy Diamond", "Pink Floyd", "Lead",
  ("Moog Minimoog Model D", lead(wave="Saw", cut=1700, res=0.35, env=0.4, glide=120, vib=0.25, g_revmix=0.3, dlymix=0.2)))
S("Welcome Machine Drone", 1975, "Prog Rock", "Welcome to the Machine", "Pink Floyd", "Pad",
  ("EMS VCS3 / Synthi AKS", an(o1wave="Saw", o2wave="Saw", o2lvl=1, o2fine=-20, flt="LP24 Ladder", cut=500, res=0.55, lfocut=0.5, lforate=0.12,
     aa=1500, ad=2000, **{"as": 1}, ar=3000, g_revmix=0.4, g_revsize=0.85)))
S("Oxygene Strings", 1976, "Electronic", "Oxygène (Part IV)", "Jean-Michel Jarre", "Strings",
  ("ARP Solina String Ensemble", strings(cut=2600, att=700, rel=2500, g_revmix=0.4, dlymix=0.15)), notes="String machine Eminent 310, stessa famiglia della Solina.")
S("Oxygene IV Lead", 1976, "Electronic", "Oxygène (Part IV)", "Jean-Michel Jarre", "Lead",
  ("ARP Odyssey", lead(wave="Pulse", o1pw=0.25, o2lvl=0.4, cut=2200, res=0.3, vib=0.3, glide=90, dlymix=0.3, g_revmix=0.3, g_chomode="I")), notes=UNC)
S("I Feel Love Bass", 1977, "Disco", "I Feel Love", "Donna Summer", "Bass",
  ("Moog modular", moog_bass(cut=420, res=0.3, env=0.6, dec=160, o2=0, fine=0, sus=0.0, ad=220, **{"as": 0.0}, ar=60, mono=False,
                             dlymix=0.22, dlydiv="1/16", dlyfb=0.2, dlyping=0)), notes="Giorgio Moroder: Moog modular sincronizzato a 1/16.")
S("Flash Light Bass", 1977, "Funk / Soul", "Flash Light", "Parliament", "Bass",
  ("Moog Minimoog Model D", moog_bass(cut=280, res=0.35, env=0.7, dec=420, o2=-1, fine=4, glide=35, o2wave="Pulse")), notes="Bernie Worrell, Minimoog multi-traccia.")
S("Trans-Europe Lead", 1977, "Electronic", "Trans-Europe Express", "Kraftwerk", "Lead",
  ("Polymoog", lead(wave="Pulse", o1pw=0.4, o2lvl=0.0, cut=1500, res=0.2, glide=0, g_mode="Poly", g_revmix=0.25)), notes=UNC)
S("Close Encounters Tones", 1977, "Film / TV", "Close Encounters of the Third Kind", "John Williams", "Lead",
  ("ARP 2500", lead(wave="Saw", o2lvl=0.5, cut=1600, res=0.2, glide=0, vib=0.0, g_mode="Poly", aa=40, g_revmix=0.35)))
S("The Model Riff", 1978, "Electronic", "The Model", "Kraftwerk", "Sequence", ("Moog Minimoog Model D", pluck_a(cut=1200, res=0.25, dec=200, o2lvl=0.0)), notes=UNC)
S("Chase Sequence", 1978, "Disco", "Chase (Midnight Express)", "Giorgio Moroder", "Sequence",
  ("Moog modular", moog_bass(cut=650, res=0.45, env=0.55, dec=140, o2=0, sus=0.0, ad=180, **{"as": 0.0}, mono=False, dlymix=0.15, dlydiv="1/16")))
S("Cars Lead", 1979, "New Wave", "Cars", "Gary Numan", "Lead",
  ("Moog Minimoog Model D", lead(wave="Saw", o2wave="Saw", o2lvl=0.9, o2fine=0, o2oct=-1, cut=900, res=0.35, env=0.45, glide=0, fd=900)))
S("Vox Humana", 1979, "New Wave", "Cars", "Gary Numan", "Pad",
  ("Polymoog", poly_pad(wave="Saw", pwm=0.0, cut=1800, res=0.1, env=0.15, att=60, rel=600, chorus="Ensemble", sub=0.0, o2wave="Saw", o2lvl=0.8, o2fine=-14)),
  original="Vox Humana")
S("Let's Go Sync", 1979, "New Wave", "Let's Go", "The Cars", "Lead", ("Sequential Prophet-5", sync_lead(semi=12, penv=0, cut=5000, xmod=0.15)), notes=UNC)
S("Behind the Mask Bass", 1979, "Synth-Pop", "Behind the Mask", "Yellow Magic Orchestra", "Bass",
  ("ARP Odyssey", sh_bass(cut=900, res=0.3, env=0.4, dec=200, sub=0.0, wave="Saw", g_mode="Poly")))
S("Synclavier Strings", 1979, "Electronic", "Phaedra-style FM strings (Synclavier I)", "New England Digital demo", "Strings",
  ("Synclavier", fm6(32, [(1, 0, 0.8, 300, 3000, 0.9, 900), (2, 0.003, 0.5, 300, 3000, 0.9, 900), (3, 0, 0.35, 300, 3000, 0.9, 900),
                          (4, 0.004, 0.25, 300, 3000, 0.9, 900), (5, 0, 0.15, 300, 3000, 0.9, 900), (6, 0, 0.1, 300, 3000, 0.9, 900)], aa=300, ar=900, g_revmix=0.3)),
  notes="Sintesi additiva/FM del primo Synclavier.")

# ---------------------------------------------------------------------------------------- 1980s
S("Rydeen Bass", 1980, "Synth-Pop", "Rydeen", "Yellow Magic Orchestra", "Bass", ("ARP Odyssey", sh_bass(cut=800, res=0.35, env=0.45, dec=160, sub=0.0, wave="Saw", g_mode="Poly")))
S("Doctor Who 1980 Bass", 1980, "Film / TV", "Doctor Who (1980 theme)", "Peter Howell", "Bass",
  ("Yamaha CS-80", moog_bass(cut=600, res=0.3, env=0.4, dec=200, mono=True, dlymix=0.2, dlydiv="1/8D")))
S("Chariots Brass", 1981, "Film / TV", "Chariots of Fire", "Vangelis", "Brass",
  ("Yamaha CS-80", brass(cut=1200, res=0.15, env=0.45, att=140, dec=1600, sus=0.7, lforate=5.0, lfopitch=0.15, lfodelay=600, g_revmix=0.45, g_revsize=0.9)))
S("In the Air Tonight Pad", 1981, "Rock", "In the Air Tonight", "Phil Collins", "Pad",
  ("Sequential Prophet-5", poly_pad(wave="Saw", pwm=0.0, cut=900, res=0.3, env=0.2, att=300, rel=2000, chorus="I", o2wave="Pulse", o2lvl=0.6, o2fine=8, g_revmix=0.35)))
S("Ghosts Pad", 1981, "New Wave", "Ghosts", "Japan", "Pad",
  ("Sequential Prophet-5", poly_pad(wave="Saw", pwm=0.0, cut=700, res=0.35, att=600, rel=2500, chorus="Off", g_revmix=0.4)))
S("Tom Sawyer Growl", 1981, "Prog Rock", "Tom Sawyer", "Rush", "Lead",
  ("Oberheim OB-X", an(o1wave="Saw", o2wave="Saw", o2lvl=1, o2fine=-15, oct=-1, flt="LP24 Ladder", cut=250, res=0.55, env=0.75, fa=600, fd=1200, fs=0.3,
     aa=5, ad=900, **{"as": 1.0}, ar=400, g_mode="Mono", g_glide=150, fdrive=0.4)))
S("I Can't Go for That Riff", 1981, "R&B", "I Can't Go for That (No Can Do)", "Hall & Oates", "Keys",
  ("Sequential Prophet-5", poly_stab(cut=1400, res=0.2, env=0.4, dec=380, chorus="Off")))
S("Tainted Love Bink", 1981, "Synth-Pop", "Tainted Love", "Soft Cell", "FX",
  ("Synclavier", fm6(1, [(1, 0, 1.0, 0.5, 180, 0, 120), (1, 0.41, 0.65, 0.5, 90, 0, 90)], vel=0.3, ad=250, **{"as": 0.0}, ar=120, oct=1, g_revmix=0.15)))
S("Valerie Synth", 1982, "Pop", "Valerie", "Steve Winwood", "Chord",
  ("Sequential Prophet-5", poly_stab(cut=1800, res=0.15, env=0.35, dec=600, chorus="I", wave="Saw", o2="Saw")))
S("Blade Runner Brass", 1982, "Film / TV", "Blade Runner (End Titles)", "Vangelis", "Brass",
  ("Yamaha CS-80", brass(cut=900, res=0.25, env=0.55, att=350, dec=2500, sus=0.6, aa=200, ar=1500, lforate=5.5, lfopitch=0.2, lfodelay=800,
                         g_revmix=0.55, g_revsize=0.95, dlymix=0.15)))
S("Blade Runner Blues", 1982, "Film / TV", "Blade Runner Blues", "Vangelis", "Pad",
  ("Yamaha CS-80", poly_pad(wave="Saw", pwm=0.0, cut=1100, res=0.2, env=0.3, att=400, rel=2500, chorus="Ensemble", lfopitch=0.12, lforate=5, g_revmix=0.5, g_revsize=0.95)))
S("1999 Stabs", 1982, "Funk / Soul", "1999", "Prince", "Stab", ("Oberheim OB-Xa", poly_stab(cut=1400, res=0.15, env=0.5, dec=320, chorus="Off", uni=2, detune=0.15)))
S("Hungry Like the Wolf Arp", 1982, "New Wave", "Hungry Like the Wolf", "Duran Duran", "Sequence",
  ("Roland Jupiter-8", pluck_a(cut=1500, res=0.3, dec=150, o1wave="Pulse", o2lvl=0.4, dlymix=0.15, dlydiv="1/16")))
S("Thriller Opening Pad", 1982, "Pop", "Thriller", "Michael Jackson", "Pad",
  ("Roland Jupiter-8", poly_pad(wave="Saw", pwm=0.0, cut=1400, res=0.2, att=1200, rel=2500, chorus="II", o2wave="Saw", o2lvl=0.8, o2fine=-9, g_revmix=0.35)),
  notes="Anthony Marinelli, Jupiter-8 per i pad d'apertura (fonte: blog di settore).")
S("Planet Rock ORCH5", 1982, "Hip-Hop", "Planet Rock", "Afrika Bambaataa & The Soulsonic Force", "Stab",
  ("Fairlight CMI", add("Brass", bright=0.8, att=1, dec=420, sus=0.0, rel=200, bits=0.55, srr=0.35, oct=-1, g_revmix=0.2)), original="ORCH5",
  notes="Fairlight = campionatore digitale a 8 bit: il colpo d'orchestra è sintetizzato (additivo + riduzione bit/frequenza).")
S("Don't Go Bass", 1982, "Synth-Pop", "Don't Go", "Yazoo", "Bass", ("Roland SH-101", sh_bass(cut=700, res=0.3, env=0.5, dec=200, sub=0.6, wave="Saw")))
S("I Ran Bass", 1982, "New Wave", "I Ran (So Far Away)", "A Flock of Seagulls", "Bass",
  ("Korg MS-20", sh_bass(cut=650, res=0.5, env=0.4, dec=250, sub=0.2, wave="Saw", flt="LP12", fdrive=0.6)))
S("Raga Bhairav 303", 1982, "Acid House", "Raga Bhairav (Ten Ragas to a Disco Beat)", "Charanjit Singh", "Bass",
  ("Roland TB-303", acid303(cut=500, res=0.7, env=0.5, dec=300, glide=60)))
S("Jump Brass", 1983, "Rock", "Jump", "Van Halen", "Brass",
  ("Oberheim OB-Xa", brass(cut=1600, res=0.08, env=0.35, att=5, dec=1200, sus=0.75, o2fine=12, uni=3, detune=0.2, aa=2, g_chomode="I", g_chomix=0.3)))
S("Blue Monday Bass", 1983, "New Wave", "Blue Monday", "New Order", "Bass", ("Moog Source", sh_bass(cut=700, res=0.2, env=0.45, dec=180, sub=0.0, wave="Saw", g_mode="Poly")))
S("Relax Bass", 1983, "Synth-Pop", "Relax", "Frankie Goes to Hollywood", "Bass",
  ("Roland Jupiter-8", moog_bass(cut=550, res=0.3, env=0.5, dec=200, o2=0, fine=10, sus=0.1, mono=False, dlymix=0.12, dlydiv="1/16")))
S("Time After Time Pad", 1983, "Pop", "Time After Time", "Cyndi Lauper", "Pad",
  ("Roland Juno-60", poly_pad(wave="Pulse", pw=0.45, pwm=0.3, cut=1900, res=0.1, att=200, rel=1400, chorus="II", sub=0.2)), original="61",
  notes="Il produttore racconta di essere partito dal preset 61 del Juno-60.")
S("Beat It Gong", 1983, "Pop", "Beat It", "Michael Jackson", "FX",
  ("Synclavier", fm_bell(r2=1.41, oct=-2, ad=9000, ar=4000, g_revmix=0.4)), notes="Intro di gong del Synclavier.")
S("E.PIANO 1", 1985, "R&B", "Saving All My Love for You", "Whitney Houston", "Keys", ("Yamaha DX7", fm_epiano()), original="E.PIANO 1",
  notes="Il preset 11 del DX7 è il suono delle ballate anni '80; per alcuni brani di Whitney Houston si discute se sia DX7, Rhodes o entrambi.")
S("BASS 1", 1985, "Synth-Pop", "Take On Me", "a-ha", "Bass",
  ("Yamaha DX7", fm_bass(alg=16, ratios=(0, 0, 1, 1, 1, 1), lv=(1.0, 0.78, 0.5, 0.35, 0.3, 0.0), dec=(1600, 500, 250, 120, 300, 200), fb=0.5,
                         g_chomode="I", g_chomix=0.4)), original="BASS 1", notes="Il basso slap di Take On Me: preset 15 BASS 1 con chorus.")
S("Take On Me Riff", 1985, "Synth-Pop", "Take On Me", "a-ha", "Lead",
  [("Roland Juno-60", an(o1wave="Pulse", o1pw=0.45, o2lvl=0.0, sub=0.3, flt="LP24 Ladder", cut=3200, res=0.1, env=0.2, fd=300, fs=0.6, aa=1, ad=400,
                         **{"as": 0.8}, ar=180, g_chomode="I", g_chomix=0.6)),
   ("Yamaha DX7", fm6(5, [(2, 0, 0.6, 1, 600, 0.3, 200), (3, 0, 0.35, 1, 300, 0.1, 150)], vel=0.5, level=0.35))],
  notes="Riff principale sul Juno-60 con DX7 nella produzione: layer analogico + digitale.")
S("Fresh Bass", 1984, "Funk / Soul", "Fresh", "Kool & the Gang", "Bass",
  ("Yamaha DX7", fm_bass(alg=16, lv=(1.0, 0.7, 0.45, 0.3, 0.2, 0.0), fb=0.4)), original="BASS 1")
S("Radio Ga Ga Arp", 1984, "Rock", "Radio Ga Ga", "Queen", "Sequence",
  ("Roland Jupiter-8", moog_bass(cut=800, res=0.25, env=0.4, dec=150, o2=0, fine=0, sus=0.0, ad=180, **{"as": 0.0}, mono=False, dlymix=0.15, dlydiv="1/16")))
S("Break Free Solo", 1984, "Rock", "I Want to Break Free", "Queen", "Lead",
  ("Roland Jupiter-8", lead(wave="Saw", cut=2800, res=0.2, glide=60, vib=0.3, uni=2, detune=0.15, g_chomode="I")))
S("Axel F Lead", 1984, "Film / TV", "Axel F", "Harold Faltermeyer", "Lead",
  ("Roland Jupiter-8", an(o1wave="Pulse", o1pw=0.5, o2wave="Saw", o2lvl=0.35, o2oct=1, flt="LP24 Ladder", cut=2600, res=0.15, env=0.3, fa=1, fd=220, fs=0.5,
                          aa=1, ad=300, **{"as": 0.75}, ar=120, dlymix=0.18, dlydiv="1/8D", g_revmix=0.15)), notes=UNC)
S("Final Countdown Brass", 1986, "Rock", "The Final Countdown", "Europe", "Brass",
  [("Roland JX-8P", brass(cut=1400, env=0.4, att=30, dec=900, sus=0.7, level=0.6)), ("Yamaha TX-816", fm_brass(level=0.7))],
  notes="Fanfara: Yamaha TX-816 (FM) + Roland JX-8P (analogico) in layer.")
S("Take My Breath Bass", 1986, "Pop", "Take My Breath Away", "Berlin", "Bass",
  ("Yamaha DX7", fm_bass(alg=16, lv=(1.0, 0.55, 0.25, 0.0, 0.0, 0.0), dec=(3000, 900, 300, 200, 200, 200), sus=(0.7, 0.3, 0, 0, 0, 0), fb=0.15,
                         **{"as": 0.8}, ar=250, g_mode="Poly", g_chomode="I", g_chomix=0.3)))
S("Running Up That Hill Cello", 1985, "Pop", "Running Up That Hill", "Kate Bush", "Strings",
  ("Fairlight CMI", add("Strings", bright=0.35, att=40, dec=2000, sus=1.0, rel=500, bits=0.4, srr=0.2, oct=-1, flt="LP12", cut=2500, lfopitch=0.12, lforate=5.5)))
S("Shout ARR1", 1985, "New Wave", "Shout", "Tears for Fears", "Vox",
  ("Fairlight CMI", add("Choir", bright=0.4, att=150, rel=900, bits=0.45, srr=0.25, g_revmix=0.3)), original="ARR1")
S("Can You Feel It Pad", 1986, "Deep House", "Can You Feel It", "Mr. Fingers", "Pad",
  ("Roland Juno-106", poly_pad(wave="Pulse", pw=0.45, pwm=0.35, cut=1700, res=0.12, env=0.15, att=350, rel=1500, chorus="II", sub=0.3)))
S("Trapped Bass", 1986, "House", "Trapped", "Colonel Abrams", "Bass", ("Roland Juno-106", sh_bass(cut=700, res=0.3, env=0.45, dec=260, sub=0.8)), notes=UNC)
S("What The", 1991, "Techno", "Mentasm", "Second Phase (Joey Beltram & Mundo Muzique)", "Lead", ("Roland Alpha Juno-2", hoover()), original="What The",
  notes="Il riff è costruito interamente sul preset 'What the...' di Eric Persing: nasce l'hoover.")
S("Fantasia", 1987, "Pop", "Hazy Shade of Winter", "The Bangles", "Pad",
  ("Roland D-50", fm6(29, [(1, 0, 0.8, 30, 8000, 1.0, 1800), (2, 0, 0.6, 30, 8000, 1.0, 1800), (1, 0, 0.7, 1, 900, 0.0, 900), (3, 0.5, 0.55, 1, 400, 0.0, 400),
                           (2, 0, 0.5, 1, 1500, 0.0, 1200), (7, 0, 0.4, 1, 300, 0, 300)], vel=0.5, aa=30, ad=3000, **{"as": 0.9}, ar=1800, uni=2, detune=0.06,
                      g_chomode="Ensemble", g_chomix=0.5, g_revmix=0.4)), original="Fantasia")
S("Staccato Heaven", 1987, "Rock", "Say You Will", "Foreigner", "Pluck", ("Roland D-50", fm_pluck(r=2, level=0.9, g_revmix=0.35, dlymix=0.25)), original="Staccato Heaven")
S("Horn Section", 1987, "Pop", "Tell It to My Heart", "Taylor Dayne", "Brass", ("Roland D-50", add("Brass", bright=0.7, att=15, dec=800, sus=0.7, rel=150, uni=1, g_revmix=0.15)),
  original="Horn Section")
S("Acid Tracks", 1987, "Acid House", "Acid Tracks", "Phuture", "Bass", ("Roland TB-303", acid303(cut=260, res=0.88, env=0.7, dec=260, dlymix=0.08)))
S("Your Love Arp", 1987, "House", "Your Love", "Frankie Knuckles & Jamie Principle", "Sequence",
  ("Roland Juno-60", pluck_a(cut=1600, res=0.2, dec=380, o1wave="Pulse", o2lvl=0.0, sub=0.3, g_chomode="I", g_chomix=0.5, dlymix=0.15, g_revmix=0.25)), notes=UNC)
S("Strings of Life Stab", 1987, "Techno", "Strings of Life", "Rhythim Is Rhythim (Derrick May)", "Stab",
  ("Korg M1", add("Strings", bright=0.6, att=5, dec=420, sus=0.0, rel=300, bits=0.3, g_revmix=0.25)),
  notes="Archi campionati da un'esecuzione di Michael James: strumento esatto non documentato. Ricreato su modulo digitale.")
S("Just Want Another Chance", 1988, "Techno", "Just Want Another Chance", "Reese (Kevin Saunderson)", "Bass",
  ("Casio CZ-5000", pd(wave="Saw", amt=0.55, uni=2, det=0.12, flt="LP24 Clean", cut=900, res=0.15, dec=2000, sus=1.0, rel=150, oct=-1, g_mode="Mono", env=0.15,
                       lfoshape="Sine", lforate=0.3, lfotim=0.2)), notes="Il 'Reese bass': due saw desintonizzate a distorsione di fase.")
S("Big Fun Lately Bass", 1988, "House", "Big Fun", "Inner City", "Bass",
  ("Yamaha TX81Z", fm_bass(alg=5, ratios=(0, 0, 1, 1, 1, 1), lv=(1.0, 0.68, 0.0, 0.0, 0, 0), dec=(2500, 600, 300, 180, 200, 200), sus=(0.55, 0.18, 0, 0, 0, 0), fb=0.55)),
  original="Lately Bass", notes="Lately Bass del TX81Z, citato dalla comunità per Big Fun e Good Life.")
S("Voodoo Ray", 1988, "Acid House", "Voodoo Ray", "A Guy Called Gerald", "Bass", ("Roland SH-101", sh_bass(cut=420, res=0.7, env=0.45, dec=200, sub=0.2, wave="Saw")))
S("Land of Confusion 303", 1988, "Acid House", "Land of Confusion", "Armando", "Bass", ("Roland TB-303", acid303(cut=330, res=0.8, env=0.55, dec=180)))
S("Pizzagogo", 1988, "Pop", "Orinoco Flow", "Enya", "Pluck",
  ("Roland D-50", fm6(5, [(1, 0, 1.0, 0.5, 500, 0, 400), (2, 0, 0.5, 0.5, 200, 0, 200), (2, 0, 0.6, 0.5, 300, 0, 400), (5, 0, 0.45, 0.5, 90, 0, 90)],
                      vel=0.6, ad=700, **{"as": 0.0}, ar=400, g_revmix=0.4, dlymix=0.15)), original="Pizzagogo")
S("Digital Native Dance", 1988, "Pop", "Waiting for a Star to Fall", "Boy Meets Girl", "Sequence",
  ("Roland D-50", wt("Bit Steps", pos=0.4, cut=5000, att=1, dec=1200, sus=0.6, rel=600, lfoshape="S&H", lforate=8, wlfo=0.8, lfocut=0.35,
                     dlymix=0.3, dlydiv="1/8", g_revmix=0.3)), original="Digital Native Dance")
S("Orinoco Living Calliope", 1996, "Eurodance", "Coco Jamboo", "Mr. President", "Lead",
  ("Roland D-50", add("Glass", bright=0.6, att=5, dec=900, sus=0.4, rel=300, g_revmix=0.2)), original="Living Calliope")
S("Metal Harp", 1989, "Pop", "Another Day in Paradise", "Phil Collins", "Pluck",
  ("Roland D-50", fm6(5, [(1, 0, 1.0, 0.5, 1500, 0, 900), (4, 0, 0.4, 0.5, 400, 0, 300), (1, 0.01, 0.8, 0.5, 1200, 0, 900), (7, 0, 0.3, 0.5, 200, 0, 200)],
                      vel=0.6, ad=2000, **{"as": 0.0}, ar=900, g_revmix=0.35, g_chomode="I")), original="Metal Harp")
S("Ride on Time Piano", 1989, "House", "Ride on Time", "Black Box", "Piano",
  ("Korg M1", add("Piano", bright=0.8, tilt=0.45, inh=0.2, att=1, dec=3500, sus=0.0, rel=350, g_chomode="I", g_chomix=0.2)), original="Piano 16'")
S("French Kiss Bass", 1989, "House", "French Kiss", "Lil Louis", "Bass",
  ("Yamaha TX81Z", fm_bass(alg=5, lv=(1.0, 0.6, 0, 0, 0, 0), fb=0.4, cut=600)), original="Lately Bass", notes=UNC)
S("Pacific State Lead", 1989, "House", "Pacific State", "808 State", "Lead",
  ("Roland Juno-106", an(o1wave="Pulse", o1pw=0.5, o2lvl=0.0, flt="LP24 Ladder", cut=4000, res=0.05, env=0.0, aa=30, ad=500, **{"as": 0.9}, ar=500,
                         lforate=5.2, lfopitch=0.15, lfodelay=300, g_chomode="I", dlymix=0.3, dlydiv="1/8D", g_revmix=0.4)), notes=UNC)
S("It's My Life Synth", 1984, "New Wave", "It's My Life", "Talk Talk", "Lead",
  ("Roland Juno-106", an(o1wave="Pulse", o1pw=0.3, pwm=0.25, o2lvl=0.0, sub=0.2, flt="LP24 Ladder", cut=2500, res=0.15, env=0.3, aa=5, ad=600, **{"as": 0.8}, ar=300,
                         lforate=0.6, g_chomode="II")), notes=UNC)
S("Sweet Dreams Riff", 1983, "Synth-Pop", "Sweet Dreams (Are Made of This)", "Eurythmics", "Sequence",
  ("Oberheim OB-X", pluck_a(cut=1300, res=0.2, dec=260, o2wave="Saw", o2fine=10, dlymix=0.1)), notes=UNC)
S("Wake Me Up Bass", 1984, "Pop", "Wake Me Up Before You Go-Go", "Wham!", "Bass", ("Roland Juno-106", sh_bass(cut=900, res=0.2, env=0.4, dec=200, sub=0.5)), notes=UNC)
S("We Close Our Eyes Bass", 1987, "Pop", "We Close Our Eyes", "Go West", "Bass", ("Roland SH-101", sh_bass(cut=550, res=0.35, env=0.55, dec=220, sub=0.9, wave="Saw")))
S("PPG Wave Pad", 1984, "Synth-Pop", "Wave pad (Tangerine Dream, Thomas Dolby)", "Thomas Dolby", "Pad",
  [("PPG Wave 2.3", wt("PPG Digital", pos=0.15, wenv=0.4, cut=4000, att=300, dec=2500, sus=0.9, rel=1600, uni=2, det=0.1, bits=0.3)),
   ("Roland Juno-60", poly_pad(wave="Saw", pwm=0.0, cut=900, res=0.35, att=400, rel=1600, chorus="Off", sub=0.0, level=0.45))],
  notes="PPG = wavetable digitale + filtro analogico SSM: layer di un modulo digitale e di uno analogico.")
S("Lately Bass", 1991, "Eurodance", "What Is Love", "Haddaway", "Bass",
  ("Yamaha TX81Z", fm_bass(alg=5, lv=(1.0, 0.68, 0.0, 0.0, 0, 0), dec=(2500, 600, 300, 180, 200, 200), sus=(0.55, 0.18, 0, 0, 0, 0), fb=0.55)),
  original="Lately Bass", notes="Citato in liste di appassionati insieme a Whigfield e Ace of Base.")
S("Solid Bass", 1990, "Techno", "Chime", "Orbital", "Bass",
  ("Yamaha DX21", fm_bass(alg=5, ratios=(0, 1, 1, 1, 1, 1), lv=(1.0, 0.62, 0, 0, 0, 0), dec=(1800, 380, 260, 150, 200, 200), sus=(0.5, 0.12, 0, 0, 0, 0), fb=0.3)),
  original="Solid Bass", notes="Solid Bass (DX21/DX100) è la versione originale del Lately Bass; Gearnews cita Chime di Orbital.")

# ---------------------------------------------------------------------------------------- 1990s
S("Vogue Piano", 1990, "House", "Vogue", "Madonna", "Piano",
  ("Korg M1", add("Piano", bright=0.72, tilt=0.4, inh=0.15, att=1, dec=4000, sus=0.0, rel=400, g_chomode="I", g_chomix=0.15)), original="Piano 16'")
S("Twin Peaks E.Piano", 1990, "Film / TV", "Twin Peaks Theme (Falling)", "Angelo Badalamenti", "Keys",
  ("Yamaha DX7", fm_epiano(bright=0.2, g_revmix=0.35, g_chomode="II")), original="E.PIANO 1", notes=DEB)
S("Universe", 1991, "Rock", "Don't Try So Hard", "Queen", "Pad",
  ("Korg M1", wt("Strings", pos=0.5, cut=4500, att=400, dec=3000, sus=1.0, rel=1800, uni=3, det=0.18, wlfo=0.25, lforate=0.2, g_chomode="Ensemble", g_chomix=0.4, g_revmix=0.35)),
  original="Universe")
S("Ooh/Aah", 1991, "Rock", "I'm Going Slightly Mad", "Queen", "Vox", ("Korg M1", add("Choir", bright=0.5, att=200, rel=1200, g_revmix=0.3)), original="Ooh/Aah")
S("M1 Choir", 1991, "Synth-Pop", "Pandora's Box", "OMD", "Vox", ("Korg M1", add("Choir", bright=0.55, att=150, rel=1000, g_revmix=0.3, g_chomode="I")), original="Choir")
S("Gypsy Woman Organ", 1991, "House", "Gypsy Woman (She's Homeless)", "Crystal Waters", "Organ",
  ("Korg M1", add("House Organ", bright=0.6, tilt=0.6, att=1, dec=500, sus=0.35, rel=100)), original="Organ 2")
S("Mentasm Hoover", 1991, "Techno", "Mentasm", "Second Phase", "Lead", ("Roland Alpha Juno-2", hoover(penv=-24, pdec=260, g_glide=240)), original="What The")
S("Dominator Hoover", 1991, "Rave", "Dominator", "Human Resource", "Lead", ("Roland Alpha Juno-2", hoover(cut=6000, res=0.2, g_drive=0.3)))
S("Get Ready Hoover", 1991, "Eurodance", "Get Ready for This", "2 Unlimited", "Lead", ("Roland Alpha Juno-2", hoover(cut=5000, g_glide=120)), notes=UNC)
S("Rhythm Is a Dancer Piano", 1992, "Eurodance", "Rhythm Is a Dancer", "Snap!", "Piano",
  ("Korg M1", add("Piano", bright=0.85, tilt=0.4, inh=0.2, att=1, dec=3000, sus=0.0, rel=300, g_chomode="I", g_chomix=0.2)), original="Piano 16'")
S("Lore", 1992, "Pop", "Sleeping Satellite", "Tasmin Archer", "Pad",
  ("Korg M1", wt("Vocal", pos=0.1, wlfo=0.4, lforate=0.25, cut=5000, att=500, dec=3000, sus=1.0, rel=2200, uni=2, det=0.1, g_revmix=0.45, g_revsize=0.85)), original="Lore")
S("Acperience", 1992, "Acid House", "Acperience 1", "Hardfloor", "Bass",
  ("Roland TB-303", acid303(cut=240, res=0.9, env=0.75, dec=320, lfocut=0.25, lforate=0.05, drive=0.55, dist="RAT", dgain=0.35, dtone=0.6, dlymix=0.2, dlydiv="1/8D")))
S("G-Funk Whistle", 1992, "Hip-Hop", "Nuthin' but a 'G' Thang", "Dr. Dre feat. Snoop Doggy Dogg", "Lead",
  ("Moog Minimoog Model D", lead(wave="Sine", o2lvl=0.0, cut=6000, res=0.0, env=0.0, glide=140, vib=0.3, lforate=5.5, fdrive=0.0, g_revmix=0.2)))
S("Organ 2", 1993, "House", "Show Me Love", "Robin S", "Organ",
  ("Korg M1", add("House Organ", bright=0.55, tilt=0.55, att=1, dec=900, sus=0.55, rel=120, oct=-1)), original="Organ 2",
  notes="Stonebridge trovò il basso scorrendo i preset dell'M1: Organ 2 (I17).")
S("Please Don't Go Organ", 1992, "Eurodance", "Please Don't Go", "Double You", "Organ",
  ("Korg M1", add("House Organ", bright=0.6, tilt=0.5, att=1, dec=600, sus=0.4, rel=110)), original="Organ 2")
S("Children Dream Organ", 1995, "Trance", "Children (Dream Version)", "Robert Miles", "Organ",
  ("Korg M1", add("House Organ", bright=0.5, tilt=0.6, att=1, dec=700, sus=0.4, rel=200, g_revmix=0.35, dlymix=0.2, dlydiv="1/8D")), original="Organ 2")
S("Higher State 303", 1995, "Acid House", "Higher State of Consciousness", "Josh Wink", "Bass",
  ("Roland TB-303", acid303(cut=200, res=0.93, env=0.8, dec=150, lfocut=0.7, lforate=0.03, drive=0.6)), notes="Due 303 non modificate, senza pedali.")
S("Da Funk Lead", 1995, "House", "Da Funk", "Daft Punk", "Lead",
  ("Korg MS-20", an(o1wave="Saw", o2wave="Saw", o2lvl=0.9, o2semi=7, flt="LP12", cut=900, res=0.45, env=0.35, fd=500, fs=0.3, aa=1, ad=600, **{"as": 0.9}, ar=120,
                    fdrive=0.7, dist="Tube", dgain=0.5, g_mode="Mono", g_glide=40, g_drive=0.3)),
  notes="Saw in quinta e leggero inviluppo sul filtro dell'MS-20 (Splice); altre fonti parlano di 303 distorta.")
S("Born Slippy Chords", 1996, "Techno", "Born Slippy .NUXX", "Underworld", "Chord",
  ("Roland Juno-106", poly_stab(cut=2200, res=0.1, env=0.3, dec=1500, chorus="I", dlymix=0.25, g_revmix=0.3)))
S("The Bells", 1996, "Techno", "The Bells", "Jeff Mills", "Bell",
  ("Korg Wavestation", fm6(5, [(1, 0, 1.0, 0.5, 900, 0, 400), (3, 0.5, 0.55, 0.5, 600, 0, 300), (1, 0, 0.6, 0.5, 700, 0, 400), (2, 0, 0.5, 0.5, 400, 0, 300)],
                           vel=0.4, ad=1200, **{"as": 0.0}, ar=400, g_revmix=0.15)),
  notes="Strumento originale non documentato (citati DX100, K2000, campionatore): ricreato in FM.")
S("Coco Jamboo Calliope", 1996, "Eurodance", "Coco Jamboo", "Mr. President", "Lead",
  ("Roland D-50", add("Glass", bright=0.65, att=5, dec=800, sus=0.5, rel=250, lfopitch=0.1, lforate=5.5, g_revmix=0.2)), original="Living Calliope")
S("Everybody Needs a 303", 1997, "Rave", "Everybody Needs a 303", "Fatboy Slim", "Bass",
  ("Roland TB-303", acid303(cut=450, res=0.72, env=0.55, dec=240, dist="DS-1", dgain=0.4)))
S("For an Angel Supersaw", 1998, "Trance", "For an Angel (1998 remix)", "Paul van Dyk", "Pluck",
  ("Roland JP-8000", supersaw(det=0.35, mix=0.55, cut=1500, att=1, dec=280, sus=0.0, rel=250, env=0.45, fd=250, fs=0.0, dlymix=0.3, dlydiv="1/8D")))
S("Out of the Blue Supersaw", 1999, "Trance", "Out of the Blue", "System F (Ferry Corsten)", "Lead",
  ("Roland JP-8000", supersaw(det=0.5, mix=0.7, cut=10000, dec=800)), notes="Supersaw del JP-8000 'nella sua forma più pura' (ClassicTrance).")
S("Sandstorm", 1999, "Trance", "Sandstorm", "Darude", "Pad",
  ("Roland JP-8080", supersaw(det=0.4, mix=0.6, cut=4000, att=40, dec=1500, sus=0.9, rel=600, g_revmix=0.3)), original="Sandstorm",
  notes="Il primo suono che il JP-8080 carica all'accensione si chiama Sandstorm: gli accordi del brano, che ne prese il nome.")
S("Sandstorm Lead", 1999, "Trance", "Sandstorm", "Darude", "Lead",
  [("Roland JP-8080", supersaw(det=0.28, mix=0.45, cut=5500, att=1, dec=150, sus=0.85, rel=80, env=0.25, g_drive=0.2, dlymix=0.15)),
   ("Access Virus", wt("Analog Morph", pos=0.66, cut=4000, att=1, dec=150, sus=0.8, rel=80, uni=3, det=0.15, level=0.4))],
  notes="Sandstorm: Access Virus C, Korg MS2000R e Roland JP-8080 (Sound On Sound).")
S("Knights of the Jaguar Strings", 1999, "Techno", "Knights of the Jaguar", "DJ Rolando (UR)", "Strings",
  ("Korg Triton", add("Strings", bright=0.55, att=120, dec=2000, sus=1.0, rel=900, g_chomode="Ensemble", g_chomix=0.45, g_revmix=0.3)), notes=UNC)
S("Blue Lead", 1999, "Eurodance", "Blue (Da Ba Dee)", "Eiffel 65", "Lead",
  ("Clavia Nord Lead", wt("Pulse Width", pos=0.3, cut=5000, att=1, dec=300, sus=0.6, rel=150, uni=2, det=0.1, dlymix=0.15)),
  notes="Lo strumento del lead non è documentato: ricreato su VA digitale.")
S("Insomnia Pluck", 1995, "Trance", "Insomnia", "Faithless", "Pluck",
  ("Roland JP-8000", wt("Analog Morph", pos=0.66, wenv=-0.3, cut=1800, res=0.3, env=0.55, att=1, dec=260, sus=0.0, rel=200, uni=2, det=0.1, fs=0.0, fd=220,
                        dlymix=0.25, dlydiv="1/8D", g_revmix=0.25)), notes="Molti synth successivi hanno un preset chiamato 'Insomnia'. " + UNC)

# ---------------------------------------------------------------------------------------- 2000s
S("Try Again 303", 2000, "R&B", "Try Again", "Aaliyah", "Bass",
  ("Roland TB-303", acid303(cut=380, res=0.65, env=0.45, dec=250, glide=50)), notes="Forse l'unico 303 in un singolo n.1 della Billboard Hot 100.")
S("Everything In Its Right Place", 2000, "Indie", "Everything In Its Right Place", "Radiohead", "Keys",
  ("Sequential Prophet-5", an(o1wave="Pulse", o1pw=0.35, o2wave="Saw", o2lvl=0.4, o2fine=6, flt="LP24 Ladder", cut=1400, res=0.15, env=0.3, aa=3, ad=1200, **{"as": 0.7}, ar=400,
                              g_chomode="I", g_revmix=0.25)))
S("Real Slim Shady Keys", 2000, "Hip-Hop", "The Real Slim Shady", "Eminem", "Keys",
  ("Korg Triton", fm6(1, [(1, 0, 1.0, 0.5, 600, 0.2, 100), (3, 0, 0.55, 0.5, 200, 0.05, 100)], vel=0.6, ad=900, **{"as": 0.3}, ar=100)), notes=UNC)
S("Grindin' Triton Bass", 2002, "Hip-Hop", "Grindin'", "Clipse (The Neptunes)", "Bass",
  ("Korg Triton", add("Soft Pad", bright=0.2, att=1, dec=1500, sus=0.0, rel=200, oct=-1, penv=-3, pdec=40, g_mode="Mono", g_drive=0.3)),
  notes="I Neptunes e il Korg Triton: " + UNC)
S("Satisfaction Bass", 2002, "Electro", "Satisfaction", "Benny Benassi", "Bass",
  ("Access Virus", wt("Pulse Width", pos=0.35, cut=900, res=0.45, env=0.45, fd=400, fs=0.3, att=1, dec=400, sus=0.7, rel=80, oct=-1, g_drive=0.7, g_mode="Mono")),
  notes=DEB)
S("Strobe Pluck", 2009, "EDM", "Strobe", "deadmau5", "Pluck",
  ("LennarDigital Sylenth1", wt("Pulse Width", pos=0.1, cut=1100, res=0.3, env=0.5, att=1, dec=400, sus=0.0, rel=400, uni=3, det=0.15, fs=0.0, fd=350,
                                dlymix=0.35, dlydiv="1/8D", g_revmix=0.4)), notes="Onda quadra 'larga' nel mix; synth originale incerto (Sylenth1 citato).")
S("Scary Monsters Screamer", 2010, "Dubstep", "Scary Monsters and Nice Sprites", "Skrillex", "Bass",
  ("LennarDigital Sylenth1", wt("Growl", pos=0.7, wenv=0.3, cut=3500, res=0.45, env=0.35, att=1, dec=300, sus=0.8, rel=60, uni=3, det=0.15, oct=-1, lforate=8,
                                lfoshape="S&H", lfotim=0.7, lfocut=0.4, g_drive=0.7, g_mode="Mono")), original="LD Screamer",
  notes="Un thread KVR attribuisce il suono a Sylenth1, preset LD Screamer (fonte forum).")
S("Doomsday Wobble", 2011, "Dubstep", "Doomsday", "Nero", "Bass",
  ("NI Massive", wt("Growl", pos=0.3, cut=500, res=0.4, env=0.0, att=1, dec=1000, sus=1.0, rel=80, uni=2, det=0.1, oct=-1, lfoshape="Sine",
                    lforate=3.0, lfocut=0.8, wlfo=0.6, g_drive=0.5, g_mode="Mono")), notes="Secondo la comunità i wobble di Nero nascono in Massive.")
S("Veldt Synth", 2012, "EDM", "The Veldt", "deadmau5", "Chord",
  ("NI Massive", supersaw(det=0.3, mix=0.5, cut=3000, att=5, dec=600, sus=0.5, rel=300, env=0.3, g_revmix=0.3)), notes=UNC)
S("Levels Lead", 2011, "EDM", "Levels", "Avicii", "Lead",
  ("LennarDigital Sylenth1", supersaw(det=0.45, mix=0.6, cut=6000, dec=250, sus=0.7, rel=200, env=0.25, fd=250, g_revmix=0.3, dlymix=0.15)),
  notes="Avicii usava Sylenth1; il synth del brano non è documentato con certezza.")
S("Secrets Lead", 2015, "EDM", "Secrets", "Tiësto & KSHMR", "Lead",
  ("LennarDigital Sylenth1", supersaw(det=0.5, mix=0.7, cut=7000, dec=200, sus=0.8, rel=120, penv=-12, pdec=25, g_revmix=0.35, g_drive=0.25)))
S("Immunity MS-20", 2013, "Ambient", "Open Eye Signal (Immunity)", "Jon Hopkins", "Bass",
  ("Korg MS-20", sh_bass(cut=300, res=0.6, env=0.5, dec=300, sub=0.3, wave="Saw", flt="LP12", fdrive=0.8, lfocut=0.3, lforate=0.1)))
S("Powers of Ten Lead", 2015, "Techno", "Powers of Ten", "Stephan Bodzin", "Lead",
  ("Moog Sub 37", lead(wave="Saw", cut=900, res=0.45, env=0.55, glide=120, fd=900, fs=0.3, g_revmix=0.35, dlymix=0.3, dlydiv="1/8D")))
S("Finesse ORCH5", 2017, "R&B", "Finesse", "Bruno Mars", "Stab",
  ("Fairlight CMI", add("Brass", bright=0.85, att=1, dec=380, sus=0.0, rel=200, bits=0.5, srr=0.3, oct=-1, g_revmix=0.2)), original="ORCH5")
S("Versace on the Floor", 2017, "R&B", "Versace on the Floor", "Bruno Mars", "Keys", ("Yamaha DX7", fm_epiano(bright=0.35, g_chomode="II", g_chomix=0.4, g_revmix=0.3)),
  original="E.PIANO 1")
S("Eventually Pad", 2015, "Indie", "Eventually", "Tame Impala", "Pad",
  ("Roland Juno-106", poly_pad(wave="Saw", pwm=0.0, cut=1300, res=0.2, att=500, rel=2000, chorus="II", g_revmix=0.4)), notes=UNC)
S("Blinding Lights Lead", 2019, "Synthwave", "Blinding Lights", "The Weeknd", "Lead",
  ("Roland Juno-60", an(o1wave="Pulse", o1pw=0.35, pwm=0.2, o2wave="Saw", o2lvl=0.4, o2fine=8, flt="LP24 Ladder", cut=3200, res=0.1, env=0.2, aa=2, ad=500, **{"as": 0.8},
                        ar=300, g_chomode="I", dlymix=0.25, dlydiv="1/8D", g_revmix=0.35)), notes="La maggior parte degli esperti indica il Roland Juno-60 (MusicRadar).")
S("Break My Soul Organ", 2022, "House", "Break My Soul", "Beyoncé", "Organ",
  [("Korg M1", add("House Organ", bright=0.55, tilt=0.55, att=1, dec=800, sus=0.5, rel=120, oct=-1)),
   ("Korg M1", add("Piano", bright=0.75, tilt=0.45, att=1, dec=3000, sus=0.0, rel=300, level=0.5))], original="Organ 2",
  notes="M1 Organ e M1 Piano insieme (Sonicstate): due moduli digitali in layer.")

S("Stranger Things Arp", 2016, "Synthwave", "Stranger Things (main theme)", "Kyle Dixon & Michael Stein", "Sequence",
  ("Sequential Prophet-5", pluck_a(cut=900, res=0.4, env=0.5, dec=260, o2wave="Saw", g_revmix=0.35, dlymix=0.25)), notes=UNC)
S("Cola Bass", 2017, "Tech House", "Cola", "CamelPhat & Elderbrook", "Bass",
  ("Xfer Serum", wt("Analog Morph", pos=0.66, cut=450, res=0.3, env=0.35, att=1, dec=300, sus=0.5, rel=80, oct=-1, uni=2, det=0.08, g_mode="Mono",
                    lfocut=0.2, lforate=0.2)), notes="Synth originale non documentato: basso ricreato nello stile del brano.")
S("Losing It Bass", 2018, "Tech House", "Losing It", "FISHER", "Bass",
  ("Xfer Serum", wt("Growl", pos=0.25, wenv=0.25, cut=700, res=0.15, env=0.4, att=1, dec=160, sus=0.35, rel=60, oct=-1, g_drive=0.45, g_mode="Mono")),
  notes="Synth originale non documentato: basso ricreato nello stile del brano.")
S("Nanana Piano", 2023, "House", "(It Goes Like) Nanana", "Peggy Gou", "Piano",
  ("Korg M1", add("Piano", bright=0.78, tilt=0.45, inh=0.18, att=1, dec=3200, sus=0.0, rel=350, g_chomode="I", g_chomix=0.15, g_revmix=0.12)),
  notes="Revival del piano house anni '90 in stile M1: " + UNC)
S("Doppler Acid", 2021, "Techno", "Doppler", "Charlotte de Witte", "Bass",
  ("Roland TB-303", acid303(cut=300, res=0.86, env=0.65, dec=200, drive=0.6, dist="RAT", dgain=0.55, dtone=0.45, g_drive=0.2, dlymix=0.15)),
  notes="Linea acid in stile 303 con distorsione RAT, tipica dell'acid techno; " + UNC)
S("Kids Lead", 2008, "Indie", "Kids", "MGMT", "Lead",
  ("Roland Juno-60", lead(wave="Pulse", o1pw=0.4, o2lvl=0.0, cut=3500, res=0.1, glide=0, g_mode="Poly", g_chomode="II", g_revmix=0.2)), notes=UNC)
S("Bulletproof Synth", 2009, "Synth-Pop", "Bulletproof", "La Roux", "Lead",
  ("Roland SH-101", lead(wave="Pulse", o1pw=0.3, o2lvl=0.0, cut=2500, res=0.25, glide=0, sub=0.4)), notes=UNC)
S("Nightcall Bass", 2010, "Synthwave", "Nightcall", "Kavinsky", "Bass", ("Moog Minimoog Model D", moog_bass(cut=600, res=0.2, env=0.4, dec=500, sus=0.4)), notes=UNC)
S("Don't You Worry Child Lead", 2012, "EDM", "Don't You Worry Child", "Swedish House Mafia", "Lead",
  ("LennarDigital Sylenth1", supersaw(det=0.5, mix=0.7, cut=8000, dec=900, g_revmix=0.35, dlymix=0.2)), notes=UNC)
S("Latch Chords", 2013, "House", "Latch", "Disclosure feat. Sam Smith", "Chord",
  ("Roland Juno-106", poly_stab(cut=1600, res=0.15, env=0.4, dec=450, chorus="II", g_revmix=0.2)), notes=UNC)
S("Bad Guy Bass", 2019, "Pop", "bad guy", "Billie Eilish", "Bass",
  ("Xfer Serum", wt("Analog Morph", pos=0.0, cut=2000, att=1, dec=800, sus=0.6, rel=100, oct=-1, g_drive=0.35, g_mode="Mono")), notes=UNC)
S("Physical Synth", 2020, "Pop", "Physical", "Dua Lipa", "Lead",
  ("Roland Juno-106", an(o1wave="Saw", o2wave="Pulse", o2lvl=0.5, o2fine=6, flt="LP24 Ladder", cut=2200, res=0.2, env=0.35, aa=1, ad=400, **{"as": 0.7}, ar=200,
                         dlymix=0.2, dlydiv="1/8D", g_chomode="I")), notes=UNC)
S("Rumble Sub", 2022, "Techno", "Rumble techno (es. Amelie Lens - Higher)", "Amelie Lens", "Bass",
  ("Moog Sub 37", an(o1wave="Sine", oct=-1, o2lvl=0.0, flt="LP24 Ladder", cut=200, res=0.2, aa=1, ad=900, **{"as": 0.0}, ar=600, penv=5, pdec=40, fdrive=0.8,
                     g_revmix=0.5, g_revsize=0.8, g_revdamp=0.9, g_drive=0.4)), notes=UNC)

# ------------------------------------------------------------------------------------------------ resolve + write
levels = json.load(open(LEVELS)) if os.path.exists(LEVELS) else {}
models_used = set()
out, seen = [], set()
for pr in P:
    if pr["name"] in seen: raise SystemExit("duplicate " + pr["name"])
    seen.add(pr["name"])
    if pr["genre"] not in GENRES: raise SystemExit("genre %s (%s)" % (pr["genre"], pr["name"]))
    if pr["cat"] not in CATS: raise SystemExit("cat " + pr["cat"])
    mods, glob = [], {}
    for slot, (model, params) in enumerate(pr["modules"]):
        if model not in KIND: raise SystemExit("unknown instrument %r" % model)
        eng = params.get("engine", "Analog")
        if (KIND[model] == "A") != (eng == "Analog"):
            raise SystemExit("engine/instrument mismatch in %s: %s is %s but engine %s" % (pr["name"], model, KIND[model], eng))
        mp = {}
        for k, v in params.items():
            if v is None: continue
            if k.startswith("g_"): glob[k] = v
            else: mp[k] = v
        models_used.add(model)
        mods.append(dict(slot=slot, model=model, p=mp))
    if pr["name"] in levels: glob["g_trim"] = round(max(-36.0, min(36.0, levels[pr["name"]])), 2)
    synth = " + ".join(dict.fromkeys(m[0] for m in pr["modules"]))
    out.append(dict(name=pr["name"], original=pr["original"], year=pr["year"], decade="%ds" % (pr["year"] // 10 * 10), genre=pr["genre"],
                    song=pr["song"], artist=pr["artist"], synth=synth, cat=pr["cat"], notes=pr["notes"], modules=mods, g=glob))
out.sort(key=lambda x: (x["year"], x["name"]))
models = [dict(name=n, kind=k, skin=s, maker=mk, year=y) for (n, k, s, mk, y) in MODELS if n in models_used]
decades = sorted({o["decade"] for o in out})
json.dump(dict(version=2, genres=GENRES, decades=decades, models=models, presets=out), open(OUT, "w"), indent=1, ensure_ascii=False)

md = ["# 5ynth - libreria dei preset (%d)\n" % len(out),
      "Ordinata per anno. Ogni preset: nome (genere - brano più famoso che ha usato quel suono), strumento originale, moduli usati. ",
      "**Nome di fabbrica** = nome reale del preset sullo strumento, quando il suono è un preset di fabbrica documentato.\n",
      "| Anno | Preset | Genere | Brano | Artista | Strumento | Moduli | Nome di fabbrica | Note |", "|---|---|---|---|---|---|---|---|---|"]
for o in out:
    md.append("| %d | %s | %s | %s | %s | %s | %s | %s | %s |" % (o["year"], o["name"], o["genre"], o["song"], o["artist"], o["synth"],
              "+".join("ABCDE"[m["slot"]] for m in o["modules"]), o["original"], o["notes"].replace("|", "/")))
os.makedirs(os.path.dirname(DOC), exist_ok=True)
open(DOC, "w").write("\n".join(md) + "\n")

from collections import Counter
print("presets:", len(out), " models used:", len(models_used), "/", len(MODELS))
print("genres:", Counter(o["genre"] for o in out).most_common())
print("decades:", Counter(o["decade"] for o in out))
print("layers:", sum(len(o["modules"]) > 1 for o in out), " factory names:", sum(bool(o["original"]) for o in out))
