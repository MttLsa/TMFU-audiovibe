#!/usr/bin/env python3
"""
Builds docs/5ynth_Manuale_v<version>.pdf (Italian user manual) with reportlab.
Inputs: resources/presets.json, docs/logo_5ynth.png (SynthZillaTest <dir> logo), docs/gui_*.png (SynthZillaTest <dir> gui)
Usage:  python3 tools/make_manual.py [version]
"""
import json, os, sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version = sys.argv[1] if len(sys.argv) > 1 else "2.1.0"
lib = json.load(open(os.path.join(ROOT, "resources", "presets.json")))
OUT = os.path.join(ROOT, "docs", "5ynth_Manuale_v%s.pdf" % version)

# house font: Urbanist (Light + SemiBold) (resources/fonts, SIL OFL 1.1) for every text in the manual
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.fonts import addMapping
FONTS = os.path.join(ROOT, "resources", "fonts")
pdfmetrics.registerFont(TTFont("Urbanist", os.path.join(FONTS, "Urbanist-Light.ttf")))
pdfmetrics.registerFont(TTFont("Urbanist-SemiBold", os.path.join(FONTS, "Urbanist-SemiBold.ttf")))
for bold, italic, face in ((0, 0, "Urbanist"), (1, 0, "Urbanist-SemiBold"), (0, 1, "Urbanist"), (1, 1, "Urbanist-SemiBold")):
    addMapping("Urbanist", bold, italic, face)

ss = getSampleStyleSheet()
for st in ss.byName.values():
    if hasattr(st, "fontName"):
        st.fontName = "Urbanist-SemiBold" if "Bold" in st.fontName else "Urbanist"
ACC = colors.HexColor("#3a6ea5")
H1 = ParagraphStyle("h1", parent=ss["Heading1"], fontSize=18, textColor=ACC, spaceAfter=8)
H2 = ParagraphStyle("h2", parent=ss["Heading2"], fontSize=13, textColor=colors.HexColor("#333333"), spaceBefore=8, spaceAfter=4)
P = ParagraphStyle("p", parent=ss["BodyText"], fontSize=9.5, leading=13)
SMALL = ParagraphStyle("s", parent=P, fontSize=7, leading=8.6)
BUL = ParagraphStyle("b", parent=P, leftIndent=10, bulletIndent=2)

story = []
def h1(t): story.append(Paragraph(t, H1))
def h2(t): story.append(Paragraph(t, H2))
def p(t): story.append(Paragraph(t, P))
def bullets(items):
    for it in items: story.append(Paragraph(it, BUL, bulletText="•"))
def img(name, w=170):
    path = os.path.join(ROOT, "docs", name)
    if os.path.exists(path):
        from PIL import Image as PI
        iw, ih = PI.open(path).size
        story.append(Image(path, width=w * mm, height=w * mm * ih / iw))
        story.append(Spacer(1, 4))
def table(rows, widths, head=True, style=SMALL):
    data = [[Paragraph(str(c), style) for c in r] for r in rows]
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if head else 0)
    ts = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("GRID", (0, 0), (-1, -1), 0.25, colors.HexColor("#c8c8c8")),
          ("TOPPADDING", (0, 0), (-1, -1), 1.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 1.5)]
    if head: ts.append(("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e6e6e6")))
    t.setStyle(TableStyle(ts))
    story.append(t)
    story.append(Spacer(1, 6))

# ------------------------------------------------------------------------------------------- cover
story.append(Spacer(1, 22 * mm))
img("logo_5ynth.png", 170)
story.append(Spacer(1, 4))
story.append(Paragraph("5-in-1 synthesizer: fino a 5 synth analogici / digitali con i suoni famosi 1963–2023",
                       ParagraphStyle("st", parent=P, fontSize=14, leading=18)))
story.append(Spacer(1, 6))
story.append(Paragraph("TMFU audiovibe  ·  Manuale d'uso  ·  versione %s  ·  VST3 / AU / Standalone" % version, P))
story.append(Spacer(1, 12))
img("gui_Organ_2.png", 170)
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- intro
h1("1. Cos'è 5ynth")
p("<b>5ynth</b> (prima si chiamava SynthZilla) è uno strumento virtuale che contiene fino a <b>cinque synth</b> in un unico "
  "plugin: i moduli A–E. Ogni modulo può diventare uno dei grandi synth della storia (Minimoog, TB-303, DX7, Juno, M1, "
  "D-50, PPG, Virus, JP-8000…), di cui prende il suono caratteristico e l'aspetto grafico. I suoni sono ricreati "
  "<b>per sintesi</b>, senza campioni. La libreria contiene %d preset ordinati per anno, ognuno legato al brano più famoso "
  "che ha usato quel suono." % len(lib["presets"]))
h2("Installazione (macOS)")
p("Apri <i>5ynth-%s-macOS.pkg</i>: installa l'AU in /Library/Audio/Plug-Ins/Components, il VST3 in "
  "/Library/Audio/Plug-Ins/VST3 e l'app Standalone in /Applicazioni. Il programma di installazione è universale "
  "(Mac Intel e Apple Silicon). Nella DAW il plugin compare come <b>5ynth</b> sotto <b>TMFU audiovibe</b>, tra gli strumenti. "
  "I progetti salvati con SynthZilla ritrovano il plugin: i codici di identificazione non sono cambiati." % version)
p("Il pacchetto non è firmato da uno sviluppatore Apple: se macOS lo blocca, fai clic destro sul file → <i>Apri</i>, "
  "oppure consenti l'apertura da Impostazioni di Sistema → Privacy e sicurezza.")
h2("La finestra")
bullets([
    "<b>Testata</b>: logo 5YNTH (un clic apre il logo grande, un altro clic lo chiude), nome del preset con genere e brano, "
    "anno, artista, strumento e moduli usati; frecce <b>&lt; &gt;</b> per scorrere i preset, <b>BROWSER</b> per la libreria, "
    "meter d'uscita <b>OUT</b>.",
    "<b>Schede dei moduli</b> A–E: accensione, menu Synth Model, <b>Lvl</b> e <b>Pan</b>, barra di livello. Un clic sulla scheda "
    "mostra il pannello del modulo sotto. <b>+ CUSTOM</b> aggiunge un modulo (fino a E), la <b>x</b> toglie l'ultimo.",
    "<b>Pannello del modulo</b>: tutti i parametri del modulo scelto, con l'aspetto dello strumento originale.",
    "<b>Striscia globale</b>: VOICE, FX, OUTPUT FILTERS, MASTER, EQ a 5 bande e LFO.",
    "<b>Tastiera</b> in basso per suonare col mouse.",
])
p("La finestra è <b>ridimensionabile</b> dal 50 % al 200 % trascinando l'angolo in basso a destra; la dimensione viene ricordata.")

# ------------------------------------------------------------------------------------------- logo
h2("Il logo")
p("Il logo 5YNTH è disegnato in vettoriale, quindi resta nitido a ogni dimensione della finestra e su schermi Retina. "
  "Il <b>5</b> è il simbolo dello strumento: la barra superiore è una <b>tastiera stilizzata</b> e le <b>cinque barre luminose</b> "
  "sono i cinque synth. Le lettere YNTH hanno lo stesso spessore, la stessa altezza e la stessa larghezza del 5; una cornice "
  "segue il contorno della scritta.")
img("gui_logo.png", 130)
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- modules
h1("2. I moduli synth A–E")
p("All'avvio ci sono 2 moduli (A e B). Ogni modulo ha un menu <b>Synth Model</b>: scegliendo uno strumento il modulo carica "
  "il suo suono caratteristico e prende il suo aspetto (legno Moog/Prophet, fader Juno/CS-80, TB-303 argento, LCD e membrane "
  "DX7/M1, PPG blu, Virus/Nord rossi, CRT Fairlight…), senza loghi o grafiche originali. Strumento analogico → motore "
  "<b>Analog</b>; digitale → motore digitale; synth ibridi o strumenti sovrapposti sul disco → più moduli in layer.")
models = lib["models"]
rows = [["Anno", "Strumento", "Tipo"]] + [[m.get("year", ""), m["name"], "analogico" if m.get("kind") == "A" else "digitale"]
                                          for m in sorted(models, key=lambda m: (m.get("year", 0), m["name"]))]
h2("Strumenti disponibili (%d)" % len(models))
half = (len(rows) - 1 + 1) // 2
left, right = rows[1:1 + half], rows[1 + half:]
merged = [["Anno", "Strumento", "Tipo", "Anno", "Strumento", "Tipo"]]
for i in range(half):
    r = right[i] if i < len(right) else ["", "", ""]
    merged.append(left[i] + r)
table(merged, [11, 50, 20, 11, 50, 20])

h2("Sezioni del modulo")
table([
    ["Sezione", "Parametri"],
    ["Modulo", "On, Synth Model, Level, Pan, Engine (Analog, FM 6-op, Wavetable, Supersaw, Phase Dist, Additive)"],
    ["Pitch", "Octave ±3, Semi ±12, Fine ±100 ct, Pitch Env ±48 st con Pitch Decay"],
    ["Oscillatori (Analog)", "Osc1 / Osc2: Wave (Saw, Pulse, Triangle, Sine), PW, Level; Osc2 Octave / Semi / Fine; Sync, Cross Mod, "
                             "Ring Mod, Sub Osc, Noise, PWM"],
    ["Unison", "Unison 1–7 voci, Detune, Drift (instabilità analogica)"],
    ["FM 6-op", "32 algoritmi, Feedback, Vel>Index; 6 operatori con ratio, fine, livello e inviluppo (vista DX7 o MODERN)"],
    ["Wavetable", "12 tabelle (Analog Morph, Pulse Width, PPG Digital, Vocal, Bell, Organ, Hollow, Sync Sweep, Growl, Bit Steps, "
                  "Strings, Reso Sweep), WT Position, WT Env, WT LFO"],
    ["Supersaw", "Supersaw Detune, Supersaw Mix (stile JP-8000)"],
    ["Phase Dist", "PD Wave (Saw, Square, Pulse, Double Sine, Reso Saw / Tri / Trap), PD Amount, PD Env (stile Casio CZ)"],
    ["Additive", "Additive Model (organi, piani, archi, coro, campane, ottoni, pad…), Brightness, Partial Decay, Inharmonicity, "
                 "Even Partials"],
    ["Filtro", "Filter Type (LP24 Ladder, LP 303 Diode, LP12, BP12, HP12, LP24 Clean, Notch, Off), Cutoff, Resonance, "
               "Filter Drive, Key Track, Env Amount, Vel>Filter"],
    ["Drive a pedale", "Distortion (Off, Tube, RAT, DS-1), Dist Gain, Dist Tone, Dist Mix"],
    ["Inviluppi", "Filtro: F.Attack, F.Decay, F.Sustain, F.Release; ampiezza: Attack, Decay, Sustain, Release"],
    ["LFO del modulo", "LFO Shape, LFO Rate, LFO Delay, LFO>Pitch, LFO>Cutoff, LFO>Timbre"],
    ["Carattere", "Accent (303), Bit Crush, Rate Reduce"],
    ["Delay", "Delay Time (1/16 … 1/2, terzine e puntati), Delay Feedback, Delay Mix, Delay Ping-Pong"],
], [32, 138])

h2("FM a 6 operatori: DX7 o MODERN")
p("I moduli FM hanno 6 operatori e i 32 algoritmi in stile DX7, con lo schema dell'algoritmo disegnato nel pannello. "
  "In modo <b>DX7</b> i parametri sono come sullo strumento originale (COARSE / FINE 0–99, OUT LVL 0–99, rate R1/R2/R4, "
  "livello L3, FEEDBACK 0–7) su celle a membrana con LCD. In modo <b>MODERN</b> gli stessi operatori sono presentati in stile "
  "Operator di Ableton Live: ratio, fine, livelli in dB, Attack / Decay / Release in ms, Sustain in dB.")
img("gui_E.PIANO_1.png", 150)
h2("TB-303")
p("Il filtro <b>LP 303 Diode</b> è un diode ladder a 4 poli con il primo polo un'ottava sopra gli altri, passa-alto nel "
  "percorso di risonanza (lo \"squelch\" del 303), saturazione d'ingresso, sovracampionato 2×. L'<b>Accent</b> usa il circuito "
  "\"sweep\": accent consecutivi si accumulano. I drive <b>RAT</b> e <b>DS-1</b> modellano i pedali usati sull'acid.")
img("gui_Acid_Tracks.png", 150)
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- global
h1("3. Voce, effetti e uscita")
table([
    ["Gruppo", "Parametri"],
    ["VOICE", "Mode (Poly, Mono, Legato), Glide, Bend (range 0–24 st), Vel (sensibilità alla velocity), Width (stereo), "
              "MW Vib (vibrato con la rotella di modulazione)"],
    ["FX", "Drive, Chorus (Off, I, II, I+II stile Juno, Ensemble) e Mix, Reverb: Rev Size, Damp, Rev Mix"],
    ["OUTPUT FILTERS", "LP, BP e HP d'uscita, ognuno con accensione, Cut e Res"],
    ["MASTER", "Volume d'uscita (−36 … +6 dB); un limiter di sicurezza protegge l'uscita"],
    ["EQ", "EQ grafico a 5 bande: trascina i nodi (frequenza / guadagno), rotella = Q, doppio clic = on/off; il pulsante + "
           "apre la finestra grande con tipo, frequenza, guadagno e Q di ogni banda"],
], [32, 138])
img("gui_eq.png", 150)
h2("8 LFO globali")
p("Ogni LFO può modulare <b>qualsiasi parametro</b> del plugin (tutti i moduli, effetti, EQ, filtri d'uscita). Forme Triangle, "
  "Sine, Saw Up/Down, Square, S&amp;H, Random; rate libero o <b>SYNC</b> al tempo (8 battute … 1/32, terzine e puntati), "
  "depth bipolare, fase. Il <b>+</b> apre la vista con tutti gli 8 LFO. LFO, EQ e filtri d'uscita non cambiano quando carichi "
  "un preset.")
h2("Controlli")
bullets([
    "Trascina in verticale manopole e fader; <b>Shift</b> = movimento 10× più fine; <b>doppio clic</b> = valore di default.",
    "Dopo un clic su un controllo le frecce lo muovono di 1 % (fader 0,1 %).",
    "Le manopole si muovono a passi di 0,01, i fader di 0,001. Sugli strumenti che hanno fader i fader sono più lunghi.",
])
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- presets
h1("4. Preset")
p("I preset sono ordinati per anno. Nel <b>BROWSER</b> ogni riga mostra anno, nome (genere – brano), strumento e moduli; "
  "c'è la ricerca testuale e i salti per decennio. Quando il suono è un preset di fabbrica documentato viene usato il suo nome "
  "reale (Organ 2, E.PIANO 1, Lately Bass, What The, ORCH5, Sandstorm…). Tutti i preset sono normalizzati con picco a "
  "−1 dBFS. Quando lo strumento di un brano non è documentato con certezza, la scheda del preset lo dice.")
img("gui_browser.png", 150)
rows = [["Anno", "Preset", "Genere", "Brano", "Artista", "Strumento", "Moduli"]]
for pr in lib["presets"]:
    mods = "".join(sorted({"ABCDE"[m.get("slot", 0)] for m in pr.get("modules", []) if isinstance(m, dict)})) or "A"
    rows.append([pr["year"], pr["name"], pr["genre"], pr["song"], pr["artist"], pr["synth"], mods])
h2("Elenco completo (%d)" % len(lib["presets"]))
table(rows, [10, 33, 20, 35, 30, 33, 9])

# ------------------------------------------------------------------------------------------- notes
h1("5. Note")
bullets([
    "Formati: VST3, AU (strumento, <i>aumu Szla Tmfu</i>) e app Standalone; macOS 10.13 o successivo, Intel e Apple Silicon.",
    "Marchi e nomi degli strumenti appartengono ai rispettivi proprietari e sono usati solo per indicare lo strumento di "
    "riferimento. Nessun campione, logo o grafica originale è incluso.",
    "Font: Inter (interfaccia) e Urbanist (questo manuale), entrambi SIL Open Font License 1.1.",
])

def footer(c, doc):
    c.saveState()
    c.setFont("Urbanist", 7.5)
    c.setFillColor(colors.HexColor("#888888"))
    c.drawString(18 * mm, 10 * mm, "5ynth %s  ·  TMFU audiovibe" % version)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=16 * mm,
                        title="5ynth - Manuale d'uso", author="TMFU audiovibe")
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print(OUT)
