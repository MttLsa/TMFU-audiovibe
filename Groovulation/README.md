# GROOVULATION — loop mixer AU / VST3 (macOS Intel + Apple Silicon)

Mixer di loop e campioni a 12 moduli, nello stile di Twinbox con i colori di un MPC 60 vintage.

📖 **Manuale d'uso (PDF):** [docs/Groovulation_Manuale_v1.2.0.pdf](docs/Groovulation_Manuale_v1.2.0.pdf)

| Sezione | Moduli |
|---|---|
| **DRUMS** | 1 Kick (file con kick / bd / bass drum / cassa nel nome), 2 Snare/Clap, 3 Bottom (tom e suoni medio-bassi senza cassa forte, o "no kick"), 4 Top, 5 Percussion |
| **CUSTOM** | 6 Custom: vuoto di default, ci carichi qualsiasi file col floppy o trascinandolo sul modulo |
| **BASS** | 7 Bass |
| **MELODIC** | 8 Melodic 01 (chords & stabs), 9 Melodic 02 (pad), 10 Melodic 03 (synth) |
| **VOX / FX** | 11 Vocals, 12 FX |

## Libreria e riconoscimento
- Cartelle: `/Users/Shared/Groovulation/` → `Loops` (55 loop di fabbrica: House, Deep House, Micro House, Minimal, Techno),
  `PRINT`, `Snapshots`, `Database`. Le cartelle personali si aggiungono da **MENU → Add loop folder to scan**.
- **Tipo del loop**:
  - **KICK**: solo i file con "kick", "bd", "bass drum" o "cassa" nel nome (anche attaccati, es. "KickLoop01"),
    purché il nome non contenga "no kick" / "no_kick" / "nokick" / "without kick" / "kickless".
  - **BOTTOM**: i file "no kick", i tom e i loop di batteria senza una cassa forte. La cassa la misura l'analisi
    audio, con i transienti nella banda 30–100 Hz sincronizzati con i colpi.
  - Un loop di batteria con cassa forte ma senza "kick" nel nome (es. "Drum Loop 01") non viene caricato.
  - **VOCALS / FX**: per nome (vocal, vox, voice, acapella, choir… / fx, sfx, riser, sweep, impact, whoosh…).
    Questi loop non finiscono mai in altri moduli.
  - Se il nome non dice nulla, il tipo lo decide un classificatore audio (k-NN) che impara dai loop con il nome
    giusto.
- Vengono esclusi i loop più lunghi di 32 battute al tempo del progetto e quelli più corti di una battuta.
- **Loop simili**: descrittore di timbro (MFCC), spettro, dinamica, ritmo e armonia; distanza pesata e famiglie
  k-means per tipo. Il registro leggibile si trova in `Database/similar_loops.json`.
- **Tonalità**: cromagramma ad alta risoluzione con somma armonica e compensazione dell'accordatura, profili
  Krumhansl/Temperley; per i bassi anche la fondamentale (YIN). In alternativa (TYPE → FILE NAME) la tonalità si
  legge dal nome del file ("Gmin", "D", "F#m", "La minore", ...).

## Modalità
- **MANUAL** (default): i loop suonano alla loro intonazione originale, al tempo del progetto.
- **SCALE**: Bass, Melodic, Vocals (e Custom se intonato) vengono trasposti nella scala scelta (stessa lista del
  device Scale di Ableton). PITCH funziona ancora: il risultato va sulla nota più vicina della scala.
- **RAW**: tempo e intonazione originali; PITCH resta manuale.

## Motore audio
- **Time-stretch / pitch-shift polifonico ad alta qualità** (Signalsmith Stretch, classe "Complex Pro") per tutti
  i moduli. Sui DRUMS usa una finestra di analisi più corta (~50 ms), così gli attacchi restano netti.
- Lo swing è un warp continuo (sin²) e la quantità di swing scorre in ~150 ms: muovere il knob non crea salti né
  scalini.
- I loop vengono ricampionati alla frequenza del progetto come segnali ciclici (nessun buco al giro del loop).
- I cambi di loop (LOOP SELECT, SIMILAR, LFO → Loop) vanno in dissolvenza incrociata tra due voci.

## Ogni modulo
Display LCD con waveform ad alta risoluzione e zoom (rotella / pinch = zoom sul puntatore, shift+rotella o barra = scorri, − + FIT; zoomando a fondo si vedono i singoli campioni), griglia 1/1 … 1/64 con terzine fino a 1/48, lunghezza in step di griglia per le poliritmie), GAIN (−6 dB di default),
PAN, ADSR, VOL.A (automazione di volume disegnata sul display), PITCH ±32 semitoni (risoluzione 0,001),
SLICE con fade di 0,5 ms: i marcatori S / E si muovono al millisecondo (SNAP MS: shift = 0,1 ms, frecce ← → = ±1 ms, shift ±0,1 ms, alt/cmd ±10 ms) oppure sulla griglia (SNAP GRID); con LOOP la parte tra S ed E si ripete di continuo (crossfade di 5 ms).
Spostare S / E o cambiare GRID mentre suona non fa click (gate ammorbidito e crossfade di 20 ms tra due voci); spegnendo SLICE si spegne anche LOOP.
**LOOP MARKERS** (LS / LE, bandierine in basso nel display): la riproduzione si ripete tra i due marker, in modo normale e in SLICE.
**CUTOFF**: tre filtri indipendenti HP → BP → LP, ognuno con ON e la sua RES; HP e LP si chiudono e si aprono in modo speculare (scala logaritmica 20 Hz–20 kHz),
**CHORUS** (stesso DSP di CHROMA5YNTH: Chorus I / II, Dimension 1–4, Custom con Rate / Depth, Mix), **WIDTH** (0 % = mono, 100 % = invariato, fino a 400 %) e **MONO**,
DIRT, DELAY, REVERB, **3 LFO** (destinazioni: gain, pan, pitch, **tempo**, cutoff, resonance, dirt, delay,
reverb, slice e **Loop**), LOOP SELECT, SIMILAR, TYPE.

**LFO → Loop**: il modulo precarica in RAM fino a 8 loop simili (massimo 8 battute) e a ogni passo della griglia l'LFO
sceglie quale suonare, con crossfade. Non legge dal disco durante la riproduzione, quindi niente glitch.

## Master
- **GLOBAL SWING**: ON + quantità (1/8 o 1/16), in tempo reale.
- **CUTOFF**: HP → BP → LP indipendenti, ognuno con ON e RES; HP e LP speculari; il suono segue i knob senza ritardi.
- **UNDO / REDO** (barra in alto, Cmd+Z / Shift+Cmd+Z): un passo per ogni gesto su un controllo.
- **Livello**: −6 dB di headroom sul bus e un soft clipper di sicurezza.
- **REMIX**:
  - **Beat Repeat**: Interval, Offset, Grid, Chance, Length, Variation, Decay, Pitch, Pitch Decay, Volume,
    modalità Mix / Insert (il pulsante selezionato è chiaro), crossfade di 2 ms.
  - **TAPE MACHINE**: trascini le bobine; STOP, START, WOBBLE, PULL (strappo del nastro: rewind lungo con pitch che
    scende lentamente), REWIND (2 s, fade-out di 60 ms).
  - **VINYL**: START/STOP, 33/45 e pitch fader ±8 % come un giradischi vero (velocità e intonazione insieme);
    scratch, SPINBACK (2 s, fade-out di 60 ms), RESYNC.
  - **HAND**: la "mano sul deck" per controller MIDI e automazione (centro = niente, sinistra = rallenta / ferma /
    indietro, destra = spinge fino a 3×).

## PRINT / EXPORT
- **PRINT**: ogni loop come esce dal modulo (processato) → `PRINT/<snapshot>/<Modulo> - <snapshot>.wav`.
- **EXPORT**: i loop **originali**, non processati → `PRINT/<snapshot>/<Modulo> - <snapshot> (original).<ext>`.
- In entrambi i casi un pannello permette di trascinare tutti i file in Ableton Live o Logic (una traccia per file).
  In alternativa ci sono 12 uscite separate (multi-out).

## Multi-out
- 12 uscite stereo separate (Kick … FX) oltre a quella principale, attive di default.
- Per ogni modulo **OUTPUT** (pannello Gain / Pan): **MAIN** = mix principale, **SEP** = solo la sua uscita separata,
  **BOTH** = entrambe. Le uscite separate escono dopo gli effetti del modulo, senza cutoff / Beat Repeat / deck / volume del master.

## Limiti
- Saturn 2, RAUM e i device di Ableton sono proprietari: DIRT, REVERB, Beat Repeat e Groove sono implementazioni
  originali che seguono lo stesso comportamento, ma non sono identiche.
- Un plugin non può creare tracce nella DAW né leggere il nome del progetto: PRINT usa il nome dello snapshot.

## Build
```
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
./packaging/macos/make_pkg.sh 1.2.0        # installer .pkg (macOS)
cmake -B build -DGROOV_BUILD_TESTS=ON && cmake --build build --target GroovulationTest   # test offline
```
Su GitHub l'installer viene costruito automaticamente (workflow `groovulation`).
