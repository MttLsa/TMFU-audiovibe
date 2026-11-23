#!/usr/bin/env python3
"""
Builds docs/FamPat_Manuale_v<version>.pdf (Italian user manual) with reportlab.
Inputs: resources/presets.json, docs/logo_fampat.png (FamPatTest <dir> logo), docs/gui_*.png (FamPatTest <dir> gui)
Usage:  python3 tools/make_manual.py [version]
"""
import json, os, sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.fonts import addMapping
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak, KeepTogether

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version = sys.argv[1] if len(sys.argv) > 1 else "1.0.0"
lib = json.load(open(os.path.join(ROOT, "resources", "presets.json")))
OUT = os.path.join(ROOT, "docs", "FamPat_Manuale_v%s.pdf" % version)

FONTS = os.path.join(ROOT, "resources", "fonts")
pdfmetrics.registerFont(TTFont("Urbanist", os.path.join(FONTS, "Urbanist-Light.ttf")))
pdfmetrics.registerFont(TTFont("Urbanist-Bold", os.path.join(FONTS, "Urbanist-SemiBold.ttf")))
for b, i, name in ((0, 0, "Urbanist"), (1, 0, "Urbanist-Bold"), (0, 1, "Urbanist"), (1, 1, "Urbanist-Bold")):
    addMapping("Urbanist", b, i, name)

ss = getSampleStyleSheet()
ACC = colors.HexColor("#e07b2a")
BLU = colors.HexColor("#1f8fbf")
H1 = ParagraphStyle("h1", parent=ss["Heading1"], fontName="Urbanist-Bold", fontSize=19, leading=23, textColor=ACC, spaceAfter=8)
H2 = ParagraphStyle("h2", parent=ss["Heading2"], fontName="Urbanist-Bold", fontSize=13, leading=16, textColor=BLU, spaceBefore=8, spaceAfter=4)
P = ParagraphStyle("p", parent=ss["BodyText"], fontName="Urbanist", fontSize=10, leading=13.5)
SMALL = ParagraphStyle("s", parent=P, fontSize=7.5, leading=9.2)
SMALLB = ParagraphStyle("sb", parent=SMALL, fontName="Urbanist-Bold")
BUL = ParagraphStyle("b", parent=P, leftIndent=11, bulletIndent=2)
CAP = ParagraphStyle("cap", parent=P, fontSize=8.5, leading=11, textColor=colors.HexColor("#555555"), alignment=1)

story = []
def h1(t): story.append(Paragraph(t, H1))
def h2(t): story.append(Paragraph(t, H2))
def p(t): story.append(Paragraph(t, P))
def bullets(items):
    for it in items: story.append(Paragraph(it, BUL, bulletText="•"))
def img(name, w=170, caption=None):
    path = os.path.join(ROOT, "docs", name)
    if not os.path.exists(path): return
    from PIL import Image as PI
    iw, ih = PI.open(path).size
    parts = [Image(path, width=w * mm, height=w * mm * ih / iw)]
    if caption: parts.append(Paragraph(caption, CAP))
    parts.append(Spacer(1, 5))
    story.append(KeepTogether(parts))
def table(rows, widths, head=True):
    data = [[Paragraph(str(c), SMALLB if (head and r == 0) else SMALL) for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if head else 0)
    st = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("GRID", (0, 0), (-1, -1), 0.25, colors.HexColor("#c8c8c8")),
          ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 3),
          ("TOPPADDING", (0, 0), (-1, -1), 1.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 1.5)]
    if head: st.append(("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#eeeeee")))
    t.setStyle(TableStyle(st))
    story.append(t)
    story.append(Spacer(1, 6))

n = len(lib["presets"])
y0, y1 = min(x["year"] for x in lib["presets"]), max(x["year"] for x in lib["presets"])

# ------------------------------------------------------------------------------------------- cover
story.append(Spacer(1, 18 * mm))
img("logo_fampat.png", 170)
story.append(Paragraph("Famous patches: %d suoni di synth famosi dal %d al %d" % (n, y0, y1),
                       ParagraphStyle("st", parent=P, fontSize=15, leading=19, alignment=1)))
story.append(Spacer(1, 6))
story.append(Paragraph("TMFU audiovibe  ·  Manuale d'uso  ·  versione %s  ·  VST3 / AU / Standalone" % version,
                       ParagraphStyle("c", parent=P, alignment=1)))
story.append(Spacer(1, 10))
img("gui_Time_After_Time_Pad.png", 170)
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- 1
h1("1. Cos'è FamPat")
p("FamPat (<i>famous patches</i>) è un <b>player di preset</b>: contiene %d suoni di synth famosi, dal %d al %d, ognuno "
  "legato al brano più famoso che l'ha usato. I suoni sono ricreati <b>per sintesi</b>, senza campioni, con i circuiti degli "
  "strumenti originali (filtri, oscillatori, convertitori). Ogni patch ha solo i comandi che servono per suonarla: "
  "un <b>inviluppo del filtro</b>, un <b>inviluppo d'ampiezza</b> e la sezione <b>MASTER OUTPUT</b> con gli effetti che la "
  "patch usa e il livello d'uscita." % (n, y0, y1))
h2("Installazione (macOS)")
bullets([
    "Apri <i>FamPat-%s-macOS.pkg</i> (Mac Intel e Apple Silicon). Installa l'<b>AU</b> in /Library/Audio/Plug-Ins/Components, "
    "il <b>VST3</b> in /Library/Audio/Plug-Ins/VST3 e l'app <b>Standalone</b> in /Applicazioni." % version,
    "Il pacchetto non è firmato con un certificato Apple: se macOS lo blocca, clic destro sul file → <b>Apri</b>, oppure "
    "Impostazioni di Sistema → Privacy e sicurezza → <b>Apri comunque</b>.",
    "Nella DAW FamPat compare sotto <b>TMFU audiovibe</b> come strumento (AU <i>aumu Fmpt Tmfu</i>).",
])
h2("Primi passi")
bullets([
    "Carica FamPat su una traccia strumento e suona: parte con il preset <b>Organ 2</b> (Show Me Love).",
    "Scorri i preset con <b>&lt;</b> e <b>&gt;</b>, oppure apri il <b>BROWSER</b>.",
    "Regola gli inviluppi e gli effetti; salva il risultato con <b>SAVE</b>.",
    "Tutti i preset sono normalizzati con picco a −1 dBFS: passando da uno all'altro il volume resta simile.",
])

# ------------------------------------------------------------------------------------------- 2
h1("2. L'interfaccia")
img("gui_Acid_Tracks.png", 170, "Acid Tracks (Roland TB-303): inviluppi e MASTER OUTPUT con Delay e Level.")
h2("Testata")
bullets([
    "Il <b>logo</b>: un clic lo apre in grande, un altro clic lo chiude.",
    "Nome del preset con genere e brano, e sotto anno, artista e strumento.",
    "<b>&lt; &gt;</b>: preset precedente / successivo. <b>BROWSER</b>: la libreria (capitolo 3). <b>SAVE</b>: salva un preset "
    "utente. <b>OUT</b>: livello d'uscita (rosso vicino a 0 dBFS).",
])
h2("Scheda della patch")
p("Nome del preset, brano, artista, anno e genere, lo strumento originale (o gli strumenti, se il suono ne sovrappone più "
  "d'uno), il nome del preset di fabbrica quando il suono è un preset documentato e le note sulle fonti.")
h2("FILTER ENVELOPE e AMP ENVELOPE")
p("<b>Attack</b>, <b>Decay</b>, <b>Sustain</b> e <b>Release</b> dell'inviluppo del filtro e dell'inviluppo d'ampiezza, con il "
  "disegno della curva. Al caricamento di un preset partono dai valori della patch, quindi il suono è quello originale "
  "finché non li tocchi. Quando una patch suona con più synth sovrapposti, una sola coppia di inviluppi li regola tutti "
  "insieme: i tempi cambiano nella stessa proporzione e i livelli di sustain dello stesso valore, così le differenze tra i "
  "layer restano.")
h2("MASTER OUTPUT")
p("Compaiono solo gli effetti che la patch usa, più il livello d'uscita che c'è sempre:")
table([
    ["Sezione", "Comandi"],
    ["DRIVE", "Drive: saturazione dell'uscita"],
    ["PEDAL", "Gain del pedale di distorsione della patch (RAT, DS-1 o Tube)"],
    ["CHORUS", "Mode (I, II, I+II stile Juno, Ensemble) e Mix"],
    ["DELAY", "Time (1/16 … 1/2, terzine e puntati, a tempo con la DAW), Feedback, Mix"],
    ["REVERB", "Size, Damp, Mix"],
    ["LEVEL", "Master: livello d'uscita (−36 … +6 dB); un limiter di sicurezza protegge l'uscita"],
], [30, 140])
h2("Finestra e controlli")
bullets([
    "La finestra si ridimensiona liberamente (angolo in basso a destra) e la dimensione viene ricordata; ingrandita o a "
    "schermo intero occupa tutto lo schermo.",
    "Trascina in verticale le manopole; <b>Shift</b> = movimento più fine; <b>doppio clic</b> = valore predefinito del comando. "
    "Il suono segue i comandi in modo continuo, senza scalini né click.",
    "La tastiera in basso suona con il mouse e mostra le note MIDI ricevute.",
])
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- 3
h1("3. Browser e preset utente")
img("gui_browser.png", 160)
bullets([
    "<b>Ricerca</b> per preset, canzone, artista, strumento o genere.",
    "<b>Sort</b>: per anno, titolo della canzone, nome del preset o artista. Con l'ordine per anno i pulsanti dei "
    "<b>decenni</b> saltano al punto giusto.",
    "Un clic carica il preset; poi le frecce <b>su / giù</b> della tastiera caricano il precedente e il successivo; "
    "<b>Invio</b> o doppio clic chiude il browser.",
    "<b>USER</b>: i tuoi preset. Tasto destro su un preset: <i>Elimina</i> o <i>Mostra nella cartella</i>.",
    "<b>SAVE</b> apre la finestra di salvataggio: scrivi il nome (o clicca un preset esistente per sovrascriverlo) e premi SAVE. "
    "I file <i>.fampat</i> sono in ~/Library/Audio/Presets/TMFU audiovibe/FamPat.",
])

# ------------------------------------------------------------------------------------------- 4
h1("4. I suoni e i circuiti")
p("Ogni patch suona con il circuito dello strumento originale; i comandi interni non sono modificabili, così il suono resta "
  "quello del preset.")
table([
    ["Strumenti", "Circuito"],
    ["Minimoog, Moog modular, Polymoog, Source, Sub 37; ARP 2500, 2600, Odyssey", "Ladder a transistor a 4 poli non lineare. Minimoog: forme d'onda originali e contorni Attack / Decay / Sustain con l'interruttore DECAY."],
    ["Roland Juno-60 / 106, Jupiter-8, SH-101, JX-8P, Alpha Juno", "IR3109 a 4 stadi OTA; Juno, JX-8P e Alpha Juno con DCO stabili."],
    ["Roland TB-303", "Diode ladder a 4 stadi accoppiati con il passa-alto nell'anello di risonanza; accent con il circuito \"sweep\"."],
    ["Oberheim OB-X", "Filtro SEM a 2 poli (12 dB/ottava)."],
    ["Oberheim OB-Xa, Sequential Prophet-5", "Curtis CEM3320 a 4 poli."],
    ["Korg MS-20", "2 poli con risonanza limitata da diodi, fino all'auto-oscillazione."],
    ["Yamaha CS-80", "VCF a 12 dB/ottava."],
    ["EMS VCS3 / Synthi AKS", "Diode ladder."],
    ["PPG Wave 2.3", "Wavetable a 8 bit che cambiano onda a scatti + filtro SSM2044."],
    ["Yamaha DX7, TX-816, DX21, TX81Z", "FM a 6 / 4 operatori, DAC a 12 bit con esponente."],
    ["Altri digitali (D-50, M1, Triton, Fairlight, Wavestation, JP-8000 / 8080, Virus, Nord Lead, Massive, Sylenth1, Serum…)", "Motori FM, wavetable, supersaw, phase distortion e additivo."],
], [70, 100])
p("Quando lo strumento usato in un brano non è documentato con certezza, la scheda della patch lo dice.")
story.append(PageBreak())

# ------------------------------------------------------------------------------------------- 5
h1("5. Elenco dei preset (%d)" % n)
rows = [["Anno", "Preset", "Genere", "Brano", "Artista", "Strumento"]]
for pr in sorted(lib["presets"], key=lambda x: (x["year"], x["name"])):
    rows.append([pr["year"], pr["name"], pr["genre"], pr["song"], pr["artist"], pr["synth"]])
table(rows, [10, 34, 21, 37, 33, 39])

# ------------------------------------------------------------------------------------------- 6
h1("6. Note")
bullets([
    "Nella DAW sono automatizzabili solo i comandi visibili: Master, inviluppi ed effetti.",
    "Marchi e nomi degli strumenti appartengono ai rispettivi proprietari e sono usati solo per indicare lo strumento di "
    "riferimento. Nessun campione, logo o grafica originale è incluso.",
    "Font dell'interfaccia: Inter; di questo manuale: Urbanist (SIL Open Font License). Logo: Rostex.",
])

def footer(c, doc):
    c.saveState()
    c.setFont("Urbanist", 7.5)
    c.setFillColor(colors.HexColor("#888888"))
    c.drawString(18 * mm, 10 * mm, "FamPat %s — Manuale d'uso" % version)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=16 * mm,
                        title="FamPat %s — Manuale d'uso" % version, author="TMFU audiovibe")
doc.build(story, onFirstPage=footer, onLaterPages=footer)
print("wrote", OUT)
