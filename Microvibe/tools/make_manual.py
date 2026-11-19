#!/usr/bin/env python3
"""
Builds docs/Microvibe_Manuale_v<version>.pdf (Italian user manual) with reportlab.
Inputs: params.json (MicrovibeTest <dir> params), resources/presets.json, docs/gui_*.png
Usage:  python3 tools/make_manual.py <params.json> [version]
"""
import json, os, re, sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak, KeepTogether)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
params = json.load(open(sys.argv[1]))
version = sys.argv[2] if len(sys.argv) > 2 else "1.0.0"
presets = json.load(open(os.path.join(ROOT, "resources", "presets.json")))
OUT = os.path.join(ROOT, "docs", "Microvibe_Manuale_v%s.pdf" % version)

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
ACC = colors.HexColor("#c0392b")
H1 = ParagraphStyle("h1", parent=ss["Heading1"], fontSize=18, textColor=ACC, spaceAfter=8)
H2 = ParagraphStyle("h2", parent=ss["Heading2"], fontSize=13, textColor=colors.HexColor("#1f4e79"), spaceBefore=8, spaceAfter=4)
P = ParagraphStyle("p", parent=ss["BodyText"], fontSize=9.5, leading=13)
SMALL = ParagraphStyle("s", parent=P, fontSize=7.5, leading=9.5)
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

# ------------------------------------------------------------------------------------------- cover
story.append(Spacer(1, 30 * mm))
story.append(Paragraph("MICROVIBE", ParagraphStyle("t", parent=H1, fontSize=40, leading=46)))
story.append(Paragraph("Glitch, texture, percussioni organiche ed effetti per minimal e microhouse", ParagraphStyle("st", parent=P, fontSize=14, leading=18)))
story.append(Spacer(1, 6))
story.append(Paragraph("TMFU audiovibe  ·  Manuale d'uso  ·  versione %s  ·  VST3 / AU / Standalone" % version, P))
story.append(Spacer(1, 12))
img("gui_main.png", 170)
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- intro
h1("1. Cos'è Microvibe")
p("Microvibe è un <b>music effect</b>: si inserisce su una traccia audio, riceve MIDI e contiene tre generatori "
  "(texture, percussioni modali, Shepard tone), quindi suona anche su una traccia vuota. Il segnale passa da "
  "<b>sorgenti</b> (ingresso + generatori) al <b>glitch engine</b> e poi a una <b>catena di 8 effetti</b> riordinabili. "
  "Tutto è sincronizzato al tempo della DAW tramite quattro <b>corsie ritmiche</b> poliritmiche, e ogni parametro "
  "continuo può essere modulato dalla <b>matrice</b> di 32 slot.")
h2("Installazione (macOS)")
p("Apri <i>Microvibe-%s-macOS.pkg</i>: installa l'AU in /Library/Audio/Plug-Ins/Components, il VST3 in "
  "/Library/Audio/Plug-Ins/VST3 e l'app Standalone in /Applicazioni. Nella DAW il plugin compare sotto "
  "<b>TMFU audiovibe</b>. In Logic Pro si carica come <i>Audio FX → Effetti MIDI-controllati</i> (AU music effect); "
  "in Ableton Live, Bitwig, Reaper come effetto audio VST3." % version)
h2("Flusso del segnale")
story.append(Paragraph("IN → [TEXTURE + PERC + SHEPARD] → GLITCH → FX 1 … FX 8 → DRY/WET → WIDTH → OUTPUT → NORM / limiter", ParagraphStyle("code", parent=P, fontSize=8.5)))
h2("Testata")
bullets([
    "<b>Preset</b>: menu per categorie, frecce &lt; &gt; per scorrere. Il volume di uscita non cambia con i preset.",
    "<b>MACRO 1–4</b>: nei preset sono collegate a densità/glitch, spazio, sporcizia, tono. Il chip M1…M4 sotto ogni macro si trascina su qualsiasi manopola.",
    "<b>DENSITY</b>: aumenta o diminuisce tutti gli eventi probabilistici (glitch, percussioni, ratchet). Al centro (0,5) le probabilità sono quelle impostate; a 0 non suona niente; a 1 la probabilità diventa 1-(1-p)².",
    "<b>DRY/WET</b>, <b>WIDTH</b>, <b>OUTPUT</b>. <b>NORM</b>: porta l'uscita verso un livello medio costante e attiva un limiter a −1 dBFS (senza NORM il limiter di sicurezza è a −0,3 dBFS). Il LED rosso sopra il meter indica che il limiter lavora.",
])

# ------------------------------------------------------------------------------------------- sources
h1("2. Sorgenti")
h2("IN / CLOCK")
bullets([
    "<b>Input Level</b>, <b>Input Gate</b> (soglia in dB, 'off' al minimo).",
    "<b>Transient Threshold / Source</b>: rilevatore di transienti sull'ingresso principale o sul sidechain. I transienti possono far avanzare le corsie (Clock Source = Transient), far partire inviluppi e resettare LFO.",
    "<b>Tempo (no host)</b>: tempo usato nella Standalone o quando la DAW non lo fornisce.",
    "<b>Pattern A–H</b>, <b>Pattern Chain</b> e <b>Chain End</b>: ogni pattern ha le sue griglie delle corsie; con la catena il pattern avanza di uno a ogni battuta.",
])
h2("TEXTURE")
p("Due strati per gli sfondi. Il <b>letto</b>: rumore (White, Pink, Brown, Crackle) filtrato da un banco di risonatori "
  "accordati sull'accordo scelto (<i>Texture Chord</i>, nella scala corrente) + <b>Drone</b> sinusoidale con <b>Drift</b>. "
  "La <b>nuvola granulare</b> legge un buffer di 4 s dell'ingresso, del letto o di entrambi: Density (grani al secondo), "
  "Size, Spray (quanto indietro nel buffer), Pitch e Pitch Random (quantizzati in scala), Spread stereo, Reverse, "
  "finestra Hann/Tukey/Rect e <b>Freeze</b> (smette di registrare: granuli sempre sullo stesso istante). <b>Tilt</b> inclina lo spettro. "
  "Con <b>Texture Chord = Scale</b> le note del drone e del letto risonante si scelgono nella pagina TUNING, con una progressione "
  "di fino a 4 accordi (vedi capitolo 6).")
h2("PERC — percussioni organiche")
p("Sei voci a <b>sintesi modale</b>: un eccitatore colpisce un banco di risonatori con i rapporti di frequenza di un "
  "materiale reale; oppure suoni <b>808</b> sintetizzati. Nessun campione.")
bullets([
    "<b>Model</b>: Wood (barra libera), Glass (calice), Metal (piastra/campana), Skin (membrana), Tube (canna), Stone, Plastic.",
    "<b>Modelli 808</b>: Kick 808, Tom 808, Snare 808, Clap 808, Closed Hat 808, Open Hat 808. Kick e tom sono sinusoidi con sweep di pitch e click (a Tune 0 il kick è in G1 e il tom in G2, e seguono la scala); lo snare unisce due sinusoidi 180/330 Hz e rumore; il clap è rumore filtrato con quattro colpi ravvicinati; gli hat sono sei onde quadre metalliche. Il <b>closed hat interrompe l'open hat</b> (choke). Per gli 808 <i>Tune</i> sposta l'accordatura originale, <i>Material</i> è il tono (snappy per lo snare, brillantezza per gli hat), <i>Inharmonic</i> la profondità dello sweep; l'eccitatore non si usa.",
    "<b>Filtro di voce</b>: Off, HP, BP, LP con <b>Cutoff</b> e <b>Resonance</b>, insieme a <b>Voice Level</b> (gain), <b>Tune</b> e <b>Decay</b>: ogni suono si regola da solo.",
    "<b>Exciter</b>: Click, Noise (sabbia, shaker), Mallet (battente morbido), <b>Input</b>: 30 ms dell'ingresso usati come eccitatore (microsampling).",
    "<b>Tune</b> (semitoni dal Root, quantizzati dalla scala), <b>Decay</b>, <b>Material</b> (smorza le parziali alte), <b>Inharmonic</b>, <b>Position</b> (punto di percussione).",
    "<b>Lane</b>: la corsia che la suona. <b>Every / Offset</b>: suona solo un colpo ogni N della corsia (sottodivisioni e poliritmie dentro la stessa corsia). <b>Probability</b>, <b>Humanize</b> (velocity e pan).",
])
img("gui_perc.png", 150)
h2("SHEPARD")
p("Generatore <b>Shepard–Risset</b>: 3–10 parziali distanziate di un intervallo (ottava, quinta, quarta, tritono, terza "
  "maggiore o <i>Custom</i> in cent) con inviluppo spettrale gaussiano attorno a <b>Centre</b>. Il glissando sale, scende "
  "o va avanti e indietro (<b>Pendulum</b>) senza fine; con <b>Sync</b> un intervallo dura <i>Cycle</i> battute.")

# ------------------------------------------------------------------------------------------- lanes & glitch
h1("3. Corsie e glitch")
h2("Corsie (L1–L4)")
bullets([
    "<b>Steps</b> 1–64: ogni corsia ha la sua lunghezza → polimetro (16 contro 13 contro 7…).",
    "<b>Division</b> × <b>Tuplet</b> (3:2, 5:4, 7:8, 11:8…): N step nel tempo di M → poliritmia. Con <b>Sync</b> spento la corsia corre libera in Hz (phasing alla Steve Reich).",
    "<b>Mode</b>: Euclid (Pulses, Rotate) oppure Grid (celle disegnate; Alt + trascina = valore dello step, usato come velocity e come sorgente di modulazione STEP).",
    "<b>Swing</b>, <b>Probability</b>, <b>Ratchet</b> + <b>Ratchet Prob</b>, <b>Humanize</b>.",
    "<b>Condition</b>: Always, 1:2, 2:2, 1:3, 1:4 (in quale giro della corsia suonare), Fill / Not Fill (Density sopra / sotto 0,75), First.",
    "<b>Clock Source</b>: Clock (tempo), Transient (ogni transiente dell'ingresso = uno step), MIDI (ogni nota = uno step).",
])
h2("Stop della DAW")
p("Quando premi stop si fermano, con una dissolvenza di circa 30 ms, texture (letto, drone, grani), Shepard, percussioni, glitch "
  "e tutte le corsie, insieme al rumore del vinile e al fruscio del nastro. Solo le <b>code di delay e riverbero</b> si spengono "
  "da sole in modo naturale (a trasporto fermo il feedback del delay è limitato e i freeze vengono rilasciati, così finiscono). "
  "Le note MIDI suonate a mano continuano a funzionare. Nella versione Standalone il plugin suona sempre.")
h2("Glitch engine")
p("Un buffer di 8 secondi registra continuamente le sorgenti. A ogni colpo della corsia scelta (<b>Glitch Lane</b>) "
  "e con probabilità <b>Glitch Probability</b> parte un evento lungo <b>Event Length</b> step, di un tipo scelto a caso "
  "secondo i <b>pesi</b>:")
bullets([
    "<b>Repeat</b>: stutter/roll di una fetta lunga <i>Repeat Size</i>; <i>Decay</i> abbassa ogni ripetizione, <i>Pitch Ramp</i> la intona, <i>Shrink</i> accorcia le ripetizioni (roll a scalare).",
    "<b>Reverse</b>, <b>Slice</b> (fette precedenti rimescolate, alcune al contrario), <b>Tape Stop</b> (<i>Stop Time</i>), <b>Gate</b> (<i>Gate Shape</i>, <i>Gate Rate</i>).",
    "<b>Freeze</b> (loop granulare di <i>Freeze Size</i>), <b>Crush</b> (<i>Bits</i>, <i>Downsample</i>), <b>Dropout</b> (salti tipo CD), <b>Pitch</b> (loop intonato di <i>Glitch Pitch</i>, in scala).",
    "<b>Crossfade</b> su ogni taglio; <b>Glitch Mix</b>.",
    "MIDI: le note C1–G#1 tengono premuto un tipo di glitch finché la nota resta giù.",
])

# ------------------------------------------------------------------------------------------- fx
h1("4. Catena FX")
p("Otto slot: clic su uno slot vuoto (o tasto destro) per scegliere il modulo, clic sul LED per accenderlo/spegnerlo, "
  "trascina uno slot su un altro per scambiarli. Ogni modulo compare una sola volta nella catena.")
fx = [
    ("Tape", "isteresi magnetica Jiles–Atherton risolta in tempo reale (RK2, 2× oversampling) o modello Eco; Drive, Saturation, <b>Bias</b> (sottopolarizzato = zona morta), velocità 7,5 / 15 / 30 ips (head bump e perdita di acuti), Age, Hiss, Compression, Dropouts, Wow/Flutter."),
    ("Vinyl", "Dust, Crackle, Scratch (legati al giro: 33/45/78 rpm), Rumble, Hum 50/60 Hz, <b>Year</b> (banda passante d'epoca, mono prima del 1958), Wear (perdita di acuti e distorsione del solco), Warp, <b>Brake</b> (rallenta fino a fermarsi; tornando a 0 il disco riparte e recupera: modulalo con un LFO o un inviluppo per spin-down ritmici), Bass Mono, Noise Level."),
    ("Wobble", "come il modulo Wobble dell'RC-20: <b>Wow</b> 0,1–4 Hz e <b>Flutter</b> 6–20 Hz, cursore Wow ↔ Flutter, un solo <b>Amount</b>, <b>Flux</b> per renderlo irregolare, Stereo Spread."),
    ("Delay", "Digital, Tape, Ping-Pong, <b>Multitap</b> (3, 4, 5 o 7 tap equidistanti: poliritmie); Sync o ms, Feedback fino a 1,1, LP/HP e saturazione nel loop, Wobble, <b>Feedback Pitch</b> (spirali, in scala), Freeze, Ducking."),
    ("Reverb", "rete di ritardi a 8 linee: Size, Decay, Pre-delay, Damping, Diffusion, Mod, Low Cut, <b>Shimmer</b> (+ intervallo), Freeze, Ducking; modi Hall, <b>Gated</b> e <b>Reverse</b> (swell al contrario)."),
    ("Filter", "LP, HP, BP, Notch (12/24 dB), Comb, Formant (Vowel); Drive."),
    ("Resonator", "banco di risonatori accordati sull'accordo scelto nella scala (Chord, Octave, Decay, Bright, Spread)."),
    ("Ring/Shift", "ring modulator o frequency shifter (barber-pole) a trasformata di Hilbert."),
    ("Crusher, Chorus, Phaser, Comp, Stereo", "bit/sample-rate con jitter; chorus a 1–4 voci; phaser 4–12 stadi; compressore + transient shaper; width, Haas, auto-pan, bassi mono."),
    ("Shepard FX", "processori infiniti: <b>Filter</b> (banco di passa-banda che sale/scende per sempre), <b>Phaser</b> e <b>Flanger</b> barber-pole, <b>Notch</b> (banco di notch infiniti, idea DAFx-15)."),
]
for n, t in fx:
    story.append(Paragraph("<b>%s</b> — %s" % (n, t), BUL, bulletText="•"))
img("gui_vinyl.png", 150)

# ------------------------------------------------------------------------------------------- modulation
h1("5. Modulazione")
bullets([
    "<b>LFO 1–8</b>: Sine, Triangle, Saw Up/Down, Square, S&amp;H, Smooth Random, <b>Custom</b> (16 punti disegnati col mouse); Sync (da 8 battute a 1/32, terzine, puntati, quintine, settimine) o Hz; Phase, Skew, Smooth, Depth, Unipolar, Loop / One-shot, Retrigger da corsia, MIDI, transiente o battuta.",
    "<b>ENV 1–4</b>: Attack, Hold, Decay, Sustain, Release, Curve; trigger da MIDI, corsie, transienti o dall'inizio di un glitch.",
    "<b>RANDOM 1–4</b>: S&amp;H, Smooth, Drift, Brownian; Sync o Hz, Smooth.",
    "<b>FOLLOWER 1–2</b>: inviluppo dell'ingresso, del sidechain o delle sorgenti.",
    "<b>STEP 1–4</b>: il valore dello step corrente di ogni corsia (sequencer di parametri).",
    "<b>MACRO 1–4</b> e <b>MIDI</b>: velocity, nota, mod wheel, aftertouch, pitch bend.",
])
h2("Assegnare una modulazione")
bullets([
    "Trascina un chip colorato su una manopola: si crea uno slot della matrice con amount 0,3. L'arco colorato mostra la profondità, il punto il valore modulato.",
    "Tasto destro su una manopola: aggiungi una sorgente dal menu, rimuovi le modulazioni esistenti, torna al default.",
    "Pagina <b>MATRIX</b>: 32 slot con Sorgente, <b>Via</b> (moltiplicatore: es. LFO 1 × Macro 2), Destinazione (menu per gruppi), Curva (lineare, esponenziale, logaritmica, S) e Amount bipolare.",
    "<b>Intermodulazione</b>: sono destinazioni anche i parametri dei modulatori e gli amount della matrice (un LFO può cambiare la velocità di un altro, un random la profondità di uno slot). I cicli sono stabili: ogni blocco di 32 campioni usa i valori del blocco precedente.",
])
img("gui_matrix.png", 140)

# ------------------------------------------------------------------------------------------- tuning
h1("6. Microtonalità")
p("La pagina <b>TUNING</b> sceglie la scala usata da tutti gli elementi intonati: percussioni, letto e drone della "
  "texture, grani, risonatore, pitch del glitch, pitch del delay e dello shimmer. <b>Snap</b> passa da continuo (0) "
  "a completamente quantizzato (1).")
bullets([
    "Scale interne: cromatica, maggiore, minore, dorica, frigia, pentatonica minore, esatonale, 19/24/31/53-EDO, giusta intonazione 5-limit e 7-limit, serie armonica 8–16, Bohlen-Pierce, pelog, slendro, maqam Rast e Bayati, Partch 43.",
    "<b>CARICA .SCL</b> (e facoltativamente <b>+ .KBM</b>): file della libreria Scala; vengono salvati nel progetto.",
    "<b>Follow MTS-ESP</b>: se nella sessione c'è un master MTS-ESP (ODDSound MTS-ESP Mini/Suite) Microvibe segue l'accordatura globale.",
])
h2("Note del drone e progressione di accordi")
p("Quando nella TEXTURE <i>Chord</i> è su <b>Scale</b>, nella pagina TUNING compare la sezione <b>NOTE DEL DRONE / PROGRESSIONE ACCORDI</b>.")
bullets([
    "<b>ACCORDO 1–4</b>: scegli quale accordo modificare, poi clicca sulle note della scala (nomi delle note per le scale a 12 semitoni, numero del grado e cent per le altre). Le note accordano il drone e il letto risonante. Senza note suona una triade minore automatica.",
    "<b>Progression On</b>: fa suonare gli accordi uno dopo l'altro (spenta: suona l'accordo 1). Il segno &gt; indica l'accordo in riproduzione.",
    "<b>Chords</b> (quanti accordi usare), <b>Progression Order</b> (Forward, Reverse, Ping-Pong, Random).",
    "<b>Chord Unit</b>: unità di tempo sincronizzata (da 8 battute a 1/32, terzine, puntati, quintine, settimine) oppure, con <i>Progression Sync</i> spento, in secondi. <b>Durata 1–4</b>: quante unità dura ogni accordo.",
    "<b>Chord Glide</b>: glissando tra un accordo e il successivo (0–4 s). <b>Drone Voicing</b>: Close, Open (note alterne un'ottava sopra), Wide.",
])
img("gui_tuning.png", 120)

# ------------------------------------------------------------------------------------------- presets
story.append(PageBreak())
h1("7. Preset")
p("180 preset in 12 categorie. In tutti le macro sono collegate: <b>1</b> densità e glitch, <b>2</b> spazio (delay/reverb), "
  "<b>3</b> sporcizia (wobble, polvere), <b>4</b> tono (filtro). I volumi sono calibrati per suonare tutti a un livello simile.")
for cat in presets["categories"]:
    names = [x["name"] for x in presets["presets"] if x["cat"] == cat]
    story.append(Paragraph("<b>%s</b>: %s" % (cat, ", ".join(names)), BUL, bulletText="•"))

# ------------------------------------------------------------------------------------------- parameter reference
story.append(PageBreak())
h1("8. Riferimento parametri")
p("Elenco completo (i gruppi ripetuti — LFO 1–8, corsie 1–4, percussioni 1–6, slot della matrice 1–32… — sono mostrati "
  "una volta, con 'n' al posto del numero). 'mod' = destinazione di modulazione.")
seen = set()
rows = [["Parametro", "Gruppo", "Valori", "Default", ""]]
for prm in params:
    key = re.sub(r"\d+", "#", prm["id"]) if re.match(r"^(lf|en|rn|fo|mx|ln|pc|m_|fx_slot|gl_w)", prm["id"]) else prm["id"]
    if key in seen: continue
    seen.add(key)
    name = prm["name"]
    if key != prm["id"]: name = re.sub(r"\b\d+\b", "n", name, count=1)
    if prm["choices"]:
        ch = prm["choices"].split("|")
        vals = (" / ".join(ch[:6]) + (" … (%d)" % len(ch) if len(ch) > 6 else "")) if prm["kind"] == 3 else ""
        dflt = ch[int(prm["def"])] if prm["kind"] == 3 and int(prm["def"]) < len(ch) else ""
    else:
        vals, dflt = "", ""
    if prm["kind"] == 2: vals, dflt = "on / off", "on" if prm["def"] > 0.5 else "off"
    if prm["kind"] in (0, 1):
        f = (lambda v: ("%g" % round(v, 3)))
        vals = "%s … %s %s" % (f(prm["min"]), f(prm["max"]), prm["unit"])
        dflt = f(prm["def"])
    rows.append([Paragraph(name, SMALL), Paragraph(prm["group"], SMALL), Paragraph(vals, SMALL), Paragraph(dflt, SMALL), "mod" if prm["dest"] else ""])
t = Table(rows, colWidths=[50 * mm, 24 * mm, 70 * mm, 22 * mm, 9 * mm], repeatRows=1)
t.setStyle(TableStyle([
    ("FONTNAME", (0, 0), (-1, -1), "Urbanist"), ("FONTNAME", (0, 0), (-1, 0), "Urbanist-SemiBold"),
    ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#1f4e79")), ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
    ("FONTSIZE", (0, 0), (-1, 0), 8), ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f1f4f8")]),
    ("VALIGN", (0, 0), (-1, -1), "TOP"), ("FONTSIZE", (4, 1), (4, -1), 7),
    ("TOPPADDING", (0, 0), (-1, -1), 1.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 1.5)]))
story.append(t)

story.append(Spacer(1, 10))
p("<font size=7.5>Microtuning: client MTS-ESP di ODDSound (licenza 0BSD). RC-20 Retro Color è un marchio di XLN Audio: "
  "Microvibe ne riprende solo il comportamento documentato del modulo Wobble. Microvibe non contiene codice, grafica o "
  "campioni di altri prodotti.</font>")

def footer(c, d):
    c.saveState(); c.setFont("Urbanist", 7.5); c.setFillColor(colors.grey)
    c.drawString(15 * mm, 10 * mm, "Microvibe %s — TMFU audiovibe" % version)
    c.drawRightString(195 * mm, 10 * mm, str(d.page)); c.restoreState()

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=15 * mm, rightMargin=15 * mm, topMargin=15 * mm, bottomMargin=16 * mm,
                        title="Microvibe — Manuale d'uso", author="TMFU audiovibe")
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print(OUT)
