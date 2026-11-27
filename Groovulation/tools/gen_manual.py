#!/usr/bin/env python3
"""Groovulation user manual (Italian, PDF, Inter font).

Usage: python3 tools/gen_manual.py <version> <screenshot folder>
The screenshot folder is the output folder of GroovulationTest (gui_main.png, gui_logo.png).
Writes docs/Groovulation_Manuale_v<version>.pdf
"""
import sys, os
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                Image, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from PIL import Image as PILImage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
VERSION = sys.argv[1] if len(sys.argv) > 1 else "1.2.0"
SHOTS = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build", "test_out")
OUT = os.path.join(ROOT, "docs", f"Groovulation_Manuale_v{VERSION}.pdf")
TMP = os.path.join(SHOTS, "_manual")
os.makedirs(TMP, exist_ok=True)

pdfmetrics.registerFont(TTFont("Inter", os.path.join(ROOT, "resources/fonts/Inter-Regular.ttf")))
pdfmetrics.registerFont(TTFont("Inter-SemiBold", os.path.join(ROOT, "resources/fonts/Inter-SemiBold.ttf")))
pdfmetrics.registerFontFamily("Inter", normal="Inter", bold="Inter-SemiBold", italic="Inter", boldItalic="Inter-SemiBold")

ACCENT = colors.HexColor("#c6402c")
DARK = colors.HexColor("#45413a")
CREAM = colors.HexColor("#efe9db")
GRID = colors.HexColor("#b9b09b")

body = ParagraphStyle("body", fontName="Inter", fontSize=10, leading=14.2, spaceAfter=5)
small = ParagraphStyle("small", parent=body, fontSize=9, leading=12.2, spaceAfter=0)
cell = ParagraphStyle("cell", parent=body, fontSize=9, leading=12, spaceAfter=0)
cellb = ParagraphStyle("cellb", parent=cell, fontName="Inter-SemiBold")
h1 = ParagraphStyle("h1", fontName="Inter-SemiBold", fontSize=16, leading=20, textColor=DARK, spaceBefore=10, spaceAfter=8)
h2 = ParagraphStyle("h2", fontName="Inter-SemiBold", fontSize=11.5, leading=15, textColor=ACCENT, spaceBefore=8, spaceAfter=4)
caption = ParagraphStyle("cap", parent=small, textColor=colors.HexColor("#6b6559"), alignment=1, spaceBefore=3, spaceAfter=8)
bullet = ParagraphStyle("bul", parent=body, leftIndent=12, bulletIndent=2, spaceAfter=2)
note = ParagraphStyle("note", parent=body, fontSize=9.5, leading=13.5, textColor=DARK)
title = ParagraphStyle("title", fontName="Inter-SemiBold", fontSize=26, leading=32, textColor=DARK, alignment=1)
sub = ParagraphStyle("sub", fontName="Inter", fontSize=13, leading=18, textColor=colors.HexColor("#6b6559"), alignment=1)

W = A4[0] - 36 * mm


def P(t, s=body):
    return Paragraph(t, s)


def bullets(items):
    return [Paragraph(i, bullet, bulletText="•") for i in items]


def table(rows, widths, header=True):
    data = [[Paragraph(c, cellb if (header and r == 0) or (k == 0) else cell) for k, c in enumerate(row)] for r, row in enumerate(rows)]
    t = Table(data, colWidths=[w * W for w in widths], repeatRows=1 if header else 0)
    st = [("GRID", (0, 0), (-1, -1), 0.4, GRID), ("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5),
          ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]
    if header:
        st += [("BACKGROUND", (0, 0), (-1, 0), CREAM)]
    t.setStyle(TableStyle(st))
    return t


def boxed(t):
    tb = Table([[Paragraph(t, note)]], colWidths=[W])
    tb.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), CREAM), ("LINEBEFORE", (0, 0), (0, -1), 3, ACCENT),
                            ("LEFTPADDING", (0, 0), (-1, -1), 9), ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 7)]))
    return tb


def crop(name, box, out):
    src = os.path.join(SHOTS, name)
    if not os.path.exists(src):
        return None
    im = PILImage.open(src)
    if box:
        im = im.crop(box)
    p = os.path.join(TMP, out)
    im.save(p)
    return p


def img(path, width, cap=None):
    if path is None:
        return []
    im = PILImage.open(path)
    h = width * im.height / im.width
    r = [Image(path, width=width, height=h)]
    if cap:
        r.append(Paragraph(cap, caption))
    return r


main_png = crop("gui_main.png", None, "main.png")
strips_png = crop("gui_main.png", (8, 84, 1832, 364), "strips.png")
editor_png = crop("gui_main.png", (8, 376, 1424, 992), "editor.png")
master_png = crop("gui_main.png", (1432, 376, 1832, 992), "master.png")
logo_png = crop("gui_logo.png", None, "logo.png")


def on_page(c, doc):
    if doc.page == 1:
        return
    c.saveState()
    c.setFont("Inter", 8)
    c.setFillColor(colors.HexColor("#6b6559"))
    c.drawString(18 * mm, A4[1] - 12 * mm, f"GROOVULATION  Manuale d'uso - versione {VERSION}")
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.setStrokeColor(GRID)
    c.line(18 * mm, A4[1] - 14 * mm, A4[0] - 18 * mm, A4[1] - 14 * mm)
    c.restoreState()


doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=20 * mm, bottomMargin=16 * mm,
                      title=f"Groovulation - Manuale d'uso {VERSION}", author="TMFU audiovibe")
doc.addPageTemplates([PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=on_page)])

S = []
# ---------------------------------------------------------------- cover
S += [Spacer(1, 30 * mm)]
S += img(logo_png, W * 0.8)
S += [Spacer(1, 8 * mm), P("Loop &amp; sample mixer - AU / VST3 per macOS (Intel e Apple Silicon)", sub), Spacer(1, 4 * mm),
      P("Manuale d'uso", title), P(f"versione {VERSION}", sub), Spacer(1, 10 * mm)]
S += img(main_png, W)
S += [Spacer(1, 6 * mm), P("TMFU audiovibe", sub), PageBreak()]

# ---------------------------------------------------------------- index
chapters = ["Che cos'è Groovulation", "Installazione e primo avvio", "L'interfaccia in breve", "Barra superiore (UNDO / REDO)",
            "Modalità MANUAL, SCALE e RAW", "I 12 moduli e come vengono riempiti", "Le strip dei moduli", "Il modulo CUSTOM",
            "La libreria di loop", "L'editor del modulo", "Il display: griglia, LENGTH, SLICE, LOOP MARKERS, VOL.A, zoom",
            "I pannelli del modulo (Gain, ADSR, Pitch, Cutoff, Dirt, Width, Chorus, Delay, Reverb, LFO)", "La sezione MASTER",
            "REMIX: Beat Repeat", "REMIX: Tape Machine e Vinyl", "PRINT, EXPORT e snapshot", "Uscite separate (multi-out)",
            "Scorciatoie e consigli", "Novità della versione 1.2.0", "Limiti noti e domande frequenti"]
S += [P("Indice", h1)] + [P(f"{i + 1}. {c}") for i, c in enumerate(chapters)] + [PageBreak()]

n = iter(range(1, 100))
def H(t):
    return P(f"{next(n)}. {t}", h1)

# 1
S += [H("Che cos'è Groovulation"),
      P("Groovulation è un mixer di loop e campioni. Dodici moduli suonano contemporaneamente loop presi da cartelle di default o "
        "personali, sempre a tempo con il progetto. Ogni loop si può tagliare, mettere in loop tra due marcatori, filtrare con tre "
        "filtri indipendenti, saturare, arricchire con chorus e allargamento stereo, riverberare, modulare con tre LFO e, se vuoi, "
        "intonare automaticamente in una scala. Il master aggiunge swing globale, filtri, Beat Repeat e due \"deck\" da suonare con "
        "il mouse: un registratore a bobine e un giradischi."),
      P("Il plugin analizza ogni loop che trova: tipo di materiale (cassa, rullante, basso, pad...), tempo, numero di battute, "
        "tonalità, nota fondamentale e \"carattere\" del suono. Così sa in quale modulo caricarlo, come intonarlo e quali loop si somigliano."),
      P("Come si usa, in tre passi", h2)]
S += bullets(["Apri Groovulation su una traccia strumento della DAW e premi Play: ogni modulo parte con un loop casuale del suo tipo.",
              "Cambia i loop con le frecce, con RND (casuale) o con SIM (il più simile), e regola i moduli nell'editor in basso.",
              "Quando ti piace, premi PRINT per salvare i loop elaborati, oppure EXPORT per gli originali, e trascinali nella DAW."])
# 2
S += [H("Installazione e primo avvio"), P("Installazione", h2)]
S += bullets([f"Apri <b>Groovulation-{VERSION}-macOS.pkg</b>. Installa il plugin AU (Logic, GarageBand, Ableton Live) e VST3 "
              "(Ableton Live, Cubase, Reaper, Bitwig...), entrambi universali Intel / Apple Silicon.",
              "Il pacchetto non è firmato con un certificato Apple Developer. La prima volta usa clic destro → Apri, oppure "
              "Impostazioni di Sistema → Privacy e sicurezza → Apri comunque.",
              "Logic potrebbe richiedere un riavvio per vedere il nuovo AU; in Ableton Live usa Rescan dei plugin."])
S += [P("Le cartelle di Groovulation", h2), P("L'installer crea <b>/Users/Shared/Groovulation</b>, condivisa da tutti gli utenti del Mac:"),
      table([["Cartella", "Contenuto"],
             ["Loops", "I 55 loop di fabbrica (House, Deep House, Micro House, Minimal, Techno), divisi in cartelle per modulo."],
             ["PRINT", "I loop salvati con PRINT ed EXPORT, in una sottocartella con il nome dello snapshot."],
             ["Snapshots", "Gli snapshot salvati dal MENU (file .groovulation)."],
             ["Database", "L'analisi dei loop (loopdb.json) e il registro leggibile dei loop simili (similar_loops.json)."]], [0.2, 0.8]),
      P("Primo avvio", h2),
      P("Al primo avvio il plugin installa i loop di fabbrica e li analizza (qualche secondo); poi carica un loop casuale in ogni "
        "modulo. Lo stato dell'analisi compare nel display LCD in alto (SCAN ... %). Le analisi vengono salvate: le volte successive "
        "solo i file nuovi o modificati vengono analizzati."),
      boxed("Groovulation è uno strumento: mettilo su una traccia MIDI / strumento. Suona quando la DAW è in Play; con la DAW ferma "
            "(o nell'app standalone) puoi usare il pulsante RUN con il suo BPM interno."), PageBreak()]
# 3
S += [H("L'interfaccia in breve")] + img(main_png, W, "La finestra principale") + [
      table([["Zona", "Che cosa contiene"],
             ["Barra superiore", "Logo, display LCD (BPM, play, modalità, stato), UNDO / REDO, MENU, RANDOM ALL, SCALE con tonalità e scala, RAW, RUN e BPM interno, PRINT, EXPORT."],
             ["Strip dei moduli", "Dodici strip in cinque sezioni: DRUMS (rosso), CUSTOM (grigio), BASS (ambra), MELODIC (azzurro), VOX / FX (oliva). Clicca una strip per aprirla nell'editor."],
             ["Editor del modulo", "In basso a sinistra: tutte le regolazioni del modulo selezionato, con il display grande."],
             ["Master", "A destra: volume master, GLOBAL SWING, CUTOFF (HP, BP, LP), Beat Repeat, Tape Machine e Vinyl."]], [0.22, 0.78]),
      Spacer(1, 6),
      boxed("La finestra si ridimensiona dall'angolo in basso a destra mantenendo le proporzioni. Passa il mouse su un controllo per "
            "vederne la descrizione. Su ogni manopola: doppio clic = valore di default, doppio clic sul valore = scrivi un numero, "
            "SHIFT mentre trascini = regolazione fine.")]
# 4
S += [H("Barra superiore"),
      table([["Controllo", "Funzione"],
             ["Display LCD", "Riga 1: BPM del progetto, ▶ / ■ (play / stop), modalità (MANUAL, SCALE con tonalità e scala, RAW). Riga 2: numero di loop nel database e nome dello snapshot; durante l'analisi o il PRINT mostra l'avanzamento."],
             ["UNDO / REDO", "Annulla e ripristina le modifiche ai parametri (manopole, pulsanti, selettori, marcatori). Ogni gesto con il mouse è un passo. Scorciatoie: Cmd+Z (undo), Shift+Cmd+Z o Cmd+Y (redo). Caricare uno snapshot azzera la cronologia."],
             ["MENU", "Cartelle di loop (aggiungi, rimuovi, riscansiona, ricostruisci il database, mostra le cartelle e il registro dei loop simili), snapshot (salva, carica, rinomina) e la cartella PRINT."],
             ["RANDOM ALL", "Carica un loop casuale del tipo giusto in ogni modulo (tranne CUSTOM)."],
             ["SCALE + tonalità + scala", "Attiva l'intonazione automatica (capitolo 5): Major, Minor, Dorian, Mixolydian, Lydian, Phrygian, Locrian, Whole Tone, diminuite, Blues, Pentatoniche, Harmonic / Melodic Minor, scale giapponesi, Messiaen, Chromatic e altre."],
             ["RAW", "I loop suonano al tempo e all'intonazione originali (capitolo 5)."],
             ["RUN + BPM", "Clock interno per suonare con la DAW ferma o nell'app standalone."],
             ["PRINT / EXPORT", "Salvano i loop dei moduli (capitolo 16)."]], [0.22, 0.78])]
# 5
S += [H("Modalità MANUAL, SCALE e RAW"),
      table([["Modalità", "Tempo", "Intonazione"],
             ["MANUAL (default: SCALE e RAW spenti)", "al BPM del progetto", "originale del loop, più il PITCH del modulo"],
             ["SCALE (pulsante SCALE acceso)", "al BPM del progetto", "BASS, MELODIC 01-03, VOCALS (e CUSTOM se intonato) vengono trasposti nella scala scelta"],
             ["RAW (pulsante RAW acceso)", "originale del loop", "originale (+ PITCH del modulo)"]], [0.34, 0.24, 0.42]),
      P("Come funziona SCALE", h2),
      P("Per ogni loop il plugin conosce tonalità e note (analisi audio o nome del file, vedi TYPE nel capitolo 10). Con SCALE attivo "
        "sceglie lo spostamento in semitoni che porta il più possibile delle note del loop dentro la scala, preferendo mettere la tonica "
        "del loop sulla tonica della scala e lo spostamento più piccolo. Corregge anche l'accordatura fine del loop (in cent)."),
      boxed("Esempi con scala C Major: un basso in E maggiore scende di 4 semitoni; un loop di accordi in D maggiore scende di 2; un loop "
            "in A minore resta dov'è (ha già le note di C maggiore)."),
      P("Con SCALE attivo il PITCH del modulo funziona ancora: il risultato viene portato sulla nota della scala più vicina. Il valore "
        "realmente suonato compare nelle strip e nell'editor (PLAY +2.00 st). I moduli DRUMS e FX non vengono intonati."), PageBreak()]
# 6
S += [H("I 12 moduli e come vengono riempiti")] + img(strips_png, W, "Le dodici strip, divise in cinque sezioni") + [
      table([["Modulo", "Che cosa carica"],
             ["1 KICK", "Solo i file con kick, bd, bass drum o cassa nel nome (anche attaccati: \"KickLoop01\", \"BD_Loop_02\"), purché il nome NON contenga \"no kick\", \"no_kick\", \"nokick\", \"without kick\" o \"kickless\"."],
             ["2 SNARE", "Rullanti, clap, rim, snap."],
             ["3 BOTTOM", "Tom e percussioni medio-basse senza una cassa forte, e i file con \"no kick\" nel nome."],
             ["4 TOP", "Hi-hat, ride, piatti, shaker (\"top\" loop)."],
             ["5 PERCUSSION", "Conga, bongo, tamburello, cowbell, clave, legni..."],
             ["6 CUSTOM", "Vuoto: lo riempi tu con il floppy o trascinando un file (capitolo 8)."],
             ["7 BASS", "Linee di basso, sub, 808, reese."],
             ["8 MELODIC 01", "Accordi e stab (keys, piano, rhodes, organo, chitarra, fiati)."],
             ["9 MELODIC 02", "Pad, atmosfere, drone, archi, texture."],
             ["10 MELODIC 03", "Synth: lead, arpeggi, pluck, sequenze, campane, riff."],
             ["11 VOCALS", "Vocal, vox, voice, acapella, choir, chant, adlib, vocoder."],
             ["12 FX", "FX, sfx, riser, uplifter, downlifter, sweep, impact, whoosh, transizioni."]], [0.2, 0.8]),
      P("Regole generali", h2)]
S += bullets(["Il tipo si legge prima dal nome del file, poi dalle cartelle che lo contengono.",
              "VOCALS e FX vengono riconosciuti per primi: un loop vocale o un effetto non finisce mai in un altro modulo.",
              "Se il nome non dice nulla, il tipo lo decide un classificatore audio che impara dai loop con nomi chiari (timbro, spettro, dinamica, ritmo, intonazione).",
              "Un loop di batteria con una cassa forte ma senza \"kick\" nel nome (es. \"Drum Loop 01\") non viene caricato: non può andare in KICK (non ha il nome) né in BOTTOM / TOP / PERCUSSION (ha la cassa).",
              "Vengono esclusi i file più lunghi di 32 battute al tempo del progetto e quelli più corti di una battuta."])
S += [boxed("Se vuoi che un tuo loop finisca in un modulo preciso, il modo più sicuro è il nome: \"kick\", \"snare\", \"top\", \"perc\", "
            "\"bass\", \"chords\", \"pad\", \"synth\", \"vocal\", \"fx\"... oppure una cartella con quel nome.")]
# 7
S += [H("Le strip dei moduli"),
      table([["Controllo", "Funzione"],
             ["Intestazione", "Numero e nome del modulo; clic = apri il modulo nell'editor (bordo colorato = modulo selezionato)."],
             ["Display", "Waveform del loop con griglia e posizione di riproduzione; mostra anche SLICE, LOOP MARKERS e VOL.A."],
             ["Nome e info", "Nome del loop, BPM, battute, tonalità e, se SCALE o PITCH lo spostano, i semitoni suonati."],
             ["&lt; &gt;", "LOOP SELECT: loop precedente / successivo dello stesso tipo. Tutte le altre regolazioni restano."],
             ["RND", "Loop casuale dello stesso tipo."],
             ["SIM", "SIMILAR SELECT: il loop più simile (stesso tipo, sonorità e carattere simili). Ogni pressione va al successivo in ordine di somiglianza."],
             ["GAIN / PITCH", "Volume (default −6 dB) e intonazione del modulo; il meter a destra mostra il livello."],
             ["ON / S", "Accende o spegne il modulo; S = solo."]], [0.22, 0.78])]
# 8
S += [H("Il modulo CUSTOM"),
      P("CUSTOM parte vuoto e accetta qualsiasi file audio (WAV, AIFF, FLAC, MP3, OGG, M4A, CAF), da qualsiasi cartella:")]
S += bullets(["clicca l'icona del floppy nella strip o nell'editor e scegli il file;",
              "oppure trascina il file dal Finder sulla strip CUSTOM (si illumina quando puoi rilasciarlo);",
              "CLEAR svuota il modulo."])
S += [P("Il file viene analizzato come gli altri (tempo, tonalità...). Segue SCALE solo se è materiale intonato. Il percorso viene "
        "salvato con la sessione o con lo snapshot.")]
# 9
S += [H("La libreria di loop"), P("Aggiungere le tue cartelle", h2)]
S += bullets(["MENU → Add loop folder to scan...: scegli una cartella; viene scansionata con tutte le sottocartelle.",
              "MENU → Remove folder: toglie una cartella dalla libreria (i file non vengono toccati).",
              "MENU → Rescan folders: cerca file nuovi o modificati. Rebuild database: rianalizza tutto da capo.",
              "Le cartelle sono condivise da tutte le istanze del plugin e da tutti i progetti."])
S += [P("Che cosa viene analizzato", h2),
      table([["Dato", "Come"],
             ["Tipo", "Nome del file e delle cartelle; analisi audio (cassa nella banda 30–100 Hz, timbro, spettro, ritmo) e classificatore che impara dai loop con nome."],
             ["Tempo e battute", "BPM dal nome (\"124bpm\", \"_128_\") oppure dalla durata, verificata con il tempo stimato dai transienti."],
             ["Tonalità", "Cromagramma ad alta risoluzione con correzione dell'accordatura e profili tonali; per i bassi anche la nota fondamentale."],
             ["Fondamentale", "Altezza della nota (YIN), mostrata come nota e cent."],
             ["Loop simili", "Impronta di timbro, spettro, dinamica, ritmo e armonia; i loop di ogni tipo sono raggruppati in \"famiglie\" (fam N nel menu dei loop)."]], [0.22, 0.78]),
      P("Il registro dei loop simili", h2),
      P("MENU → Show 'similar loops' registry apre similar_loops.json: per ogni modulo, le famiglie di loop simili con BPM, battute, "
        "tonalità e fondamentale. SIM nelle strip e nell'editor usa queste somiglianze."), PageBreak()]
# 10
S += [H("L'editor del modulo")] + img(editor_png, W, "L'editor del modulo selezionato") + [
      P("Intestazione", h2),
      P("Numero, nome del modulo e menu LOOP con tutti i loop del tipo (con la famiglia di somiglianza). Sotto, la riga informativa: "
        "tipo (da nome o da audio), BPM, battute, tonalità rilevata dall'audio e dal nome, fondamentale, accordatura, famiglia e "
        "semitoni suonati (PLAY)."),
      P("Barra degli strumenti", h2),
      table([["Controllo", "Funzione"],
             ["GRID", "Divisione della griglia del display: 1/1, 1/2, 1/4, 1/8, 1/12, 1/16, 1/24, 1/32, 1/48, 1/64 (1/12, 1/24, 1/48 = terzine). È anche l'unità di LENGTH. Cambiarla mentre suona non fa click."],
             ["LENGTH − + ORIG", "Lunghezza del loop in step della griglia. ORIG = lunghezza originale."],
             ["SLICE", "Attiva i marcatori S (inizio) ed E (fine) sul display: suona solo la parte tra i due, il resto è silenziato, la lunghezza resta quella del loop. Spegnendo SLICE si spegne automaticamente anche LOOP."],
             ["LOOP", "Con SLICE attivo: la parte tra S ed E si ripete di continuo (niente silenzio fuori dai marcatori), con una dissolvenza di 5 ms nel punto di ripetizione. ADSR e VOL.A seguono il giro della slice."],
             ["LOOP MARKERS", "Attiva i marcatori LS (loop start) ed LE (loop end): la riproduzione si ripete tra i due, sia in modo normale sia in SLICE (vedi capitolo 11)."],
             ["MS / GRID", "Come si muovono i marcatori SLICE e LOOP: MS = al millisecondo, GRID = sulla griglia."],
             ["FADE 0.5ms", "Fade-in e fade-out di 0,5 ms ai bordi della slice (niente click)."],
             ["VOL.A / RESET VOL", "Automazione di volume disegnata sul display (clip gain); RESET VOL la riporta piatta."],
             ["TYPE: AUDIO / FILE NAME", "Da dove prendere la tonalità per SCALE: dall'analisi audio o dal nome del file (\"bass Gmin\" = Sol minore, \"pad D\" = Re maggiore, \"F#m\", \"La minore\"...). Usa FILE NAME per i campioni difficili da analizzare."],
             ["LOOP SELECT", "&lt; &gt; RND SIMILAR, come nelle strip."]], [0.24, 0.76])]
# 11
S += [H("Il display: griglia, LENGTH, SLICE, LOOP MARKERS, VOL.A, zoom"), P("Griglia e poliritmie", h2),
      P("Le linee della griglia sono trasparenti: più marcate sulle battute, medie sui quarti, leggere sulle suddivisioni. La LENGTH si "
        "esprime in step della griglia: 7 × 1/16 su un loop in 4/4 crea una poliritmia, perché il ciclo del modulo dura 7 sedicesimi e "
        "si sfasa rispetto agli altri moduli. Una linea tratteggiata indica dove il loop ricomincia dentro la lunghezza scelta."),
      P("SLICE", h2)]
S += bullets(["Trascina i marcatori S ed E (bandierine rosse in alto); clic sul display = sposta lì il marcatore più vicino.",
              "In modalità MS: passi di 1 ms; con SHIFT movimento 10 volte più lento e passi di 0,1 ms. Le posizioni (in ms dall'inizio) e la durata della slice sono scritte sul display.",
              "Frecce ← → (dopo aver cliccato sul display): spostano l'ultimo marcatore toccato di 1 ms; con SHIFT di 0,1 ms; con ALT o CMD di 10 ms.",
              "In modalità GRID i marcatori si agganciano alla griglia (SHIFT = libero).",
              "LOOP acceso: la selezione tra S ed E si ripete in loop (crossfade di 5 ms, niente click); spento: fuori dai marcatori c'è silenzio.",
              "Puoi spostare S ed E anche mentre il plugin suona: il gate è ammorbidito e, in LOOP, la nuova posizione entra con una dissolvenza incrociata di 20 ms tra due voci. Niente click né pop."])
S += [P("LOOP MARKERS (LS / LE)", h2)]
S += bullets(["Con LOOP MARKERS acceso compaiono due marcatori blu con le bandierine in basso: LS (inizio del loop) ed LE (fine).",
              "La riproduzione gira tra LS ed LE. Funziona in modo normale e in SLICE: in SLICE i marcatori S / E fanno da gate dentro il giro LS–LE.",
              "Se sono accesi sia SLICE + LOOP sia LOOP MARKERS, il giro lo decide SLICE LOOP (S / E).",
              "Con SLICE e LOOP MARKERS entrambi accesi: la metà alta del display muove S / E, la metà bassa muove LS / LE.",
              "Si muovono come i marcatori SLICE (MS o GRID, SHIFT fine, frecce ← →) e anche loro si spostano mentre suona senza click."])
S += [P("VOL.A (automazione di volume)", h2)]
S += bullets(["Con VOL.A acceso: clic sul display = nuovo punto; trascina per spostarlo; doppio clic per cancellarlo.",
              "La linea tratteggiata è 0 dB; in alto fino a +6 dB, in basso fino al silenzio."])
S += [P("Zoom e risoluzione", h2),
      P("La waveform è calcolata direttamente dall'audio: minimo e massimo per ogni pixel e, zoomando a fondo, la linea reale del segnale fino ai singoli campioni.")]
S += bullets(["Rotella o pinch sul trackpad: zoom attorno al puntatore.",
              "SHIFT + rotella, rotella orizzontale o barra in basso: scorre la vista.",
              "− + FIT in basso a destra: zoom indietro, avanti, tutto il loop.",
              "Marcatori e punti VOL.A seguono lo zoom: zoomando li posizioni con precisione."])
# 12
S += [PageBreak(), H("I pannelli del modulo"),
      table([["Pannello", "Controlli"],
             ["GAIN / PAN", "Volume del modulo (default −6 dB, doppio clic = −6 dB) e panorama."],
             ["ADSR", "Inviluppo applicato a ogni giro del loop (o della slice, o del giro LS–LE): A attacco, D decadimento, S sustain, R release prima della fine."],
             ["PITCH", "PITCH ±32 semitoni con risoluzione di un millesimo di semitono, senza scalini udibili, grazie al time-stretch di alta qualità usato da tutti i moduli (sui drums con una finestra più corta per attacchi netti). Il knob TEMPO è stato tolto: i loop seguono sempre il tempo del progetto."],
             ["CUTOFF", "Tre filtri indipendenti in serie, nell'ordine HP → BP → LP. Ognuno ha il suo pulsante di accensione (HP, BP, LP), la sua frequenza (FREQ, 20 Hz – 20 kHz) e la sua risonanza (RES). Accendere o spegnere un filtro è una dissolvenza di 5 ms. HP e LP si aprono e chiudono in modo speculare: HP aperto tutto a sinistra e chiude girando a destra, LP aperto tutto a destra e chiude girando a sinistra, con la stessa scala (stesse ottave per lo stesso movimento)."],
             ["DIRT", "Saturazione morbida (\"gentle\"): lineare sui segnali deboli, arrotonda solo i picchi, livello compensato."],
             ["WIDTH / MONO", "WIDTH: ampiezza stereo mid/side, 0 % = mono, 100 % = invariato, fino a 400 %. MONO somma il modulo in mono. Agisce alla fine della catena, prima di GAIN / PAN."],
             ["CHORUS", "Lo stesso chorus di CHROMA5YNTH. ON lo accende; modalità Chorus I / II (chorus BBD a due linee, scuro e leggermente saturo: lento / più veloce e profondo), Dimension 1–4 (linee in controfase, spaziale e senza vibrato percepibile, intensità crescente), Custom (RATE e DEPTH liberi). MIX = dry / wet."],
             ["DELAY", "TIME a tempo (da 1/64 a 8 battute, puntati e terzine), FEEDBACK, MIX, PING-PONG."],
             ["REVERB", "Riverbero algoritmico denso e pulito: SIZE, DECAY (0,2–20 s), DAMP, PREDELAY, MIX."],
             ["LFO 1, 2, 3", "RATE a tempo, DEPTH (bipolare), forma (Sine, Triangle, Saw Up, Saw Down, Square, S&amp;H) e destinazione."]], [0.18, 0.82]),
      P("Ordine della catena", h2),
      P("Loop → SLICE / ADSR / VOL.A → CUTOFF (HP → BP → LP) → DIRT → CHORUS → DELAY → REVERB → WIDTH / MONO → GAIN / PAN."),
      P("Destinazioni degli LFO", h2),
      P("Gain, Pan, Pitch (±12 st), Tempo (fino a ×2 / ÷2), Cutoff (±4 ottave, su tutti e tre i filtri), Resonance (tutti e tre i "
        "filtri), Dirt, Delay Mix, Delay FB, Reverb Mix, Slice Start, Slice End e Loop."),
      boxed("LFO → Loop: il modulo precarica in memoria fino a 8 loop simili a quello attuale (al massimo 8 battute) e, a ogni passo "
            "della GRID, l'LFO sceglie quale suonare, con una dissolvenza incrociata. Non legge dal disco durante la riproduzione, quindi "
            "niente glitch. Con una forma S&amp;H ottieni cambi casuali.")]
# 13
S += [PageBreak(), H("La sezione MASTER")]
mt = table([["Pannello", "Controlli"],
            ["MASTER", "Volume generale (senza manopola di risonanza). Il bus ha 6 dB di headroom e un soft clipper di sicurezza."],
            ["GLOBAL SWING", "ON accende lo swing su tutti i moduli; AMOUNT lo regola in tempo reale (la quantità scorre in ~150 ms, niente salti); 1/8 o 1/16 sceglie la griglia del groove: il pulsante selezionato è chiaro, l'altro grigio scuro."],
            ["CUTOFF", "Come nei moduli: HP → BP → LP indipendenti, ognuno con pulsante di accensione, FREQ e RES propria. HP e LP speculari. Frequenza e risonanza vengono livellate campione per campione (~3 ms): il suono segue la manopola senza ritardi anche quando la giri velocemente."],
            ["BEAT REPEAT", "Vedi capitolo 14."],
            ["TAPE &amp; VINYL", "TAPE MACHINE e VINYL aprono i due deck (capitolo 15). HAND = la \"mano sul deck\" per controller MIDI e automazione."]], [0.25, 0.75])
S += img(master_png, W * 0.42, "La sezione MASTER") + [mt]
# 14
S += [H("REMIX: Beat Repeat"),
      P("Cattura un pezzetto di audio a intervalli regolari e lo ripete."),
      table([["Controllo", "Funzione"],
             ["ON", "Accende il Beat Repeat."],
             ["Mix / Insert", "Mix: le ripetizioni si sommano al suono. Insert: il suono originale tace mentre ripete. Il pulsante selezionato è chiaro, l'altro grigio scuro. Tra originale e ripetizioni c'è sempre una dissolvenza di 2 ms: niente click."],
             ["INTERVAL", "Ogni quanto parte una ripetizione (da 1/32 a 4 battute)."],
             ["OFFSET", "Spostamento del punto di partenza dentro l'intervallo, in sedicesimi."],
             ["GRID", "Lunghezza di ogni ripetizione (1/32 … 1 battuta, terzine comprese)."],
             ["CHANCE", "Probabilità che la ripetizione parta."],
             ["LENGTH", "Durata totale della ripetizione, in sedicesimi."],
             ["VARIATION", "Cambia a caso la griglia durante la ripetizione."],
             ["DECAY", "Ogni ripetizione più bassa della precedente."],
             ["PITCH / P.DECAY", "Abbassa l'intonazione delle ripetizioni (fino a −48 semitoni), e progressivamente con P.DECAY."],
             ["VOLUME", "Volume delle ripetizioni."]], [0.22, 0.78])]
# 15
S += [PageBreak(), H("REMIX: Tape Machine e Vinyl"),
      P("Due finestre grandi da suonare con il mouse. Tutto quello che vedi agisce sull'audio dell'uscita principale: la grafica legge "
        "la velocità reale dal motore audio. Aprire la finestra attiva il deck, chiuderla lo spegne."), P("Tape Machine", h2)]
S += bullets(["Bobina destra: trascina in alto = tiri il nastro più veloce; in basso = rallenta, si ferma, va al contrario.",
              "Bobina sinistra: scuotila per trascinare e far ondeggiare il nastro.",
              "Clic tra le bobine: un'azione casuale.",
              "Pulsanti: STOP (tape stop), START (ripartenza), WOBBLE (oscillazione), PULL (strappo del nastro dalla bobina: rewind lungo con l'intonazione che scende lentamente), REWIND (2 s con fade-out di 60 ms), RANDOM.",
              "Il nastro ha un leggero wow &amp; flutter; quando lasci, il suono torna in dissolvenza a quello normale."])
S += [P("Vinyl", h2)]
S += bullets(["Afferra il disco e muovilo: la velocità segue la tua mano (scratch, anche all'indietro); tenendolo fermo, il suono si ferma.",
              "START / STOP: come un giradischi vero. STOP frena il piatto e resta fermo finché non premi di nuovo; START lo riporta a velocità.",
              "33 / 45: cambia i giri. 45 alza velocità e intonazione di tutto (+5,2 semitoni) come su un deck vero.",
              "Fader pitch (±8 %): trascinalo per cambiare insieme velocità e intonazione di tutto; vicino al centro si aggancia a 0.",
              "Pulsanti: SPINBACK (2 s con fade-out di 60 ms), SCRATCH, WOBBLE, RANDOM, RESYNC (rimette i loop a tempo con la DAW dopo aver usato fader o 33/45)."])
S += [P("<b>HAND</b> (sezione master): la \"mano sul deck\" per controller MIDI e automazioni. Al centro non fa nulla; a sinistra rallenta, "
        "ferma e manda all'indietro; a destra spinge fino a 3×.")]
# 16
S += [H("PRINT, EXPORT e snapshot"),
      table([["Pulsante", "Che cosa salva", "Dove e con che nome"],
             ["PRINT", "Ogni loop come esce dal modulo: intonazione, slice, loop markers, ADSR, filtri, effetti, LFO e swing compresi. Un giro di loop, con le code di delay e riverbero già \"avvolte\": il file si ripete senza stacchi.", "PRINT/&lt;snapshot&gt;/&lt;Modulo&gt; - &lt;snapshot&gt;.wav (es. Kick - Serata.wav)"],
             ["EXPORT", "I file originali dei loop caricati, non processati.", "PRINT/&lt;snapshot&gt;/&lt;Modulo&gt; - &lt;snapshot&gt; (original).&lt;estensione&gt;"]], [0.14, 0.5, 0.36]),
      Spacer(1, 4),
      P("Dopo PRINT o EXPORT compare un pannello con l'elenco dei file e un riquadro DRAG ME INTO YOUR DAW: trascinalo in Ableton Live o "
        "Logic Pro e la DAW crea una traccia audio per ogni file. SHOW IN FINDER apre la cartella."),
      P("Snapshot", h2),
      P("Uno snapshot salva tutto: loop di ogni modulo, parametri, automazioni VOL.A, impostazioni del master. MENU → Save snapshot as... "
        "lo salva in Snapshots, Load snapshot lo ricarica, Rename cambia il nome. Il nome dello snapshot è quello usato da PRINT ed "
        "EXPORT per cartella e file. La sessione della DAW salva comunque tutto in automatico."),
      boxed("Un plugin non può leggere il nome del progetto della DAW: per questo cartelle e file prendono il nome dello snapshot. Dai un "
            "nome allo snapshot prima di stampare.")]
# 17
S += [H("Uscite separate (multi-out)"),
      P("Oltre all'uscita principale, Groovulation ha 12 uscite stereo, una per modulo (Kick ... FX). Quando abiliti l'uscita di un modulo "
        "nella DAW, quel modulo esce lì e non più nel mix principale, così puoi registrarlo o trattarlo su una traccia separata.")]
S += bullets(["Logic Pro: inserisci Groovulation come strumento Multi-Output, poi usa il pulsante + del mixer per creare le tracce aux.",
              "Ableton Live: crea tracce audio con input dalla traccia di Groovulation e scegli l'uscita del modulo (Kick, Snare...)."])
# 18
S += [PageBreak(), H("Scorciatoie e consigli"),
      table([["Dove", "Gesto", "Effetto"],
             ["Ovunque", "Cmd+Z / Shift+Cmd+Z (o Cmd+Y)", "UNDO / REDO"],
             ["Manopole", "doppio clic", "valore di default (GAIN = −6 dB)"],
             ["Manopole", "doppio clic sul valore", "scrivi il valore (Invio per confermare)"],
             ["Manopole", "SHIFT + trascina", "regolazione fine"],
             ["Display", "rotella / pinch", "zoom attorno al puntatore"],
             ["Display", "SHIFT + rotella, barra in basso", "scorri la vista"],
             ["Display", "− + FIT", "zoom indietro / avanti / tutto"],
             ["SLICE / LOOP MARKERS (MS)", "trascina, SHIFT + trascina", "passi di 1 ms / 0,1 ms"],
             ["SLICE / LOOP MARKERS (MS)", "← →, SHIFT, ALT/CMD", "±1 ms, ±0,1 ms, ±10 ms sull'ultimo marcatore"],
             ["SLICE + LOOP MARKERS", "clic in alto / in basso nel display", "sposta S / E oppure LS / LE"],
             ["VOL.A", "clic / trascina / doppio clic", "aggiungi / sposta / cancella un punto"],
             ["Strip CUSTOM", "trascina un file", "carica il file"],
             ["Strip", "clic", "apri il modulo nell'editor"]], [0.27, 0.36, 0.37]),
      P("Consigli", h2)]
S += bullets(["Per poliritmie: GRID 1/16 e LENGTH dispari (5, 7, 9...) su TOP o PERCUSSION contro KICK in 4/4.",
              "Per cambi di loop a tempo: LFO → Loop con RATE 1 battuta e forma S&amp;H.",
              "Per isolare una frase: LOOP MARKERS su una battuta del loop, poi SLICE per tagliarne i colpi.",
              "Per un suono più largo e morbido: CHORUS in Dimension 2–3 e WIDTH 120–150 %; per controllare i bassi in mono: MONO sul modulo BASS.",
              "Per stampare un loop già \"finito\" da usare altrove: regola il modulo, metti un nome allo snapshot e premi PRINT.",
              "Se un tuo campione è difficile da analizzare, mettigli la tonalità nel nome e usa TYPE → FILE NAME."])
# 19
S += [H("Novità della versione 1.2.0"),
      table([["Area", "Novità"],
             ["Generale", "UNDO / REDO per tutti i parametri (pulsanti nella barra superiore, Cmd+Z / Shift+Cmd+Z)."],
             ["Master", "Tolta la manopola RES accanto al volume master. CUTOFF: HP → BP → LP indipendenti, ognuno con accensione e risonanza propria; HP e LP speculari; il suono segue le manopole senza ritardi anche nei movimenti veloci."],
             ["Moduli", "CUTOFF con HP → BP → LP indipendenti (accensione + RES per ciascuno), HP e LP speculari. Nuovi CHORUS, WIDTH e MONO. Tolto TEMPO dal pannello PITCH."],
             ["SLICE", "Spostare S / E o cambiare GRID mentre suona non fa più click o pop. Spegnendo SLICE si spegne anche LOOP."],
             ["LOOP MARKERS", "Nuovi marcatori LS / LE per far girare il loop tra due punti, in modo normale e in SLICE."],
             ["Pulsanti", "1/16 e 1/8 di GLOBAL SWING e Mix / Insert del Beat Repeat: il pulsante selezionato è chiaro, l'altro grigio scuro."],
             ["Beat Repeat", "Tolto NO TRIPLETS."]], [0.2, 0.8])]
# 20
S += [H("Limiti noti e domande frequenti")]
faq = [("Il plugin non suona.", "Premi Play nella DAW (o RUN con la DAW ferma). Controlla che i moduli siano ON e che non sia attivo un SOLO."),
       ("Un filtro non fa nulla.", "Dalla 1.2.0 ogni filtro (HP, BP, LP) ha il suo pulsante di accensione: accendilo. Le sessioni salvate con versioni precedenti partono con i filtri spenti."),
       ("Un mio loop è nel modulo sbagliato.", "Rinominalo con la parola del modulo (kick, snare, top, bass, pad...) e fai MENU → Rescan folders. Per KICK vale solo il nome."),
       ("Dopo un aggiornamento vedo ancora i loop di prima.", "Riapri il plugin: alla riapertura ogni modulo riceve solo loop del suo tipo."),
       ("La tonalità rilevata è sbagliata.", "Su materiale ambiguo (solo due note, accordi sospesi) può capitare. Metti la tonalità nel nome e usa TYPE → FILE NAME."),
       ("UNDO non annulla il cambio di loop.", "UNDO / REDO riguardano i parametri (manopole, pulsanti, selettori, marcatori). La scelta dei loop e i punti VOL.A non fanno parte della cronologia."),
       ("I loop vanno fuori tempo dopo il fader del giradischi.", "È voluto, come su un giradischi vero: premi RESYNC o chiudi la finestra VINYL."),
       ("DIRT, REVERB, Beat Repeat e il groove sono copie di prodotti commerciali?", "No: sono implementazioni originali."),
       ("Può creare tracce nella DAW o leggere il nome del progetto?", "No, nessun plugin AU/VST3 può farlo: usa il trascinamento dopo PRINT/EXPORT, oppure le uscite separate, e il nome dello snapshot.")]
for q, a in faq:
    S += [KeepTogether([P(f"<b>{q}</b>", body), P(a)])]

doc.build(S)
print("wrote", OUT)
