#!/usr/bin/env python3
"""
Builds docs/DUALBOX_Manuale_v<version>.pdf (Italian user manual) with reportlab.
Figures are taken from the GUI snapshots in docs/ (written by DualboxTest: gui_main.png, win_*.png, B_lfo_panel8.png).
Usage:  python3 tools/make_manual.py [version]
"""
import os, sys, io
from PIL import Image as PI
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image, Table, TableStyle,
                                PageBreak, KeepTogether, NextPageTemplate, Flowable)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = sys.argv[1] if len(sys.argv) > 1 else "2.15.0"
OUT = os.path.join(ROOT, "docs", "DUALBOX_Manuale_v%s.pdf" % VERSION)
DOCS = os.path.join(ROOT, "docs")

# ---------------------------------------------------------------- fonts (DejaVu: arrows, notes, triangles)
def find_font(name):
    for d in ("/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/dejavu", "/Library/Fonts", os.path.expanduser("~/Library/Fonts")):
        p = os.path.join(d, name)
        if os.path.exists(p): return p
    raise SystemExit("font not found: " + name)
pdfmetrics.registerFont(TTFont("DV", find_font("DejaVuSans.ttf")))
pdfmetrics.registerFont(TTFont("DV-B", find_font("DejaVuSans-Bold.ttf")))
try: pdfmetrics.registerFont(TTFont("DV-I", find_font("DejaVuSans-Oblique.ttf")))
except SystemExit: pdfmetrics.registerFont(TTFont("DV-I", find_font("DejaVuSans.ttf")))   # no oblique installed
pdfmetrics.registerFont(TTFont("DV-M", find_font("DejaVuSansMono.ttf")))
pdfmetrics.registerFontFamily("DV", normal="DV", bold="DV-B", italic="DV-I", boldItalic="DV-B")

INK = colors.HexColor("#1a1a1a"); DIM = colors.HexColor("#6b6b6b"); RULE = colors.HexColor("#1a1a1a")
BLUE = colors.HexColor("#4f8cff"); LIME = colors.HexColor("#b4dc3c"); GRID = colors.HexColor("#c8c8c8")
HEAD = colors.HexColor("#ececec"); TIPBG = colors.HexColor("#f3f6ea")

H1 = ParagraphStyle("h1", fontName="DV-B", fontSize=19, leading=24, textColor=INK, spaceBefore=0, spaceAfter=10)
H2 = ParagraphStyle("h2", fontName="DV-B", fontSize=12.5, leading=16, textColor=INK, spaceBefore=10, spaceAfter=5)
H3 = ParagraphStyle("h3", fontName="DV-B", fontSize=10, leading=13, textColor=INK, spaceBefore=6, spaceAfter=3)
P = ParagraphStyle("p", fontName="DV", fontSize=9, leading=12.6, textColor=INK, spaceAfter=5)
BUL = ParagraphStyle("b", parent=P, leftIndent=11, bulletIndent=1, spaceAfter=2.5)
CELL = ParagraphStyle("c", fontName="DV", fontSize=7.8, leading=10, textColor=INK)
CELLB = ParagraphStyle("cb", parent=CELL, fontName="DV-B")
CAP = ParagraphStyle("cap", fontName="DV-I", fontSize=7.8, leading=10, textColor=DIM, spaceBefore=3, spaceAfter=8)
TIP = ParagraphStyle("tip", parent=P, fontSize=8.6, leading=12, spaceAfter=0)
TOC1 = ParagraphStyle("toc1", fontName="DV-B", fontSize=10, leading=17, leftIndent=0)
TOC2 = ParagraphStyle("toc2", fontName="DV", fontSize=8.5, leading=14, leftIndent=14, textColor=colors.HexColor("#555555"))

story = []
def h1(t):
    story.append(PageBreak()); story.append(Heading(t, H1, 0))
    story.append(Rule())
def h2(t): story.append(Heading(t, H2, 1))
def h3(t): story.append(Paragraph(t, H3))
def p(t): story.append(Paragraph(t, P))
def bullets(items):
    for it in items: story.append(Paragraph(it, BUL, bulletText="•"))
def tip(t):
    tb = Table([[Paragraph("<b>Suggerimento</b>&nbsp;&nbsp;" + t, TIP)]], colWidths=[170 * mm])
    tb.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), TIPBG), ("LINEBEFORE", (0, 0), (0, -1), 2.5, LIME),
                            ("LEFTPADDING", (0, 0), (-1, -1), 8), ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                            ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    story.append(Spacer(1, 4)); story.append(tb); story.append(Spacer(1, 6))
def table(rows, widths, bold_first_col=True):
    data = []
    for r, row in enumerate(rows):
        cells = []
        for c, v in enumerate(row):
            st = CELLB if (r == 0 or (c == 0 and bold_first_col)) else CELL
            cells.append(Paragraph(v, st))
        data.append(cells)
    tb = Table(data, colWidths=[w * mm for w in widths], repeatRows=1)
    tb.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, 0), HEAD), ("GRID", (0, 0), (-1, -1), 0.4, GRID),
                            ("VALIGN", (0, 0), (-1, -1), "TOP"), ("TOPPADDING", (0, 0), (-1, -1), 3),
                            ("BOTTOMPADDING", (0, 0), (-1, -1), 3), ("LEFTPADDING", (0, 0), (-1, -1), 4),
                            ("RIGHTPADDING", (0, 0), (-1, -1), 4)]))
    story.append(tb); story.append(Spacer(1, 6))

def _img(im, w_mm):
    buf = io.BytesIO(); im.save(buf, "PNG"); buf.seek(0)
    return Image(buf, width=w_mm * mm, height=w_mm * mm * im.height / im.width)
def fig(src, caption, w=170, crop=None):
    im = PI.open(os.path.join(DOCS, src)).convert("RGB")
    if crop: im = im.crop((crop[0], crop[1], crop[0] + crop[2], crop[1] + crop[3]))
    story.append(KeepTogether([_img(im, w), Paragraph(caption, CAP)]))

class Heading(Paragraph):
    def __init__(self, text, style, level):
        super().__init__(text, style); self.toc_level = level
class Rule(Flowable):
    def wrap(self, aw, ah): self.w = aw; return aw, 8
    def draw(self):
        self.canv.setStrokeColor(RULE); self.canv.setLineWidth(1.6); self.canv.line(0, 6, self.w, 6)

class Doc(BaseDocTemplate):
    def afterFlowable(self, f):
        if isinstance(f, Heading):
            self.notify("TOCEntry", (f.toc_level, f.getPlainText(), self.page))

def cover(c, doc):
    W, H = A4
    c.setFillColor(colors.HexColor("#1e1e1e")); c.rect(0, 0, W, H, stroke=0, fill=1)
    x, y = 22 * mm, H - 80 * mm
    c.setFont("DV-B", 46); c.setFillColor(colors.HexColor("#eeeeee")); c.drawString(x, y, "DUAL")
    dw = pdfmetrics.stringWidth("DUAL", "DV-B", 46)
    c.setFillColor(colors.HexColor("#8c8c8c")); c.drawString(x + dw + 2, y, "BOX")
    bw = pdfmetrics.stringWidth("BOX", "DV-B", 46)
    bx = x + dw + bw + 12
    c.setFillColor(BLUE); c.roundRect(bx, y, 6, 30, 1.5, stroke=0, fill=1)
    c.setFillColor(LIME); c.roundRect(bx + 9, y, 6, 30, 1.5, stroke=0, fill=1)
    c.setFillColor(colors.HexColor("#d0d0d0")); c.setFont("DV", 11); c.drawString(x + 1, y - 18, "TMFU audiovibe")
    c.setFont("DV", 15); c.setFillColor(colors.HexColor("#e0e0e0"))
    c.drawString(x, y - 46, "Manuale d'uso  ·  versione %s" % VERSION)
    c.setFont("DV", 9.5); c.setFillColor(colors.HexColor("#9a9a9a"))
    c.drawString(x, y - 64, "Groovebox doppia VST3 / AU: drum synth (A) + sample groovebox (B)")
    im = PI.open(os.path.join(DOCS, "gui_main.png")).convert("RGB")
    iw = W - 44 * mm; ih = iw * im.height / im.width
    buf = io.BytesIO(); im.save(buf, "PNG"); buf.seek(0)
    from reportlab.lib.utils import ImageReader
    c.drawImage(ImageReader(buf), x, 38 * mm, iw, ih)
    c.setFont("DV", 7); c.setFillColor(colors.HexColor("#7a7a7a"))
    c.drawString(x, 20 * mm, "© TMFU audiovibe. Tutti i nomi di prodotti citati appartengono ai rispettivi proprietari.")

def page(c, doc):
    W, H = A4
    c.setFont("DV-B", 7.5); c.setFillColor(INK); c.drawString(20 * mm, H - 12 * mm, "DUALBOX")
    c.setFont("DV", 7.5); c.setFillColor(DIM)
    c.drawString(20 * mm + pdfmetrics.stringWidth("DUALBOX ", "DV-B", 7.5), H - 12 * mm, "·  Manuale d'uso v%s" % VERSION)
    c.setStrokeColor(GRID); c.setLineWidth(0.5); c.line(20 * mm, H - 14 * mm, W - 20 * mm, H - 14 * mm)
    c.drawRightString(W - 20 * mm, 10 * mm, str(doc.page))

doc = Doc(OUT, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=20 * mm, bottomMargin=18 * mm,
          title="DUALBOX — Manuale d'uso", author="TMFU audiovibe", subject="Manuale d'uso del plugin DUALBOX")
fr = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")
doc.addPageTemplates([PageTemplate(id="cover", frames=[fr], onPage=cover), PageTemplate(id="body", frames=[fr], onPage=page)])

# =============================================================================================== content
story.append(NextPageTemplate("body")); story.append(PageBreak())
story.append(Paragraph("Indice", H1))
toc = TableOfContents(); toc.levelStyles = [TOC1, TOC2]; toc.dotsMinLevel = 0
story.append(toc)

# ---------------------------------------------------------------------------------------------- 1
h1("1. Introduzione")
p("<b>DUALBOX</b> è uno strumento virtuale (VST3 e Audio Unit) che riunisce in un unico plugin due drum machine "
  "con caratteri complementari, sommate e miscelate in uscita:")
bullets([
    "<b>Sezione A – Drum Synth</b> (in alto, colore blu): 8 canali di percussioni <b>sintetizzate</b>, ognuno con "
    "oscillatore, generatore di rumore filtrato, miscelazione e sensibilità alla velocity. 12 pattern (A–L) fino a 64 step.",
    "<b>Sezione B – Groovebox</b> (in basso, colore lime): una groovebox a <b>campioni</b> con 5 voci (Kick, Snare, Hat, "
    "Belt, Capsule), sequencer con pattern A–D fino a 64 step, motore Remix e una catena di effetti completa.",
    "<b>GLOBAL CONTROL</b> (a destra, colore bianco): crossfader A↔B, filtro master Cutoff con modalità HP/BP/LP e "
    "Resonance, volume Master, Tempo e clock interno.",
])
h2("1.1 Flusso del segnale")
p("Le due sezioni lavorano in parallelo. Le loro uscite vengono miscelate dal crossfader <b>A/B Mix</b> e passano poi "
  "attraverso il filtro master prima di arrivare all'uscita del plugin.")
table([["Stadio", "Descrizione"],
       ["Sezione A", "8 voci sintetiche → <b>Level A</b>"],
       ["Sezione B", "5 voci → Filter → EQ/Sat → (Delay → xFx) → FX Mix → Trash → <b>Gain (Level B)</b> → NORM"],
       ["Mix A/B", "Crossfade lineare: 0 % = solo A, 50 % = 50 % A + 50 % B, 100 % = solo B"],
       ["Cutoff", "Filtro state-variable a 2 poli: High Pass, Band Pass o Low Pass, con Resonance"],
       ["Master", "Volume finale del plugin"]], [30, 140])

h2("1.2 Novità della versione %s" % VERSION)
bullets([
    "<b>Novità 2.15</b>: il plugin si chiama <b>DUALBOX</b> (prima <i>Twinbox</i>): nel browser della DAW compare come "
    "<b>TMFU audiovibe: DUALBOX</b>. Nuovo logo con la scritta TMFU audiovibe; l'interfaccia usa il font <b>Urbanist</b> "
    "e tutte le scritte di moduli e parametri sono in inglese. I campioni si trovano in <font face='DV-M'>…/Dualbox/Samples</font> "
    "con le cartelle di fabbrica <b>Dualbox Kick / Snare / Hat / Belt / Capsule / Click</b>; il pulsante della cartella di "
    "fabbrica si chiama <b>DBX</b>. L'installer macOS installa anche i campioni di fabbrica e rimuove le vecchie copie "
    "di Twinbox.",
    "<b>Novità 2.14</b>: modulo <b>CLICK</b> nella Sezione A: un campione di transiente suonato in layer con il Kick "
    "(canale 1), dalla cartella di fabbrica Dualbox Click (12 click) o da una tua cartella, con Gain, Tune, Decay e HP.",
    "<b>Novità 2.13</b>: il knob <b>WAV</b> (scelta del campione) si muove con le frecce; risoluzione più fine dei knob "
    "(Pan a passi di 1, parametri continui a passi dello 0,2 %); nel browser della DAW il produttore si chiama "
    "<b>TMFU audiovibe</b>.",
    "<b>Novità 2.12</b>: Sezione A in <b>blu</b> e GLOBAL CONTROL in <b>bianco</b>; <b>Tie</b> diventa <b>Belt</b>, "
    "<b>Pod</b> diventa <b>Capsule</b>, <b>Dirt</b> diventa <b>Trash</b>; tolti i moduli Pulp e Magic; knob <b>A</b> "
    "(attacco) su Kick, Snare e Hat; pulsanti <b>NORM</b> (normalizzatore + limiter), <b>MIDI</b> (la tastiera suona la "
    "sezione scelta) e <b>SEQ</b> (sequencer acceso/spento) in A e B; volumi di A e B bilanciati e limiter di sicurezza "
    "sull'uscita; knob a risoluzione fine e controllabili con le frecce; doppio clic al centro per Acc, V>A, Pan e Tune; "
    "finestra libera di allargarsi fino a riempire lo schermo.",
    "<b>Novità 2.11</b>: fino a <b>8 LFO per sezione</b>. Nel modulo LFO i pulsanti <b>+</b> e <b>−</b> aggiungono o "
    "tolgono LFO (da 1 a 8, default 3); con più di 3 LFO compaiono le pagine <b>1-3 / 4-6 / 7-8</b>. La finestra grande "
    "mostra tutti gli LFO attivi in griglia, con <b>+ Add LFO</b> e <b>− Remove last LFO</b>.",
    "<b>Novità 2.10</b>: <b>3 LFO per la Sezione A</b> (pannello LFO a destra dell'editor dei suoni) e <b>3 LFO per la "
    "Sezione B</b> (modulo LFO nella riga degli effetti). Ogni LFO può modulare <b>qualsiasi parametro della propria "
    "sezione</b>, con 7 forme d'onda, modalità <b>SYNC</b> (divisioni musicali, agganciate alla posizione del brano) o "
    "<b>RATE</b> in millisecondi, Depth bipolare e Phase.",
    "<b>Novità 2.9</b>: il modulo <b>Hat</b> della Sezione B ha il selettore del filtro <b>LP / BP / HP</b> (come Snare, "
    "Belt e Capsule; default HP) e il knob <b>Sat</b>; anche il modulo <b>Capsule</b> ha il knob <b>Sat</b>.",
    "<b>Novità 2.8</b>: <b>NUDGE</b> per traccia: ogni canale della Sezione A e ogni modulo sample della Sezione B ha un "
    "piccolo display in millisecondi con le frecce ▲▼ per anticipare (valori negativi) o ritardare (valori positivi) la "
    "traccia fino a ±50 ms, senza aggiungere latenza.",
    "<b>Novità 2.7</b>: il modulo <b>Belt</b> contiene solo <b>bassi e accordi, tutti in Do</b>; il modulo <b>Capsule</b> "
    "contiene <b>tom 808 e 909 Hi/Mid/Low, conga 808 Hi/Mid/Low</b> e percussioni non metalliche (conga, bongo, tom, "
    "darbuka, cajon, claves, log drum, snare morbide), più alcuni dissonant hit morbidi. Parte dei campioni viene da "
    "librerie gratuite CC0 / Public Domain: crediti e licenze in SAMPLE_CREDITS.md.",
    "<b>Novità 2.6</b>: <b>velocity per step</b> su BELT e CAPSULE (tacca bianca, Alt/Option + trascina o Alt + frecce), "
    "anche con VEL = wav; 8 groove SIMPLE diversi per ogni genere; libreria con kick puliti, snare e clap morbidi, "
    "tom, conga, bongo, rimshot e shaker. <b>Tutti gli effetti partono a −60 dB / spenti</b>.",
    "<b>Novità 2.5</b>: modifica delle colonne BELT / CAPSULE / MOD con le <b>frecce della tastiera</b>; preset divisi in "
    "<b>SIMPLE</b> e <b>COMPLEX</b>; nuova libreria di suoni e nuovi kit per ogni genere.",
    "<b>Novità 2.4</b>: con <b>VEL = wav</b> l'altezza delle colonne BELT/CAPSULE seleziona il campione della lista (una "
    "tacca per campione, max 128 per modulo).",
    "<b>Novità 2.3</b>: opzione <b>NAME</b> per Belt e Capsule: la tonalità di ogni campione può essere letta dal "
    "<b>nome del file</b> (es. 'pad chord Db' = D♭).",
    "<b>Novità 2.2</b>: rilevamento automatico dell'intonazione dei campioni, modalità <b>SCALE</b> per Belt e Capsule "
    "(Root e scala condivise), modalità <b>MANUAL</b> con finestra ♪ e pitch per singolo campione (±32 semitoni, "
    "regolabile in tempo reale); il Delay parte da −60 dB in ogni preset.",
    "Pattern fino a 64 step in entrambe le sezioni; 128 preset di genere per sezione con browser; cartelle di campioni "
    "per modulo (Dualbox / Legacy / cartella utente); registratore di movimento del pad XY di xFx.",
])

# ---------------------------------------------------------------------------------------------- 2
h1("2. Installazione e primo avvio")
h2("2.1 macOS (Intel e Apple Silicon)")
bullets([
    "Apri il file <b>DUALBOX-%s-macOS.pkg</b> (all'interno dello zip scaricato)." % VERSION,
    "Il plugin non è firmato con un certificato sviluppatore Apple: al primo avvio macOS lo blocca. Su macOS 13–14 fai "
    "clic destro sul file → <b>Apri</b> → <b>Apri</b>. Su macOS 15 e successivi vai in <b>Impostazioni di Sistema → "
    "Privacy e sicurezza</b> e premi <b>Apri comunque</b>.",
    "Nella schermata Personalizza puoi scegliere <b>Audio Unit</b>, <b>VST3</b> e <b>Factory samples</b> (tutti "
    "selezionati di default).",
    "I plugin vengono installati in <font face='DV-M'>/Library/Audio/Plug-Ins/Components</font> (AU) e "
    "<font face='DV-M'>/Library/Audio/Plug-Ins/VST3</font> (VST3); i campioni di fabbrica in "
    "<font face='DV-M'>/Users/Shared/Dualbox/Samples</font>. I binari sono universali: funzionano sia su Mac Intel sia "
    "su Apple Silicon.",
    "Se sul Mac c'era una versione precedente con il vecchio nome (Twinbox.component / Twinbox.vst3), l'installer la "
    "rimuove.",
])
h2("2.2 Caricare il plugin nella DAW")
table([["DAW", "Procedura"],
       ["Logic Pro / GarageBand", "Traccia strumento → Instrument → AU Instruments → <b>TMFU audiovibe → DUALBOX</b>. "
        "Se non compare: Logic Pro → Impostazioni → Plug-in Manager → Reset &amp; Rescan Selection."],
       ["Ableton Live", "Impostazioni → Plug-in: attiva Use Audio Units e/o VST3 System Folders, premi <b>Rescan</b>. "
        "Il plugin si trova in Plug-in → TMFU audiovibe. Live 9 non supporta i VST3 (arrivati con Live 10.1): usa la "
        "versione AU (Audio Units)."],
       ["Cubase, Reaper, Bitwig, Studio One", "Esegui una nuova scansione dei plugin VST3; DUALBOX appare tra gli "
        "strumenti (Instrument / Drum)."]], [42, 128])
h2("2.3 Primo avvio")
p("Su macOS l'installer copia già la libreria di fabbrica. Se le cartelle mancano (per esempio su Windows o Linux, o "
  "se hai deselezionato Factory samples), DUALBOX le crea alla prima apertura e vi copia la libreria:")
table([["Sistema", "Cartella dei campioni"],
       ["macOS", "<font face='DV-M'>/Users/Shared/Dualbox/Samples</font>"],
       ["Windows", "<font face='DV-M'>C:\\Users\\Public\\Documents\\Dualbox\\Samples</font>"],
       ["Linux", "<font face='DV-M'>~/Documents/Dualbox/Samples</font>"]], [30, 140])
p("Premi <b>Play</b> nella DAW (oppure <b>RUN</b> nel pannello GLOBAL CONTROL in assenza di trasporto) e sentirai "
  "subito i pattern di esempio di entrambe le sezioni. I file che aggiungi alle cartelle non vengono mai toccati dagli "
  "aggiornamenti della libreria di fabbrica.")

# ---------------------------------------------------------------------------------------------- 3
h1("3. L'interfaccia")
fig("gui_main.png", "Fig. 1 – Interfaccia principale: barra del titolo, Sezione A (in alto), Sezione B (in basso), "
    "GLOBAL CONTROL (a destra).")
h2("3.1 Struttura")
bullets([
    "<b>Barra del titolo</b>: logo DUALBOX con la scritta TMFU audiovibe, le due tacche colorate delle sezioni e il "
    "numero di versione.",
    "<b>Sezione A</b>: intestazione (pattern A–L, BROWSER, Morph, Level A), lista canali, editor della drum patch, "
    "pannello Pattern.",
    "<b>Sezione B</b>: intestazione (BROWSER, IMPORT LEGACY, SCALE, Level B) e tre file di pannelli: sequencer e "
    "remix, i cinque moduli sample, la catena degli effetti.",
    "<b>GLOBAL CONTROL</b>: pannello comprimibile sul lato destro.",
])
h2("3.2 Comandi comuni")
table([["Azione", "Risultato"],
       ["Trascinare una manopola su/giù", "Cambia il valore con risoluzione fine (700 pixel per tutta l'escursione); "
        "con Shift premuto è 4 volte più fine. La riga sotto la manopola mostra il valore corrente con l'unità di misura."],
       ["Clic su una manopola + frecce", "La manopola cliccata ha un bordo colorato: ↑ / → aumentano e ↓ / ← "
        "diminuiscono il valore: Pan di 1 (verso R / L), i parametri a scatti (menu, numeri interi, scelta del campione "
        "WAV) di uno scatto, i parametri continui (Cutoff, Gain, Sat, Decay, ...) dello 0,2 % dell'escursione (con Shift "
        "dello 0,02 %)."],
       ["Doppio clic su una manopola", "Riporta il parametro al valore di default. Nei moduli della Sezione B, Acc, V>A, "
        "Pan e Tune tornano al centro (ore 12)."],
       ["Passare il mouse su un controllo", "Mostra il nome completo del parametro (tooltip)."],
       ["Pulsante ↗ nell'angolo di un pannello", "Apre il pannello in una finestra grande, ridimensionabile, con "
        "controlli più grandi e più leggibili."],
       ["Trascinare l'angolo della finestra del plugin", "Ridimensiona l'interfaccia: l'altezza decide la scala, la "
        "larghezza in più allarga le sezioni. Massimizzando la finestra il plugin occupa tutto lo schermo."],
       ["NORM (intestazione di A e B)", "Normalizza l'uscita della sezione (guadagno automatico verso un livello medio "
        "costante, ±12 dB) e attiva un limiter a −1 dBFS: la sezione non va mai in clipping. La spia LIM si accende "
        "quando il limiter lavora."],
       ["MIDI (intestazione di A e B)", "Sceglie quale sezione suona la tastiera / i pad MIDI: una sola alla volta "
        "(vedi 9.2)."],
       ["SEQ (intestazione di A e B)", "Accende / spegne il sequencer della sezione, indipendentemente dal MIDI. "
        "Spento: i pattern continuano a scorrere in silenzio (la posizione resta allineata) e la sezione suona solo con "
        "la tastiera MIDI. Acceso di default; non cambia caricando un preset."],
       ["Clic sull'intestazione GLOBAL CONTROL", "Comprime / espande il pannello globale."]], [52, 118])
tip("Tutti i parametri sono automatizzabili dalla DAW. Pattern, nomi dei canali, cartelle dei campioni, morph, "
    "movimento XY registrato e stato dell'interfaccia vengono salvati con il progetto.")

# ---------------------------------------------------------------------------------------------- 4
h1("4. GLOBAL CONTROL")
p("Il pannello a destra controlla la miscela delle due sezioni e il suono finale.")
gc = [["Controllo", "Funzione"],
      ["A / B Mix", "Crossfader lineare 0–100 %. A sinistra solo A, al centro 50 % + 50 %, a destra solo B."],
      ["CUTOFF", "Frequenza del filtro master, 20 Hz – 20 kHz (manopola grande)."],
      ["HP / BP / LP", "Modalità del filtro: passa-alto, passa-banda, passa-basso. In LP a 20 kHz (o in HP a 20 Hz) il "
       "filtro è escluso."],
      ["Resonance", "Enfasi sulla frequenza di taglio, 0–100 %."],
      ["Curva", "Visualizza in tempo reale la risposta del filtro."],
      ["Master", "Volume d'uscita, −60 … +6 dB, con indicatore di livello."],
      ["Tempo", "Tempo del clock interno, 40–240 BPM (usato quando la DAW è ferma o in Standalone)."],
      ["RUN", "Avvia/ferma il clock interno. L'etichetta indica HOST SYNC, INTERNAL CLOCK o STOPPED."]]
im = PI.open(os.path.join(DOCS, "gui_main.png")).convert("RGB").crop((1794, 6, 1794 + 257, 6 + 1220))
gt = Table([[_img(im, 38), Table([[Paragraph(v, CELLB if (r == 0 or c == 0) else CELL) for c, v in enumerate(row)]
                                  for r, row in enumerate(gc)], colWidths=[25 * mm, 100 * mm],
                                 style=TableStyle([("BACKGROUND", (0, 0), (-1, 0), HEAD), ("GRID", (0, 0), (-1, -1), 0.4, GRID),
                                                   ("VALIGN", (0, 0), (-1, -1), "TOP")]))]],
           colWidths=[44 * mm, 126 * mm])
gt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
story.append(gt)
tip("Usa il filtro master per le transizioni: con il crossfader al centro, un HP risonante che sale lentamente svuota "
    "il mix di entrambe le sezioni contemporaneamente.")

# ---------------------------------------------------------------------------------------------- 5
h1("5. Sezione A – Drum Synth")
fig("gui_main.png", "Fig. 2 – Sezione A.", crop=(12, 46, 1772, 494))
p("La Sezione A genera 8 percussioni interamente per sintesi. Ogni canale ha una propria drum patch composta da un "
  "oscillatore con modulazione di pitch, un generatore di rumore filtrato, uno stadio di miscelazione e la sensibilità "
  "alla velocity.")
h2("5.1 Intestazione")
table([["Controllo", "Funzione"],
       ["PATTERN A–L", "Seleziona il pattern da modificare/suonare. Durante la riproduzione il cambio avviene alla fine "
        "del pattern in corso (il pulsante del pattern in attesa si illumina in tenue)."],
       ["BROWSER", "Apre il browser dei 128 preset di genere della Sezione A (vedi capitolo 8)."],
       ["Store L / Morph / Store R", "Store L e Store R memorizzano lo stato attuale degli 8 drum patch come estremi "
        "sinistro e destro. La manopola Morph interpola tra i due set (i parametri a scelta commutano al 50 %). I "
        "pulsanti si colorano quando l'estremo è memorizzato."],
       ["Level A", "Volume della Sezione A, −60 … +6 dB, con indicatore di livello."]], [36, 134])
h2("5.2 Canali")
p("La colonna di sinistra elenca gli 8 canali (di default Kick, Snare, CH, OH, Clap, Tom, Rim, Bell). Clicca su un "
  "canale per selezionarlo: l'editor della patch e la griglia dei pattern mostrano quel canale. Per ogni canale:")
bullets([
    "<b>LED</b>: lampeggia a ogni colpo.",
    "<b>NUDGE</b> (display 0 ms con le frecce ▲▼): sposta nel tempo tutte le note del canale, da −50 ms (anticipo) a "
    "+50 ms (ritardo). ▲ = +1 ms, ▼ = −1 ms (con Shift ±0,1 ms; tieni premuto per ripetere). Puoi anche trascinare in "
    "verticale sul display o usare la rotella; doppio clic = 0 ms.",
    "<b>▶</b>: ascolta il suono.  <b>M</b>: mute del canale.",
    "<b>Campo nome</b> (sotto la lista): doppio clic per rinominare il canale selezionato.",
    "<b>Copy / Paste</b>: copia e incolla l'intera drum patch tra canali.",
])
h3("CLICK (layer di transiente del Kick)")
p("Sotto la lista dei canali c'è il modulo CLICK: aggiunge al Kick (canale 1) un breve campione di transiente (beater, "
  "click, tick...) suonato insieme a ogni colpo del Kick (sequencer, MIDI o ▶).")
table([["Controllo", "Funzione"],
       ["ON", "Accende / spegne il layer (spento di default)."],
       ["DBX · cartella", "Cartella dei campioni: DBX = Dualbox Click (12 click di fabbrica: Beater Hard / Soft, 909 "
        "Attack, Wood Tick, Tick Short, Knock, Felt Thump, Tape Click, Air Snap, Plastic Pop, Mid Punch, Dusty Click); "
        "icona cartella = una tua cartella di transienti (WAV, AIFF, FLAC, OGG; max 128 file)."],
       ["Menu · ▶", "Scelta del campione e ascolto (Kick + click)."],
       ["Gain", "Volume del click, −60 … +6 dB (scala anche con la velocity del colpo)."],
       ["Tune", "Intonazione ±24 semitoni (più alto = click più corto e brillante)."],
       ["Decay", "Durata: da 3 ms (0 %) a tutto il campione (100 %)."],
       ["HP", "Filtro passa-alto 20 Hz – 8 kHz: toglie il corpo del click e lascia solo l'attacco."]], [30, 140])
h2("5.3 Drum patch – parametri")
table([["Pannello", "Parametro", "Range", "Descrizione"],
       ["Oscillator", "Wave", "Sine / Tri / Saw", "Forma d'onda dell'oscillatore."],
       ["", "Osc Freq", "20 Hz – 20 kHz", "Frequenza di base."],
       ["", "Attack / Decay", "0–10 s / 10 ms–10 s", "Inviluppo d'ampiezza dell'oscillatore (attacco lineare, "
        "decadimento esponenziale)."],
       ["", "Pitch Mod", "Decay / Sine / Random", "Tipo di modulazione del pitch: inviluppo decrescente (kick, tom), "
        "LFO sinusoidale (vibrato/FM), rumore filtrato (hat metallici)."],
       ["", "Mod Amount", "±96 semitoni", "Profondità della modulazione di pitch."],
       ["", "Mod Rate", "0–100 %", "Velocità: in Decay è il tempo di caduta (1 ms – 10 s), in Sine la frequenza "
        "(0,1 Hz – 20 kHz), in Random il taglio del rumore."],
       ["Noise", "Filter", "LP / BP / HP", "Tipo di filtro sul rumore."],
       ["", "Filter Freq / Q", "20 Hz–20 kHz / 0,1–100", "Frequenza e risonanza del filtro del rumore."],
       ["", "Stereo", "On / Off", "Rumore indipendente sui due canali (immagine stereo più larga)."],
       ["", "Envelope", "Exp / Lin / Mod", "Forma dell'inviluppo del rumore; Mod produce le raffiche tipiche del clap."],
       ["", "Attack / Decay", "0–10 s / 10 ms–10 s", "Inviluppo del rumore."],
       ["Mixing", "Osc / Noise", "100/0 … 0/100", "Bilanciamento tra oscillatore e rumore."],
       ["", "Distort", "0–100 %", "Distorsione morbida."],
       ["", "EQ Freq / EQ Gain", "20 Hz–20 kHz / ±40 dB", "Equalizzatore a campana."],
       ["", "Level / Pan", "−60…+10 dB / L–R", "Volume e panorama del canale."],
       ["", "Choke", "On / Off", "I canali con Choke attivo si interrompono a vicenda (es. hat chiuso che taglia "
        "l'aperto)."],
       ["Velocity", "Osc / Noise / Mod", "0–200 %", "Quanto la velocity (o l'accento) influisce su volume "
        "dell'oscillatore, del rumore e sulla profondità del Pitch Mod."]], [22, 30, 36, 82])
tip("Il pulsante ↗ del pannello Oscillator apre il Drum Patch Editor grande, con il selettore degli 8 canali in alto.")
h2("5.4 Pattern")
table([["Controllo", "Funzione"],
       ["Lunghezza (− / +)", "Lunghezza del pattern corrente, 1–64 step (ogni pattern ha la sua)."],
       ["1-16 · 17-32 · 33-48 · 49-64", "Pagine della griglia; le pagine oltre la lunghezza sono attenuate."],
       ["Trigger / Accent / Fill", "Le tre righe per il canale selezionato: colpo, accento (velocity 127 invece di 64), "
        "colpo eseguito solo durante il Fill."],
       ["Swing", "Ritarda gli step dispari (0–100 %)."],
       ["Fill Rate", "M = Fill manuale; 2…8 = il Fill parte automaticamente ogni N ripetizioni del pattern."],
       ["FILL", "Attiva i colpi Fill (si illumina anche quando il Fill automatico è in corso)."],
       ["Clear", "Svuota il canale selezionato nel pattern corrente."],
       ["Copy Pat / Paste Pat", "Copia e incolla un intero pattern (tutti i canali e la lunghezza)."]], [42, 128])
h3("Editing con il mouse")
bullets(["Clic su una cella: attiva/disattiva.", "Trascinare lungo una riga: dipinge (o cancella) più step in un gesto.",
         "Clic destro su un Trigger: attiva/disattiva l'accento."])
fig("win_A_pattern.png", "Fig. 3 – Pattern Editor della Sezione A (finestra grande): tutti gli 8 canali con Trigger / "
    "Accent / Fill, 32 step per pagina.", w=150)
h2("5.5 LFO della Sezione A")
fig("gui_main.png", "Fig. 3b – Il pannello LFO della Sezione A (a destra dell'editor dei suoni).", w=60,
    crop=(1538, 99, 240, 250))
p("Da 1 a 8 LFO (3 di default) modulano qualsiasi parametro della Sezione A: i parametri di ognuno degli 8 canali (Osc "
  "Freq, Decay, Distort, Level, Pan, Nudge...) e quelli generali (Swing, Fill Rate, Level A...). Ogni riga del pannello "
  "è un LFO; il pulsante ↗ apre la finestra grande con forma d'onda e Phase. Comandi come nel paragrafo 6.9.")

# ---------------------------------------------------------------------------------------------- 6
h1("6. Sezione B – Groovebox")
fig("gui_main.png", "Fig. 4 – Sezione B: Sequencer / Remix / Out+Global, i 5 moduli sample, XFX / Delay / LFO / Mix / "
    "EQ / Filter.", crop=(12, 559, 1775, 742))
p("Nell'intestazione della Sezione B trovi BROWSER (preset di genere), IMPORT LEGACY (importazione di una libreria di "
  "campioni, vedi capitolo 7), il selettore SCALE (Root e tipo di scala condivisi da Belt e Capsule, vedi 6.5) e "
  "Level B, il volume master della sezione (è lo stesso parametro del GAIN del pannello OUT).")
h2("6.1 SEQUENCER")
table([["Controllo", "Funzione"],
       ["A B C D", "I quattro pattern del kit. Clicca per selezionare quello da suonare/modificare."],
       ["ON · lunghezza · 8 slot · LOCK", "Pattern sequencer: con ON attivo i pattern vengono suonati in catena secondo "
        "gli slot (clicca uno slot per cambiarlo A→B→C→D); la lunghezza (1–8) indica quanti slot usare. Con LOCK la "
        "griglia resta sul pattern selezionato invece di seguire quello in riproduzione."],
       ["1-16 … 49-64", "Pagine della griglia (64 step)."],
       ["COPY / PASTE", "Copia e incolla un pattern completo (tutte le corsie)."],
       ["KICK · SNARE · C-HAT · O-HAT", "Griglia principale. Quadrato piccolo = colpo normale, quadrato grande = "
        "accento. C-HAT e O-HAT suonano rispettivamente il hat chiuso e quello aperto."],
       ["BELT · CAPSULE · MOD", "Corsie a valori (barre). Nella vista normale scegli quale mostrare con i pulsanti BELT, "
        "CAPSULE, MOD in basso a sinistra; nella finestra grande sono visibili tutte."]], [44, 126])
h3("Editing con il mouse e con la tastiera")
bullets([
    "Griglia principale: clic = acceso/spento; clic destro (o Shift+clic) = accento; trascinare = dipingere.",
    "Corsie BELT / CAPSULE: l'altezza della barra è la velocity della nota (0 = nessuna nota). Trascina in verticale per "
    "disegnare; clic destro = spegne lo step. La linea centrale della corsia BELT corrisponde alla nota base quando VEL "
    "è impostato su tune (la barra sopra o sotto la linea alza o abbassa la nota fino a ±12 semitoni).",
    "<b>Selezione del campione con l'altezza (VEL = wav)</b>: se nel modulo Belt o Capsule il menu VEL è su wav, la "
    "colonna sceglie il campione: ha una tacca per ogni campione della lista del modulo (al massimo 128). La prima tacca "
    "dal basso suona l'ultimo campione della lista, la tacca più alta il primo. Il knob WAV viene ignorato (resta valido "
    "per note MIDI e ▶); V>A non si applica. Nella finestra grande il numero della tacca è scritto sulla barra. I preset "
    "della Sezione B usano questa modalità.",
    "Con VEL su qualsiasi altra impostazione (--, tune, att, dcy, st, fm, zap, cut) l'altezza resta il valore dello step, "
    "usato da V>A e dalla destinazione VEL.",
    "<b>Velocity per step</b> (BELT e CAPSULE): ogni nota ha anche una velocity propria, indicata da una tacca bianca "
    "nella colonna. Alt/Option + trascina sulla colonna imposta la velocity senza cambiare il valore o il campione; con "
    "la colonna selezionata Alt + ↑ / ↓ la cambiano del 5 % (con Shift del 25 %).",
    "<b>Tastiera</b>: clicca una colonna BELT, CAPSULE o MOD (si evidenzia con un bordo azzurro), poi usa le frecce. "
    "↑ / ↓ cambiano il valore (con VEL = wav un campione alla volta, che viene ascoltato e il cui nome appare sopra la "
    "colonna; altrimenti 1/32 dell'altezza). Shift fa passi più grandi. ← / → passano allo step precedente o "
    "successivo; Canc o Backspace spengono lo step; Esc toglie la selezione. Se la DAW intercetta i tasti, clicca di "
    "nuovo sul sequencer.",
    "Corsia <b>MOD</b>: un valore 0–1 per step, inviato alle destinazioni MOD dei moduli e agli effetti (Filter MOD, "
    "Mod Input/Mix).",
])
fig("win_B_sequencer.png", "Fig. 5 – Il sequencer della Sezione B nella finestra grande.", w=160)
h2("6.2 REMIX")
table([["Controllo", "Range", "Funzione"],
       ["NOW!", "–", "Esegue il remix alla prossima battuta."],
       ["SHIFT", "−16 … +16", "Sposta la posizione di lettura del pattern (in step)."],
       ["Remix", "0–127", "Schema di remix (0 = nessuno). Gli schemi riordinano gli step della battuta: ripetizione "
        "dell'ultimo quarto, inversioni, stutter, half-time, double-time, rimescolamento dei quarti, roll, scambi casuali."],
       ["Rate", "OFF, 2, 4, 8, 12, 16", "Ogni quante battute il remix viene applicato automaticamente."],
       ["Swing", "0–100 %", "Swing della Sezione B."],
       ["1/2", "On/Off", "Metà tempo (gli step durano un ottavo)."],
       ["Start", "1–64", "Step da cui parte la sequenza principale."],
       ["Len · Belt · Capsule · Mod", "1–64", "Lunghezze indipendenti della sequenza principale e delle corsie Belt, "
        "Capsule e Mod (lunghezze diverse creano poliritmi)."],
       ["Stretch", "On/Off", "Allunga/comprime la corsia Belt, Capsule o Mod in modo che duri esattamente quanto la "
        "sequenza principale."]], [36, 30, 104])
h2("6.3 OUT e GLOBAL")
table([["Controllo", "Range", "Funzione"],
       ["Trash", "0–100 %", "Saturazione morbida: lineare sui segnali deboli, arrotonda solo i picchi, aggiunge armoniche "
        "pari leggere, compensa il livello. Usa 20–50 % per calore, oltre per una saturazione evidente."],
       ["Gain", "−60 … +6 dB", "Volume della Sezione B (= Level B)."],
       ["Tune", "±12 semitoni", "Trasposizione globale di tutti i campioni."],
       ["CLK · BPM", "–", "Clock interno e tempo (gli stessi di RUN e Tempo del GLOBAL CONTROL)."]], [30, 30, 110])
h2("6.4 I moduli sample")
fig("gui_main.png", "Fig. 6 – I moduli Kick, Snare, Hat, Belt e Capsule.", crop=(12, 871, 1775, 229))
p("Ogni modulo ha nell'intestazione il pulsante di accensione (on/mute) e ↗ per la finestra grande con la forma d'onda. "
  "Belt e Capsule hanno in più i pulsanti NAME e SCALE e l'icona ♪ (vedi 6.5). Lo stato acceso/spento di ogni modulo "
  "non viene mai cambiato dal cambio di pattern, dalla catena di pattern o dal caricamento di un preset. La prima riga "
  "sceglie il campione:")
table([["Controllo", "Funzione"],
       ["DBX", "Usa la cartella di fabbrica Dualbox &lt;Modulo&gt;."],
       ["LEG", "Usa la cartella Legacy &lt;Modulo&gt; (dopo un'importazione, vedi 7.1)."],
       ["Icona cartella", "Scegli una cartella tua per questo modulo. Il pulsante resta acceso e il tooltip mostra il "
        "percorso."],
       ["Menu campione · ▶", "Elenco dei file della cartella attiva (in ordine alfabetico/numerico) e ascolto."],
       ["NUDGE (display ms ▲▼)", "Anticipa (−) o ritarda (+) tutte le note del modulo, da −50 a +50 ms. ▲ / ▼ = ±1 ms "
        "(Shift ±0,1 ms), trascina sul display o usa la rotella, doppio clic = 0."],
       ["Wav (knob)", "Scorre i campioni della cartella attiva e mostra il nome del campione corrente. È automatizzabile "
        "e modulabile dalla corsia Mod (destinazione wav)."]], [40, 130])
h3("Parametri dei moduli")
table([["Modulo", "Controlli"],
       ["Kick", "Gain, Acc (quanto suonano più piano i colpi non accentati), Pan, Wav, Tune (±24 st), St (punto di "
        "inizio), A (attacco), H (hold), D (decay; al 100 % il campione suona per intero), Sat (saturazione), Shp (forma "
        "thin … normal … fat), Sm (ammorbidimento), MOD (destinazione della corsia Mod: --, wav, tune, cut), Lo (taglio "
        "delle basse), Cut, Res (filtro passa-basso)"],
       ["Snare", "Come il Kick, con filtro LP / BP / HP al posto di Lo, il controllo Flam (secondo colpo ravvicinato) e "
        "la destinazione Mod flam"],
       ["Hat", "Gain, Acc, Pan, Wav, Tune, CD (decay del chiuso), OD (decay dell'aperto), Link (il chiuso taglia "
        "l'aperto), Sat, MOD, filtro LP / BP / HP (default HP), Cut e Res. Se nella cartella esiste "
        "open/&lt;stesso nome&gt; viene usato per l'aperto"],
       ["Belt", "Voce tonale (bassi, accordi, keys): Gain, V>A (quanto la velocity della corsia agisce sul volume), Pan, "
        "Wav, Tune, Port (portamento), St, A, D, FM (modulazione di frequenza), MP (pitch del modulatore), Noise, Sat, VEL "
        "(destinazione della velocity: --, wav, tune, att, dcy, st, fm, cut), filtro LP/BP/HP, Cut, Res"],
       ["Capsule", "Percussioni/FX: Gain, V>A, Pan, Wav, Tune, St, A, D, Zap (sweep di pitch), Sat, VEL (--, wav, tune, "
        "att, dcy, zap, cut), filtro LP/BP/HP, Cut, Res"]], [22, 148])
h3("Destinazioni di modulazione")
table([["Destinazione", "Effetto"],
       ["wav", "Mod (Kick/Snare/Hat): avanza fino a 8 posizioni nella cartella. VEL (Belt/Capsule): l'altezza della "
        "colonna sceglie il campione della lista (tacca 1 in basso = ultimo campione)."],
       ["tune", "Mod: alza fino a +12 st. VEL (Belt/Capsule): ±12 st intorno alla linea centrale (melodie)."],
       ["cut", "Kick/Snare: chiude il filtro; Hat: alza il passa-alto; Belt/Capsule: sposta il cutoff di ±3 ottave."],
       ["flam", "Snare: aggiunge un flam fino a 50 ms."],
       ["att / dcy / st", "Belt/Capsule: allungano attacco, decay o spostano il punto di inizio in base alla velocity."],
       ["fm / zap", "Belt: aumenta la FM. Capsule: varia lo Zap."]], [28, 142])
fig("win_B_tie.png", "Fig. 7 – Finestra grande del modulo Belt con la forma d'onda e la cartella attiva.", w=125)

h2("6.5 SCALE e intonazione dei campioni (Belt e Capsule)")
p("I campioni di Belt e Capsule hanno spesso tonalità diverse fra loro. DUALBOX offre due modi per farli suonare "
  "intonati: <b>SCALE</b> (automatico) e <b>MANUAL</b> (campione per campione).")
h3("Rilevamento dell'intonazione")
bullets([
    "Quando una cartella viene caricata, DUALBOX analizza ogni campione in background e ne rileva la nota fondamentale "
    "(nota e cent; C3 = nota MIDI 60).",
    "I suoni senza un'altezza chiara (rumori, molte percussioni, accordi) vengono marcati <b>no pitch</b> e restano "
    "invariati.",
    "Il risultato viene salvato nel file nascosto <font face='DV-M'>.dualbox_pitch.txt</font> dentro la cartella: le volte "
    "successive il caricamento è immediato.",
    "Non serve fermare l'audio: mentre l'analisi è in corso il modulo continua a suonare i campioni precedenti; i nuovi "
    "entrano in uso solo quando sono caricati e analizzati.",
])
h3("Tonalità dal nome del file (NAME)")
p("Accordi, pad, molte percussioni e i suoni ricchi di armoniche non hanno un'unica altezza misurabile: l'analisi audio "
  "li segna no pitch. Per questi casi Belt e Capsule hanno il pulsante <b>NAME</b> (nell'intestazione del modulo e nella "
  "finestra ♪, come KEY FROM FILE NAME).")
bullets([
    "<b>NAME acceso</b>: la tonalità di ogni campione viene letta dal nome del file. Se il nome non contiene una "
    "tonalità, DUALBOX usa l'analisi audio.",
    "<b>NAME spento</b>: vale solo l'analisi audio.",
    "La tonalità letta dal nome viene usata dalla modalità SCALE esattamente come quella misurata.",
])
table([["Nome del file", "Tonalità letta", "Note"],
       ["pad chord Db", "D♭ (= C#)", "bemolle con 'b', anche minuscolo ('pad chord db')"],
       ["Rhodes Cm9 · Am7 stab · 120bpm Ebmaj7", "C · A · E♭", "le sigle degli accordi (m, min, maj, sus, add, dim, "
        "aug, numeri) vengono ignorate: conta la fondamentale"],
       ["Bass_F#2 · Kick C1", "F#2 · C1", "un numero subito dopo la nota è l'ottava (C3 = nota MIDI 60)"],
       ["La minore · Sol# lead · Re", "A · G# · D", "nomi solfeggio Do, Re, Mi, Fa, Sol, La, Si"],
       ["Kick · Sub Bass · Cello · Remix", "nessuna", "le parole che iniziano con una lettera di nota non vengono "
        "confuse con le note"]], [52, 30, 88])
p("Se nel nome ci sono più note, conta l'ultima. Senza ottava nel nome, la nota viene collocata nell'ottava più vicina "
  "all'altezza misurata (o intorno a C3). Nella finestra ♪ ogni campione mostra entrambe le letture, audio e name: "
  "quella in uso è evidenziata.")
h3("Modalità SCALE")
table([["Controllo", "Funzione"],
       ["SCALE (intestazione B)", "Root (C … B) e tipo di scala, condivisi da Belt e Capsule: Major, Minor, Dorian, "
        "Mixolydian, Lydian, Phrygian, Locrian, Whole Tone, Half-whole Dim., Whole-half Dim., Minor Blues, Minor "
        "Pentatonic, Major Pentatonic, Harmonic Minor, Harmonic Major, Dorian #4, Phrygian Dominant, Melodic Minor, "
        "Lydian Augmented, Lydian Dominant, Super Locrian, 8-Tone Spanish, Bhairav, Hungarian Minor, Hirajoshi, In-Sen, "
        "Iwato, Kumoi, Pelog Selisir, Pelog Tembung, Messiaen 3–7."],
       ["SCALE (modulo Belt / Capsule)", "Accende la modalità SCALE per quel modulo. Ogni nota suonata parte "
        "dall'intonazione rilevata del campione (più il Tune del modulo) e viene portata sulla nota della scala più "
        "vicina: campioni di tonalità diverse suonano intonati fra loro."],
       ["Melodie", "Con VEL = tune, in modalità SCALE le barre della corsia Belt/Capsule si muovono per gradi della "
        "scala (circa un'ottava sopra e sotto la linea centrale) invece che per semitoni."]], [42, 128])
h3("Modalità MANUAL e finestra ♪")
fig("win_B_pitch.png", "Fig. 8 – Finestra ♪ del modulo Belt: tonalità da audio e da nome del file, ascolto e knob di "
    "pitch per ogni campione.", w=110)
bullets([
    "Con SCALE spento il modulo è in MANUAL. Clicca l'icona ♪ in alto a destra del modulo: si apre la finestra con tutti "
    "i campioni della cartella attiva.",
    "Per ogni campione: ▶ per ascoltarlo, il numero, il nome, la nota rilevata e un knob da −32 a +32 semitoni. Doppio "
    "clic sul knob = 0; <b>Reset all to 0</b> azzera tutti.",
    "Il knob è indipendente dal Tune del modulo, che continua ad agire su tutti i campioni insieme.",
    "Le modifiche si sentono in tempo reale, anche durante la riproduzione.",
    "Le impostazioni vengono salvate con il progetto, per cartella e nome del file. Con SCALE acceso la finestra mostra "
    "le note rilevate e i knob sono disattivati.",
])

fig("gui_main.png", "Fig. 9 – XFX, Delay, LFO, Mix, EQ e Filter.", crop=(12, 1102, 1775, 193))
h2("6.6 XFX")
p("Multi-effetto con 23 modalità, controllato da un pad XY: X = Parameter X, Y = Additional Parameter.")
table([["Modo", "X", "Y"],
       ["highpass / bandpass / lowpass", "Frequenza", "Risonanza"], ["phaser", "Centro dello sweep", "Feedback"],
       ["vowel", "Vocale a-e-i-o-u", "Risonanza"], ["flanger", "Profondità", "Feedback"],
       ["fm", "Frequenza del modulatore", "Profondità"], ["reson", "Intonazione del risonatore", "Feedback"],
       ["feedback", "Tempo del ritardo", "Quantità di feedback"], ["reflex", "Spaziatura delle eco (a tempo)", "Decadimento"],
       ["reverb", "Dimensione", "Smorzamento"], ["ufo", "Frequenza della banda", "Risonanza"],
       ["rainman", "Densità dei grani", "Variazione di pitch"], ["distort", "Drive", "Tono"],
       ["ringmod", "Frequenza", "Quantità"], ["lofi", "Riduzione del sample rate", "Riduzione dei bit"],
       ["pitchshift", "Trasposizione ±12 st", "Dimensione del grano"], ["gate", "Schema di gate a sedicesimi", "Profondità"],
       ["chop", "Velocità di ripetizione", "Quantità"], ["slice", "Probabilità di rimescolare i sedicesimi", "–"],
       ["freeze", "Oltre 50 % congela un loop lungo LEN", "Miscela"], ["spin", "Velocità del nastro (stop / ripartenza)", "–"],
       ["off", "Effetto spento", ""]], [46, 70, 54])
table([["Controllo", "Funzione"],
       ["Dly", "Ingresso di xFx: dal segnale dry (0 %) al segnale post-delay (100 %)."],
       ["Gain", "Livello di ritorno di xFx: −60 … 0 dB, default −60 dB."],
       ["Lo · Hi", "Taglio basso e alto sull'uscita dell'effetto."],
       ["● REC · ▶ PLAY", "Registratore di movimento del pad (vedi sotto)."],
       ["Spd · Len · Sm", "Velocità di registrazione/riproduzione (0,25×–4×), lunghezza del loop (da 1/16 a 4 battute), "
        "morbidezza della riproduzione."]], [32, 138])
h3("Registrare e riprodurre il movimento del pad XY")
bullets([
    "Imposta Len (es. 1 bar) e accendi ● REC: sul pad compare REC ARMED.",
    "Con il trasporto in movimento trascina il pad: il movimento viene scritto in un loop sincronizzato al tempo (scritta "
    "REC rossa). Ogni nuovo trascinamento con REC acceso sostituisce la registrazione.",
    "Accendi ▶ PLAY (puoi spegnere REC): il movimento viene ripetuto in loop; il pad mostra il punto in movimento con la "
    "scia e una barra con la posizione nel loop.",
    "Se tocchi il pad durante il PLAY prendi temporaneamente il controllo; rilasciandolo riparte la riproduzione. Sm "
    "ammorbidisce i salti, Spd accelera o rallenta.",
    "Il movimento registrato viene salvato con il progetto.",
])
h2("6.7 DELAY")
table([["Controllo", "Funzione"],
       ["One · Two", "Tempi del canale sinistro e destro, in sedicesimi (1–16)."],
       ["FB", "Feedback (0–98 %), saturato in modo morbido."],
       ["Gain", "Livello di ritorno del delay: −60 … 0 dB, default −60 dB (al minimo, anche in tutti i preset): alzalo a "
        "mano per sentire il delay."],
       ["LFO · FM · DM", "LFO interno; FM = modulazione del cutoff, DM = modulazione del tempo (effetto nastro)."],
       ["Wide", "Larghezza stereo: da mono a ping-pong completo."],
       ["Cut · Res · Lo", "Filtro passa-basso risonante nel feedback e taglio delle basse (Lo)."]], [32, 138])
h2("6.8 MIX, EQ, FILTER")
table([["Pannello", "Controlli"],
       ["MIX", "FX Mix: dry ↔ wet di delay + xFx (al 50 % dry e wet sono entrambi pieni). Mod Input: la corsia Mod "
        "regola quanto segnale entra negli effetti. Mod Mix: la corsia Mod regola la miscela."],
       ["EQ", "Hi, Mid, Lo (±18 dB) con frequenza di separazione >|< (60 Hz – 1 kHz) e Amp, saturatore morbido "
        "(0–24 dB)."],
       ["FILTER", "Passa-basso con inseguitore d'inviluppo: Cut, Trk (quanto il volume apre/chiude il filtro), Sm "
        "(velocità dell'inseguitore), Mod (corsia Mod → cutoff, bipolare)."]], [24, 146])
fig("win_B_fx.png", "Fig. 10 – Finestra grande Remix, Output &amp; FX (pulsante ↗ dei pannelli effetti).", w=150)
h2("6.9 LFO (Sezione B)")
fig("win_B_lfo8.png", "Fig. 10b – Finestra grande degli LFO (pulsante ↗ del modulo LFO), qui con 8 LFO attivi.", w=150)
fig("B_lfo_panel8.png", "Fig. 10c – Il modulo LFO con 8 LFO: pulsanti − / + e pagine 1-3, 4-6, 7-8.", w=60)
p("Il modulo LFO nella riga degli effetti contiene da 1 a 8 LFO (3 di default). Ognuno può modulare qualsiasi parametro "
  "della Sezione B: moduli Kick, Snare, Hat, Belt e Capsule, sequencer, Remix, Out, Delay, xFx, EQ, Filter. Le stesse "
  "regole valgono per gli LFO della Sezione A.")
table([["Controllo", "Funzione"],
       ["n/8, − / +", "Numero di LFO attivi. + aggiunge un LFO (fino a 8), − toglie l'ultimo. Un LFO tolto smette di "
        "modulare ma conserva le sue impostazioni: se lo aggiungi di nuovo torna com'era."],
       ["1-3 / 4-6 / 7-8", "Pagine del modulo compatto (compaiono con più di 3 LFO). La finestra grande ↗ mostra sempre "
        "tutti gli LFO attivi, con + Add LFO e − Remove last LFO."],
       ["LFO n (meter)", "Indicatore del valore attuale dell'LFO (colorato quando ha una destinazione)."],
       ["-- target --", "Clicca per scegliere la destinazione da un menu diviso per gruppi (per esempio Kick, Hat, Delay, "
        "xFx, Sequencer; nella Sezione A Channel 1–8 e Section A). -- (off) spegne l'LFO."],
       ["Forma", "Sine, Triangle, Saw Up, Saw Down, Square, S&amp;H (valore casuale tenuto per un ciclo), Random (casuale "
        "morbido)."],
       ["SYNC", "Acceso: la velocità è una divisione musicale (8 bar … 1/32, terzine T e puntati .), agganciata alla "
        "posizione del brano mentre suona. Spento: la velocità è RATE in millisecondi (durata di un ciclo, 10 ms … 30 s)."],
       ["Rate / divisione", "Con SYNC spento compare il knob Rate (ms); con SYNC acceso il menu della divisione."],
       ["Depth", "Quantità, da −100 a +100 %. 100 % muove il parametro di metà della sua escursione in su e in giù; i "
        "valori negativi invertono l'LFO."],
       ["Phase", "(finestra grande) Sposta l'inizio del ciclo, 0–360°."]], [32, 138])
bullets([
    "La modulazione si somma al valore del knob, che resta fermo dove l'hai messo.",
    "Più LFO sulla stessa destinazione si sommano.",
    "I parametri a scelta (menu, interruttori, numero del campione) cambiano a scatti: per esempio un LFO sul knob Wav "
    "sceglie un campione diverso a ogni colpo.",
    "Le impostazioni degli LFO sono automatizzabili, si salvano con il progetto e non cambiano quando carichi un preset.",
    "Il Morph della Sezione A non è modulabile (è un comando che riscrive i suoni).",
])

# ---------------------------------------------------------------------------------------------- 7
h1("7. Campioni e cartelle")
p("Ogni modulo sample della Sezione B legge una cartella: il menu e il knob Wav mostrano solo i file di quella cartella. "
  "Sono supportati WAV, AIFF, FLAC e OGG, mono o stereo, a qualsiasi frequenza di campionamento (fino a 30 secondi per "
  "file).")
table([["Cartella", "Contenuto"],
       ["Dualbox Kick / Snare / Hat / Belt / Capsule / Click", "Libreria di fabbrica: 48 kick puliti, 48 snare e clap "
        "morbidi, 32 hat (con i rispettivi aperti in open/), 46 Belt con solo bassi e accordi in Do, 66 Capsule con tom "
        "808/909, conga 808 e percussioni non metalliche, 12 click. Alcuni campioni vengono da librerie gratuite CC0 / "
        "Public Domain: crediti nel file SAMPLE_CREDITS.md dentro la cartella dei campioni. Quando una nuova versione "
        "porta una libreria aggiornata, i file di fabbrica vengono sostituiti automaticamente; i file aggiunti da te, "
        "le cartelle Legacy e le cartelle utente non vengono toccati."],
       ["Legacy Kick / Snare / Hat / Belt / Capsule", "Campioni importati con IMPORT LEGACY (vedi 7.1)."],
       ["Cartella utente", "Qualsiasi cartella scelta con l'icona cartella, separatamente per ogni modulo."]], [52, 118])
h2("7.1 Importare una libreria Legacy")
bullets([
    "Premi <b>IMPORT LEGACY</b> nell'intestazione della Sezione B e seleziona un file di ensemble (.ens) di tua proprietà "
    "che contiene mappe di campioni.",
    "DUALBOX estrae le mappe di campioni (Kick, Snare, Hat con i rispettivi aperti, Belt, Capsule) nelle cartelle Legacy. "
    "L'operazione richiede qualche secondo; l'avanzamento appare nell'intestazione.",
    "Al termine premi <b>LEG</b> nei moduli per usarli.",
    "I campioni importati restano solo sul tuo computer: DUALBOX non li contiene e non li distribuisce. Importa solo "
    "materiale che hai il diritto di usare.",
])
h2("7.2 Usare i tuoi campioni")
bullets([
    "Organizza i campioni in una cartella per tipo (es. Miei Kick), poi premi l'icona cartella nel modulo e selezionala.",
    "Per l'Hat, metti gli aperti in una sottocartella <b>open</b> con lo stesso nome del chiuso: verranno usati per la "
    "corsia O-HAT.",
    "Se aggiungi file a una cartella mentre il plugin è aperto, riseleziona la cartella (o premi DBX/LEG/l'icona "
    "cartella) per aggiornare l'elenco.",
    "Nota: i preset di genere usano sempre le cartelle di fabbrica; caricandone uno, i moduli tornano su DBX.",
    "Ogni cartella viene analizzata per l'intonazione (vedi 6.5); nella cartella viene creato il file nascosto "
    "<font face='DV-M'>.dualbox_pitch.txt</font>, che puoi cancellare senza problemi (verrà ricreato).",
])

# ---------------------------------------------------------------------------------------------- 8
h1("8. Preset di genere e Browser")
fig("win_B_browser.png", "Fig. 11 – Browser dei preset (Sezione B).", w=110)
p("Ogni sezione ha 128 preset: 16 per ognuno degli 8 generi Techno, Minimal, Microhouse, Rominimal, House, Tech House, "
  "Deep House, Breakbeat. Apri il browser con il pulsante BROWSER della sezione. Per ogni genere i primi 8 preset sono "
  "<b>SIMPLE</b> (8 groove essenziali tutti diversi: cassa in quattro, clap su 2 e 4, open hat in levare, shaker sui "
  "sedicesimi, hat a sedicesimi con accenti, percussioni 3+3+2, tom fra le casse, rullate di snare come fill, hat in "
  "levare con swing, micro-percussioni, rimshot e conga a bassa velocity fra i tempi, tom intonati, basso che rotola) e "
  "gli altri 8 <b>COMPLEX</b> (ghost note, poliritmi tra le corsie, variazioni tra le battute, campioni alternati, più "
  "effetti e Remix): l'etichetta è accanto al nome.")
table([["Genere", "Carattere dei kit"],
       ["Techno", "kick con coda rumble pulita, basso rumble, pad scuri, tom e conga ipnotici"],
       ["Minimal", "click &amp; cut, kick piccoli, contrabbasso e rhodes granulari, vinile"],
       ["Microhouse", "percussioni organiche, sub rotondi, voci processate, groove lunghi"],
       ["Rominimal", "kick tondi, rimshot, basso con portamento, swing marcato"],
       ["House", "909 caldi, clap, organo e rhodes, swing garage"],
       ["Tech House", "707/808/909, bassline, archi in stile Detroit, acid solo in 2 preset"],
       ["Deep House", "suoni polverosi, accordi dub, bassi funky, dettagli giocosi"],
       ["Breakbeat", "electro: 808 lunghi, snare 808 morbide, tom 808, bassi robotici"]], [32, 138])
table([["", "Sezione A", "Sezione B"],
       ["Cosa contiene un preset", "Kit di 8 drum patch con i nomi dei canali, un pattern (fino a 64 step), swing e "
        "fill rate", "Snapshot completo: campioni, parametri delle voci, effetti, lunghezze, swing, remix e i pattern A–D"],
       ["Dove viene caricato", "Nello slot di pattern corrente (A–L)", "Sostituisce kit e pattern A–D"]], [40, 65, 65])
bullets([
    "Scegli un genere a sinistra, seleziona un preset e premi <b>LOAD</b> (o fai doppio clic).",
    "<b>Kit (sounds)</b>: carica i suoni. <b>Pattern(s)</b>: carica i pattern. Disattivane uno per caricare solo l'altro "
    "(es. solo il groove su un kit tuo).",
    "<b>Set tempo</b>: imposta anche il tempo del clock interno al BPM suggerito dal preset (indicato accanto al nome).",
    "Nei preset della Sezione B tutti gli effetti partono al minimo: GAIN di Delay e xFx a −60 dB, Trash ed EQ AMP a 0, "
    "nessuna saturazione sulle voci. Alza tu gli effetti che ti servono.",
    "Caricare un preset non cambia lo stato acceso/spento dei moduli, la catena di pattern e le impostazioni SCALE.",
])
tip("I preset Rominimal e Microhouse (118–130 BPM) usano groove lunghi da 32 o 64 step, pochissima snare, percussioni "
    "legnose fuori griglia, basso che rotola con portamento, swing marcato, delay dub e poliritmi tra le corsie (Capsule "
    "di lunghezza diversa dalla sequenza principale).")

# ---------------------------------------------------------------------------------------------- 9
h1("9. Clock, sincronizzazione e MIDI")
h2("9.1 Clock")
bullets([
    "Quando la DAW è in riproduzione, entrambe le sezioni seguono la posizione e il tempo dell'host (indicatore HOST SYNC).",
    "A trasporto fermo o nella versione Standalone premi <b>RUN</b> (o <b>CLK</b> nella Sezione B): il clock interno parte "
    "dal primo step al Tempo impostato.",
    "I cambi di pattern vengono messi in coda e avvengono alla fine del pattern in corso.",
])
h2("9.2 Suonare le sezioni via MIDI (pulsanti MIDI)")
p("Nell'intestazione di ogni sezione c'è il pulsante <b>MIDI</b>. La tastiera MIDI (o un controller a pad) suona una "
  "sezione alla volta: premendo MIDI nella Sezione A la tastiera suona la A, premendolo nella B suona la B (il pulsante "
  "acceso indica la sezione attiva). Con SEQ acceso il sequencer continua a suonare e le note MIDI si aggiungono ai "
  "pattern; spegni SEQ per suonare la sezione solo dalla tastiera.")
table([["Nota", "Sezione A (MIDI A)", "Sezione B (MIDI B)"],
       ["36 (C1)", "Canale 1", "Kick"], ["37", "Canale 2", "Snare"], ["38", "Canale 3", "Hat chiuso"],
       ["39", "Canale 4", "Hat aperto"], ["40", "Canale 5", "Belt"], ["41", "Canale 6", "Capsule"],
       ["42", "Canale 7", "(Kick, la fila riparte)"], ["43 (G1)", "Canale 8", "(Snare)"]], [30, 70, 70])
p("La fila di suoni si ripete lungo la tastiera (ogni 8 tasti per A, ogni 6 per B), così una griglia di 16 pad li copre "
  "tutti due volte. Le note sotto il C1 sono ignorate. La velocity MIDI è la velocity del colpo.")

# ---------------------------------------------------------------------------------------------- 10
h1("10. Flussi di lavoro consigliati")
h2("10.1 Costruire un groove ibrido in 5 minuti")
bullets([
    "Carica un preset Rominimal nella Sezione B e un preset Minimal nella Sezione A (BROWSER, Kit + Pattern).",
    "Porta il crossfader A/B Mix verso B (circa 65 %): il groove a campioni guida, la Sezione A aggiunge click e dettagli "
    "sintetici.",
    "Nella Sezione A metti in mute (M) il Kick per non sovrapporre due casse.",
    "Nella Sezione B imposta Remix ≈ 40 e Rate 8: ogni 8 battute il groove viene rielaborato.",
    "Registra un movimento sul pad di XFX (modo reflex o vowel, Len 4 bars) e lascialo in PLAY per un'evoluzione continua.",
    "Usa il CUTOFF master in HP per le transizioni.",
])
h2("10.2 Melodie con Belt")
bullets(["Nel modulo Belt imposta VEL = tune e alza Port.",
         "Nella corsia BELT disegna barre sopra e sotto la linea centrale: ogni altezza è una nota diversa (±12 semitoni).",
         "Attiva SCALE per restare sempre nella tonalità scelta."])
h2("10.3 Variazioni con la corsia MOD")
bullets(["Imposta MOD = cut sull'Hat e disegna una rampa nella corsia MOD: gli hat si aprono progressivamente.",
         "Imposta MOD = wav sullo Snare: ogni step può suonare un campione diverso della cartella.",
         "Dai alla corsia Mod una lunghezza diversa (es. 12 o 24) per variazioni che non si ripetono a ogni battuta."])

# ---------------------------------------------------------------------------------------------- 11
h1("11. Risoluzione dei problemi")
table([["Problema", "Soluzione"],
       ["Il plugin non compare nella DAW", "Esegui una nuova scansione dei plugin; su Logic usa Plug-in Manager → Reset "
        "&amp; Rescan; riavvia il Mac se necessario. Verifica che l'installer abbia completato (la prima volta autorizza "
        "in Privacy e sicurezza)."],
       ["Non si sente nulla", "Controlla che la DAW sia in Play o che RUN sia acceso; verifica A/B Mix, Level A/B, "
        "Master e che il CUTOFF non sia chiuso (LP a 20 kHz = aperto)."],
       ["I moduli B mostrano '(empty folder)'", "La cartella attiva è vuota: premi DBX, oppure importa una libreria "
        "Legacy prima di premere LEG, oppure scegli una cartella con campioni."],
       ["IMPORT LEGACY dà errore", "Il file scelto non contiene mappe di campioni leggibili da DUALBOX."],
       ["Gli effetti coprono il kit", "Abbassa GAIN di Delay/xFx o FX Mix; ricarica il preset per tornare ai livelli "
        "bilanciati (Delay a −60 dB)."],
       ["Non sento il delay", "È voluto: il GAIN del Delay parte da −60 dB. Alzalo (es. −18 … −10 dB)."],
       ["Il pattern non cambia subito", "È normale: il cambio avviene alla fine del pattern in corso. Da fermo è "
        "immediato."],
       ["Un campione di Belt/Capsule risulta 'no pitch'", "Il suono non ha un'altezza chiara (rumore, accordo, "
        "percussione inarmonica): rinomina il file includendo la tonalità (es. 'pad chord Db') e accendi NAME, oppure "
        "intonalo in MANUAL con il knob della finestra ♪."],
       ["Il pitch rilevato sembra sbagliato di un'ottava", "Può accadere con suoni molto ricchi di armoniche: correggi "
        "con il knob nella finestra ♪ (modalità MANUAL)."],
       ["Il movimento XY non viene riprodotto", "Serve REC acceso mentre trascini il pad (scritta REC rossa), poi PLAY. "
        "Assicurati che il trasporto o RUN siano attivi."]], [50, 120])

# ---------------------------------------------------------------------------------------------- 12
h1("12. Specifiche tecniche")
table([["Voce", "Valore"],
       ["Formati", "VST3 (macOS, Windows, Linux), Audio Unit (macOS), Standalone"],
       ["macOS", "10.13 o successivo, binari universali Intel x86_64 + Apple Silicon arm64"],
       ["Uscita", "Stereo"],
       ["Voci", "Sezione A: 8 voci sintetiche (+ layer CLICK); Sezione B: 5 voci a campioni"],
       ["Pattern", "A: 12 × fino a 64 step; B: 4 × fino a 64 step + catena di 8 slot"],
       ["LFO", "da 1 a 8 per sezione (default 3); 7 forme; SYNC 8 bar … 1/32 (anche terzine e puntati) o RATE 10 ms … "
        "30 s; Depth ±100 %"],
       ["Nudge", "per traccia, −50 … +50 ms (passo 0,1 ms), senza latenza aggiunta"],
       ["Rilevamento pitch", "YIN, 30 Hz – 2 kHz, cache per cartella; opzionale dal nome del file (NAME)"],
       ["Scale", "35 scale, Root condivisa da Belt e Capsule"],
       ["Preset", "128 per sezione (8 generi × 16)"],
       ["Campioni per modulo", "max 128 per cartella"],
       ["Libreria di fabbrica", "252 campioni + 32 hat aperti, 44,1 kHz / 16 bit"],
       ["Latenza", "Nessuna"]], [45, 125])
p("<font size=7.5 color='#6b6b6b'>DUALBOX è un prodotto TMFU audiovibe, costruito con il framework JUCE 8. I campioni di "
  "fabbrica sono sintetizzati appositamente o provengono da librerie gratuite CC0 / Public Domain (crediti in "
  "SAMPLE_CREDITS.md). L'interfaccia usa il font Urbanist (SIL Open Font License 1.1). I nomi di DAW e di sistemi "
  "operativi citati in questo manuale appartengono ai rispettivi proprietari e sono usati solo per indicare la "
  "compatibilità.</font>")

doc.multiBuild(story)
print("written", OUT)
