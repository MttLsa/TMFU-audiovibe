#!/usr/bin/env python3
"""
Builds docs/5ynth_Manuale_v<version>.pdf (Italian user manual) with reportlab.
Inputs: params.json (FivesynthTest <dir> params), resources/presets.json, docs/gui_*.png, docs/logo_5ynth.png (FivesynthTest <dir> logo)
Usage:  python3 tools/make_manual.py <params.json> [version]
"""
import json, os, re, sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.fonts import addMapping
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak, KeepTogether)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
params = json.load(open(sys.argv[1]))
version = sys.argv[2] if len(sys.argv) > 2 else "2.1.0"
lib = json.load(open(os.path.join(ROOT, "resources", "presets.json")))
OUT = os.path.join(ROOT, "docs", "5ynth_Manuale_v%s.pdf" % version)

# the plug-in's own interface font
FONTS = os.path.join(ROOT, "resources", "fonts")
pdfmetrics.registerFont(TTFont("Urbanist", os.path.join(FONTS, "Urbanist-Light.ttf")))
pdfmetrics.registerFont(TTFont("Urbanist-Bold", os.path.join(FONTS, "Urbanist-SemiBold.ttf")))
for b, i, name in ((0, 0, "Urbanist"), (1, 0, "Urbanist-Bold"), (0, 1, "Urbanist"), (1, 1, "Urbanist-Bold")):
    addMapping("Urbanist", b, i, name)

ss = getSampleStyleSheet()
ACC = colors.HexColor("#e07b2a")      # analog orange
DIG = colors.HexColor("#1f8fbf")      # digital blue
H1 = ParagraphStyle("h1", parent=ss["Heading1"], fontName="Urbanist-Bold", fontSize=19, leading=23, textColor=ACC, spaceAfter=8)
H2 = ParagraphStyle("h2", parent=ss["Heading2"], fontName="Urbanist-Bold", fontSize=13, leading=16, textColor=DIG, spaceBefore=8, spaceAfter=4)
P = ParagraphStyle("p", parent=ss["BodyText"], fontName="Urbanist", fontSize=10, leading=13.5)
SMALL = ParagraphStyle("s", parent=P, fontSize=7.5, leading=9.2)
SMALLB = ParagraphStyle("sb", parent=SMALL, fontName="Urbanist-Bold")
BUL = ParagraphStyle("b", parent=P, leftIndent=11, bulletIndent=2)
CAP = ParagraphStyle("cap", parent=P, fontSize=8.5, leading=11, textColor=colors.HexColor("#555555"), alignment=1)
CODE = ParagraphStyle("code", parent=P, fontName="Courier", fontSize=8.5, leading=11)

story = []
def h1(t): story.append(Paragraph(t, H1))
def h2(t): story.append(Paragraph(t, H2))
def p(t): story.append(Paragraph(t, P))
def bullets(items):
    for it in items: story.append(Paragraph(it, BUL, bulletText="•"))
def img(name, w=170, caption=None, folder="docs"):
    path = os.path.join(ROOT, folder, name)
    if not os.path.exists(path): return
    from PIL import Image as PI
    iw, ih = PI.open(path).size
    parts = [Image(path, width=w * mm, height=w * mm * ih / iw, mask="auto")]
    if caption: parts.append(Paragraph(caption, CAP))
    parts.append(Spacer(1, 5))
    story.append(KeepTogether(parts))
def table(rows, widths, head=True, style=SMALL):
    data = [[Paragraph(str(c), SMALLB if (head and r == 0) else style) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if head else 0)
    st = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("GRID", (0, 0), (-1, -1), 0.25, colors.HexColor("#c8c8c8")),
          ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 3),
          ("TOPPADDING", (0, 0), (-1, -1), 1.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 1.5)]
    if head: st.append(("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#eeeeee")))
    t.setStyle(TableStyle(st))
    story.append(t)
    story.append(Spacer(1, 6))

# ------------------------------------------------------------------------------------------- cover
story.append(Spacer(1, 14 * mm))
img("logo_5ynth.png", 170)
story.append(Paragraph("5-in-1 synthesizer: fino a 5 synth analogici e digitali con i suoni famosi dal 1960 al 2020",
                       ParagraphStyle("st", parent=P, fontSize=14, leading=18, alignment=1)))
story.append(Spacer(1, 6))
story.append(Paragraph("TMFU audiovibe  ·  Manuale d'uso  ·  versione %s  ·  VST3 / AU / Standalone" % version,
                       ParagraphStyle("c", parent=P, alignment=1)))
story.append(Spacer(1, 10))
img("gui_Organ_2.png", 170)
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- 1 intro
h1("1. Cos'è 5ynth")
p("<b>5ynth</b> (prima si chiamava SynthZilla) è uno strumento virtuale che ricrea <b>per sintesi</b>, senza campioni, i suoni di synth più famosi della "
  "storia. Ogni preset è legato al <b>brano più famoso che ha usato quel suono</b> e la libreria (%d preset, dal %d al %d) "
  "è ordinata per anno. Un suono può usare fino a <b>5 moduli synth</b> (A–E) suonati insieme in layer: ogni modulo imita "
  "uno strumento (Minimoog, TB-303, DX7, Juno, M1, PPG, Virus…) con il suo motore di sintesi e il suo aspetto grafico."
  % (len(lib["presets"]), min(x["year"] for x in lib["presets"]), max(x["year"] for x in lib["presets"])))
h2("Installazione (macOS)")
bullets([
    "Apri <i>5ynth-%s-macOS.pkg</i> (Mac Intel e Apple Silicon). Installa l'<b>AU</b> in /Library/Audio/Plug-Ins/Components, "
    "il <b>VST3</b> in /Library/Audio/Plug-Ins/VST3 e l'app <b>Standalone</b> in /Applicazioni." % version,
    "Il pacchetto non è firmato con un certificato Apple: se macOS lo blocca, clic destro sul file → <b>Apri</b>, oppure "
    "Impostazioni di Sistema → Privacy e sicurezza → <b>Apri comunque</b>.",
    "Chiudi la DAW prima di installare un aggiornamento. Nella DAW il plugin compare sotto <b>TMFU audiovibe</b> come strumento "
    "(AU <i>aumu Szla Tmfu</i>) con il nome <b>5ynth</b>. I progetti salvati con SynthZilla ritrovano il plugin con tutte le impostazioni: "
    "i codici di identificazione non sono cambiati.",
])
h2("Primi passi")
bullets([
    "Carica 5ynth su una traccia strumento e suona: parte con il preset <b>Organ 2</b> (House, Show Me Love).",
    "Usa <b>&lt;</b> e <b>&gt;</b> per scorrere i preset in ordine di anno, oppure <b>BROWSER</b> per cercarli.",
    "Clicca una scheda modulo (A, B…) per vederne e modificarne i parametri; con <b>+ CUSTOM</b> aggiungi moduli fino a E.",
    "Tutti i preset sono normalizzati con picco a −1 dBFS: passando da un preset all'altro il volume resta simile.",
])

# ------------------------------------------------------------------------------------------- 2 interface
h1("2. L'interfaccia")
img("gui_fullscreen_1920x1080.png", 170, "La finestra a schermo intero (1920 × 1080): lo spazio in più va ai moduli.")
h2("Testata")
bullets([
    "<b>Logo 5YNTH</b> (vettoriale, nitido a ogni dimensione): il 5 ha la barra superiore a forma di tastiera e cinque barre "
    "luminose, i 5 synth. Un clic sul logo lo apre in grande, un altro clic lo chiude.",
    "<b>Nome del preset</b> (genere – brano) e, sotto, anno, artista, strumento originale, moduli usati e nome del preset di fabbrica "
    "quando il suono è un preset documentato.",
    "<b>&lt; &gt;</b>: preset precedente / successivo. <b>BROWSER</b>: apre la libreria (capitolo 5).",
    "<b>OUT</b>: meter del livello d'uscita (rosso vicino a 0 dBFS).",
])
h2("Schede dei moduli")
bullets([
    "Ogni scheda (A–E) ha: pulsante con la lettera (selezione), menu <b>Synth Model</b>, <b>LVL</b> (livello) e <b>PAN</b>. "
    "La barra colorata in basso mostra il livello del modulo (arancione = analogico, azzurro = digitale).",
    "<b>+ CUSTOM</b> aggiunge un modulo (fino a 5); la <b>x</b> sull'ultima scheda lo toglie (minimo 2 schede).",
    "Il pulsante <b>ON</b> nel pannello accende o spegne il modulo: un modulo spento non suona.",
])
h2("Finestra")
bullets([
    "La finestra si ridimensiona liberamente trascinando l'angolo in basso a destra, e la dimensione viene ricordata. "
    "Ingrandita o a schermo intero occupa tutto lo schermo: l'altezza in più va all'area dei moduli, la larghezza in più "
    "alle aree che si allargano (moduli, LFO, tastiera).",
    "La tastiera in basso suona con il mouse e mostra le note MIDI ricevute.",
])
h2("Manopole, fader e menu")
bullets([
    "Trascina in verticale per cambiare il valore; <b>Shift</b> mentre trascini = movimento più fine.",
    "Dopo un clic, le <b>frecce</b> della tastiera muovono di 1 % (fader 0,1 %); con Shift 10 volte più fine.",
    "<b>Doppio clic</b> = valore di default. La risoluzione è di 0,01 sulle manopole e di 0,001 sui fader.",
    "Gli strumenti che sul pannello originale hanno fader (Juno, CS-80, Alpha Juno, SH-101…) li mostrano anche qui, più lunghi.",
])

# ------------------------------------------------------------------------------------------- 3 modules
story.append(PageBreak())
h1("3. I moduli synth")
p("Il menu <b>Synth Model</b> di una scheda elenca tutti gli strumenti presenti nella libreria. Scegliendone uno, il modulo "
  "carica il <b>suono caratteristico</b> di quello strumento (il modulo del primo preset che lo usa) e ne prende l'aspetto: "
  "34 skin ispirate ai pannelli originali (legno Moog/Prophet, fader Juno, TB-303 argento, membrane e LCD DX7/M1, PPG blu, "
  "Virus e Nord rossi, CRT Fairlight…), senza loghi o grafiche originali.")
p("Strumento originale analogico → motore <b>Analog</b>; digitale → uno dei motori digitali. Gli strumenti ibridi o i "
  "brani con più synth sovrapposti usano più moduli in layer.")
h2("Motori (Engine)")
table([
    ["Engine", "Sintesi", "Strumenti tipici"],
    ["Analog", "2 oscillatori (Saw, Pulse, Triangle, Sine) + sub, rumore, sync, cross mod, ring mod, PWM, drift", "Minimoog, Prophet-5, Juno, Jupiter-8, OB-X, TB-303, SH-101, MS-20, CS-80"],
    ["FM 6-op", "6 operatori, 32 algoritmi in stile DX7, feedback, pannello come sul DX7", "DX7, TX-816, DX21, TX81Z"],
    ["Wavetable", "12 tabelle d'onda con posizione modulabile da inviluppo e LFO", "PPG Wave, Massive, Serum, Wavestation"],
    ["Supersaw", "7 saw desintonizzate (Detune, Mix)", "JP-8000 / JP-8080, Virus, Sylenth1"],
    ["Phase Dist", "distorsione di fase in stile Casio CZ (forme d'onda risonanti)", "Casio CZ-5000"],
    ["Additive", "fino a 40 parziali con 11 modelli (organo, piano, archi, coro, campana…)", "M1, D-50, Synclavier, Fairlight, string machine"],
], [24, 78, 68])
h2("Sezioni del pannello")
p("Le sezioni visibili dipendono dal motore: quelle che il motore non usa vengono nascoste.")
table([
    ["Sezione", "Controlli"],
    ["PITCH", "Oct (±3 ottave), Semi (±12), Fine (±100 cent)"],
    ["OSC 1 / OSC 2", "Wave, PW (larghezza d'impulso), Level; OSC 2 anche Oct, Semi, Fine"],
    ["MIXER / MOD", "Sub, Noise, Sync, X-Mod (cross modulation), Ring, PWM"],
    ["FM OPERATORS", "schema dell'algoritmo, Algorithm, Feedback, Vel>Index; per ogni operatore Coarse, Fine, Level, Attack, Decay, Sustain, Release"],
    ["WAVETABLE", "Table, Position, Env>Pos, LFO>Pos"],
    ["SUPERSAW", "Detune, Mix"],
    ["PHASE DISTORTION", "Wave, DCW (quantità), Env>DCW"],
    ["ADDITIVE", "Model, Bright, Dec.Tilt (decadimento delle parziali alte), Inharm (inarmonicità), Even (parziali pari)"],
    ["UNISON", "Voices (1–7), Detune, Drift (instabilità analogica)"],
    ["FILTER LP / BP / HP", "Type (LP24 Ladder, LP 303 Diode, LP12, BP12, HP12, LP24 Clean, Notch, Off), Cutoff, Reso, Drive, Env, Key, Vel"],
    ["DRIVE (PEDAL)", "Model (Off, Tube, RAT, DS-1), Gain, Tone, Mix"],
    ["FILTER ENV / AMP ENV", "A, D, S, R"],
    ["LFO", "Shape, Rate, Delay, Pitch, Cutoff, PWM/Tmb (timbro: PWM, posizione wavetable…)"],
    ["PITCH ENV / ACCENT", "P.Env (±48 semitoni), P.Dec, Accent (note con velocity alta)"],
    ["LO-FI", "Bits (bit crush), Rate (riduzione della frequenza di campionamento)"],
    ["DELAY", "Time (1/16 … 1/2, sincronizzato), Feedbk, Mix, Ping (ping-pong)"],
    ["OUT", "Level, Pan del modulo"],
], [38, 132])
h2("FM a 6 operatori")
p("Parametri come sul DX7 — COARSE / FINE, OUT LVL 0–99, rate R1 / R2 / R4 (99 = veloce), livello L3 0–99, "
  "FEEDBACK 0–7, celle a membrana con LCD.")
img("gui_FM_5_modules.png", 150, "Modulo FM con 5 moduli attivi.")
h2("TB-303")
bullets([
    "<b>LP 303 Diode</b>: diode ladder a 4 poli con il primo polo un'ottava sopra gli altri, passa-alto nel percorso di "
    "risonanza come nel circuito originale (lo “squelch” che toglie i bassi invece di gonfiarli), sovracampionato 2×.",
    "<b>Accent</b> con il circuito “sweep”: accent consecutivi si accumulano; sulle note accentate il decay del filtro è di 200 ms.",
    "<b>Drive</b>: modelli dei pedali usati nell'acid — <b>RAT</b> (ProCo RAT) e <b>DS-1</b> (Boss), sovracampionati 2×.",
])
img("gui_Acid_Tracks.png", 150, "Acid Tracks: TB-303 con filtro diode e accent.")

# ------------------------------------------------------------------------------------------- 4 global
story.append(PageBreak())
h1("4. Sezione globale")
p("La fascia in basso agisce su tutto il plugin. <b>LFO, EQ e filtri d'uscita non cambiano quando carichi un preset</b>: "
  "puoi impostarli una volta e sfogliare i suoni.")
h2("VOICE")
bullets([
    "<b>Mode</b>: <b>Poly</b> (16 voci), <b>Mono</b> (una voce, ogni nota riparte con l'inviluppo), <b>Legato</b> (una voce: "
    "le note suonate senza staccare cambiano solo l'altezza, senza ripartire). In Mono e Legato, se le note si sovrappongono, "
    "gli oscillatori continuano senza interruzioni (nessun click); rilasciando l'ultima nota si torna a quella ancora tenuta.",
    "<b>Glide</b> (portamento, 0–3 s), <b>Bend</b> (escursione del pitch bend, 0–24 semitoni), <b>Vel</b> (quanto la "
    "velocity influisce sul volume), <b>Width</b> (apertura stereo dell'unison), <b>MW Vib</b> (vibrato con la mod wheel).",
])
h2("FX, OUTPUT FILTERS, MASTER")
bullets([
    "<b>Drive</b> globale, <b>Chorus</b> (Off, I, II, I+II in stile Juno, Ensemble) con <b>Mix</b>, <b>Reverb</b> (Rev Size, Damp, Rev Mix).",
    "<b>Filtri d'uscita</b> LP, BP e HP, ognuno con pulsante on/off, Cut e Res.",
    "<b>Master</b> (−36 … +6 dB). Un limiter di sicurezza protegge l'uscita.",
])
h2("EQ a 5 bande")
bullets([
    "Nel riquadro piccolo trascina un nodo: orizzontale = frequenza, verticale = guadagno; <b>rotella</b> = Q; <b>doppio clic</b> = banda on/off.",
    "Il pulsante <b>+</b> apre la finestra grande con tipo (Low Cut, Low Shelf, Bell, High Shelf, High Cut, Notch), frequenza, "
    "guadagno e Q di ogni banda; <b>CLOSE</b> la chiude.",
])
img("gui_eq.png", 140, "La finestra grande dell'EQ.")
h2("8 LFO")
bullets([
    "Ogni LFO può modulare <b>qualsiasi parametro</b> del plugin (moduli, effetti, EQ, filtri d'uscita): scegli il bersaglio "
    "nel menu (<i>-- off</i> = nessuno).",
    "Forme: Triangle, Sine, Saw Up, Saw Down, Square, S&amp;H, Random. Rate libero (0,01–40 Hz) o <b>SYNC</b> al tempo della "
    "DAW (8 battute … 1/32, terzine e puntati), <b>Depth</b> bipolare, <b>Phase</b>.",
    "I pulsanti 1–8 scelgono l'LFO da modificare; il <b>+</b> apre la vista con tutti gli 8 LFO.",
    "Senza DAW (app Standalone) il tempo è quello del parametro <i>Tempo (no host)</i>, 124 bpm di default.",
])
h2("MIDI")
bullets([
    "<b>Pitch bend</b> (escursione impostata da Bend), <b>mod wheel</b> CC 1 (vibrato e cutoff, vedi MW Vib / MW Cutoff), "
    "<b>sustain</b> CC 64, <b>All Notes Off</b> / <b>All Sound Off</b> (CC 123 / 120).",
    "Tutti i parametri sono automatizzabili dalla DAW con il loro nome in inglese (es. <i>A Cutoff</i>, <i>Reverb Mix</i>).",
])

# ------------------------------------------------------------------------------------------- 5 presets
story.append(PageBreak())
h1("5. I preset")
p("Nel <b>BROWSER</b> ogni riga mostra anno, nome del preset (genere – brano), strumento e moduli. Il campo di ricerca "
  "filtra per preset, brano, artista, synth o genere; i pulsanti dei decenni (1960s … 2020s) saltano al primo preset del "
  "decennio. Clic su una riga per caricarla; sotto la lista compaiono i dettagli e le note sulla fonte. "
  "Quando lo strumento di un brano non è documentato con certezza, le note lo dicono.")
img("gui_browser.png", 160, "Il browser dei preset.")
p("<b>Nomi di fabbrica</b>: quando il suono è un preset documentato dello strumento originale (Organ 2, E.PIANO 1, "
  "Lately Bass, What The, ORCH5, Sandstorm…) il nome reale è indicato nella colonna <i>Preset di fabbrica</i>.")
pres = sorted(lib["presets"], key=lambda x: (x["year"], x["name"]))
for dec in lib["decades"]:
    rows = [["Anno", "Preset", "Genere – brano", "Artista", "Strumento", "Preset di fabbrica"]]
    for x in pres:
        if x.get("decade") != dec: continue
        rows.append([x["year"], x["name"], "%s – %s" % (x["genre"], x["song"]), x["artist"], x.get("synth", ""), x.get("original", "")])
    if len(rows) > 1:
        h2(dec)
        table(rows, [10, 33, 45, 30, 32, 20])

# ------------------------------------------------------------------------------------------- 6 parameter reference
story.append(PageBreak())
h1("6. Riferimento parametri")
p("Elenco dei %d parametri automatizzabili. I parametri dei moduli sono uguali per A–E (nella DAW: <i>A Cutoff</i>, "
  "<i>B Cutoff</i>…, id <i>A_cut</i>, <i>B_cut</i>…); quelli degli operatori FM, delle bande EQ e degli LFO sono mostrati una "
  "volta con <i>n</i> al posto del numero." % len(params))
def values(prm):
    k = prm["kind"]
    unit = (" " + prm["unit"]) if prm["unit"] else ""
    def num(v):
        return ("%g" % round(v, 3)).replace(".", ",")
    if k == 2: return "Off / On", "On" if prm["def"] > 0.5 else "Off"
    if k == 3:
        ch = prm["choices"].split("|")
        d = ch[int(prm["def"])] if int(prm["def"]) < len(ch) else ""
        return (" / ".join(ch[:8]) + (" … (%d)" % len(ch) if len(ch) > 8 else "")), d
    if prm["id"].endswith("_target"): return "-- off / qualsiasi parametro", "-- off"
    return "%s … %s%s" % (num(prm["min"]), num(prm["max"]), unit), num(prm["def"]) + unit
def section(title, items, collapse=None):
    h2(title)
    rows = [["Nome", "Id", "Valori", "Default"]]
    seen = set()
    for prm in items:
        pid, name = prm["id"], prm["name"]
        if collapse:
            key = re.sub(collapse, lambda m: m.group(0).rstrip("0123456789") + "n", pid)
            if key in seen: continue
            seen.add(key)
            pid = key
            name = re.sub(r"(Op|EQ|LFO)\d", r"\1n", name)
        rng, d = values(prm)
        rows.append([name, pid, rng, d])
    table(rows, [38, 26, 82, 24])
mod_a = [x for x in params if x["module"] == 0]
for x in mod_a: x["name"] = re.sub(r"^A ", "", x["name"])
for x in mod_a: x["id"] = re.sub(r"^A_", "&lt;M&gt;_", x["id"])
section("Moduli A–E", mod_a, collapse=r"op\d")
section("Globali", [x for x in params if x["module"] < 0 and (x["id"].startswith("g_") or x["id"].startswith("of_"))])
section("EQ (bande 1–5)", [x for x in params if re.match(r"eq\d_", x["id"])], collapse=r"^eq\d")
section("LFO (1–8)", [x for x in params if re.match(r"l\d_", x["id"])], collapse=r"^l\d")

# ------------------------------------------------------------------------------------------- 7 credits
h1("7. Note")
bullets([
    "Font dell'interfaccia: <b>Inter</b>; font di questo manuale: <b>Urbanist</b> (entrambi SIL Open Font License). Il logo è disegnato in vettoriale.",
    "I filtri e i pedali del TB-303 si basano su: Open303 (TeeBeeFilter), analisi TPT del diode ladder di Will Pirkle e JC-303, "
    "analisi ElectroSmash del ProCo RAT.",
    "Marchi e nomi degli strumenti appartengono ai rispettivi proprietari e sono usati solo per indicare lo strumento di "
    "riferimento. Le attribuzioni dei suoni ai brani vengono da fonti di settore citate nelle note dei preset.",
])

# ------------------------------------------------------------------------------------------- build
def footer(c, doc):
    c.saveState()
    c.setFont("Urbanist", 8)
    c.setFillColor(colors.HexColor("#888888"))
    c.drawString(18 * mm, 10 * mm, "5ynth %s — Manuale d'uso" % version)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                        title="5ynth %s — Manuale d'uso" % version, author="TMFU audiovibe")
doc.build(story, onFirstPage=lambda c, d: None, onLaterPages=footer)
print("wrote", OUT)
