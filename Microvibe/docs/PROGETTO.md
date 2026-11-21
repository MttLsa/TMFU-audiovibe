# MICROVIBE — progetto (bozza da approvare prima della build)

Plugin glitch / FX / generatore per minimal e microhouse.
Produttore **TMFU audiovibe**, JUCE 8, C++17, VST3 + AU + Standalone, `.pkg` universale macOS come 5ynth (ex SynthZilla / Twinsynth).

| Campo `juce_add_plugin` | Valore proposto |
|---|---|
| PRODUCT_NAME | `Microvibe` (nome provvisorio) |
| COMPANY_NAME | `TMFU audiovibe` |
| PLUGIN_MANUFACTURER_CODE | `Tmfu` |
| PLUGIN_CODE | `Mcvb` |
| BUNDLE_ID | `com.tmfuaudiovibe.microvibe` |
| Tipo | **Music effect** (AU `aumf`): processa l'audio in ingresso, riceve MIDI e ha generatori interni → funziona sia come insert sia da solo come "strumento" |
| Validazione CI | `auval -v aumf Mcvb Tmfu` |

---

## 1. Idea guida

Un unico motore costruito intorno a un **clock interno sincronizzato alla DAW** che scandisce *tre famiglie* di processi:

1. **SORGENTI** — l'audio in ingresso + tre generatori interni (texture, percussioni organiche, Shepard).
2. **GLITCH ENGINE** — buffer di cattura che viene ripetuto, tagliato, invertito, rallentato, granulato a ritmo.
3. **CATENA FX** — 8 slot riordinabili (tape, vinile, wobble, delay, reverb, filtri, ecc.).

Sopra tutto, un **sistema di modulazione** in cui *ogni* parametro automatizzabile è una destinazione e ogni modulatore può modulare gli altri (intermodulazione).

```
 IN ──┬─────────────────────────────────────────────┐
      │                                             ▼
 TEXTURE GEN ─┐                              ┌─► GLITCH ENGINE ─► FX CHAIN (8 slot) ─► WIDTH/OUT ─► NORM/limiter
 PERC GEN  ───┼─► SOURCE MIXER (4 canali) ───┤
 SHEPARD GEN ─┘                              └─► (dry/send)
                     ▲            ▲                 ▲                   ▲
                     └────── MOD MATRIX (32 slot) + 8 LFO + 4 ENV + 4 RND + 2 ENV FOLLOWER + 4 macro ──────┘
                                       CLOCK / POLYRHYTHM ENGINE (4 corsie)
```

---

## 2. Sorgenti

### 2.1 Input
Ingresso stereo con trim, gate e **sidechain trigger** (envelope follower → può far scattare glitch e percussioni sui transienti della traccia).

### 2.2 TEXTURE — generatore di sfondi
Pensato per i "letti" e i rumori ambientali tipici della microhouse.
- **Granulare** su buffer: sorgente = ingresso registrato in continuo (freeze) oppure noise colorato/campione interno. Densità 1–200 grani/s, durata 5–500 ms, spray di posizione, pitch ±24 st con **quantizzazione microtonale** (vedi §6), finestra Hann/Tukey/rettangolare.
- **Noise layer**: white/pink/brown/crackle → banco di **risonatori** (8 bande accordate sulla scala corrente) → texture "tonale" dal rumore.
- **Drone**: fino a 8 oscillatori sinusoidali leggeri con detune microtonale e drift lento.
- **Chord = Scale** *(1.1)*: le note del drone **e** del letto risonante si scelgono nella pagina TUNING (§6.1),
  con una progressione di fino a 4 accordi.
- Freeze, Spread stereo, Tilt EQ, livello.

### 2.3 PERC — percussioni organiche minimal
Sintesi **modale** (banco di risonatori, approccio Chromaphone/Elements/RipplerX) + eccitatori, nessun campione.
- 6 voci indipendenti (ognuna con propria corsia ritmica, §5).
- **Eccitatore**: click, burst di noise, impulso filtrato, "mallet" (impulso arrotondato), oppure **un frammento dell'ingresso** (microsampling alla Akufen).
- **Corpo**: modelli Wood, Glass, Metal, Skin (membrana), Tube, Stone, Plastic — fino a 16 modi per voce, con Material (smorzamento HF), Inharmonicity, Decay, Position, Tune (in scala).
- **808** *(1.1)*: Kick 808, Tom 808, Snare 808, Clap 808, Closed Hat 808, Open Hat 808, sintetizzati (nessun campione):
  kick e tom = sinusoide con sweep di pitch e click (Kick in G1 e Tom in G2 a Tune 0, seguono la scala);
  snare = due sinusoidi 180/330 Hz + rumore filtrato (*snappy*); clap = rumore passa-banda con 4 colpi ravvicinati e coda;
  hat = sei oscillatori a onda quadra 205–800 Hz in passa-banda + passa-alto. Il **closed hat interrompe l'open hat** (choke) come sull'808.
  Per gli 808 Material = tono / snappy / brillantezza, Inharmonic = profondità dello sweep; l'eccitatore non si usa.
- **Per ogni voce** *(1.1)*: Level (gain), Tune, Decay e **filtro HP / BP / LP** con Cutoff e Resonance.
- Velocity → brillantezza/decay, pan per voce, humanize (timing + velocity).
- Preset di voce: rimshot di legno, click di vetro, "blip" metallico, tock di bottiglia, shaker di sabbia, conga tappata.

### 2.4 SHEPARD — tono e filtro "infinito"
- **Shepard/Risset generator**: 6–10 parziali a ottava (o a intervallo libero: 3ª, 5ª, tritono, intervalli microtonali) con inviluppo spettrale gaussiano, velocità ±, direzione su/giù/pendolo, sync al tempo (un ciclo = N battute).
- **Shepard FX** sull'audio (come "Endless Series"): modalità **Filter** (banco di passa-banda che salgono/scendono all'infinito), **Phaser**, **Flanger** e **Notch inverso** (DAFx-15: picchi sostituiti da notch). Utilissimo per riser/faller infiniti e tensione nelle build.

---

## 3. Glitch engine

Buffer circolare di 8 s stereo che registra continuamente la sorgente. Una **pattern lane** a 1–64 step (o le corsie poliritmiche) decide quando e quale evento glitch scatta; ogni step ha probabilità, ratchet e un valore di parametro.

| Modulo | Cosa fa |
|---|---|
| **Repeat / Stutter** | ripete una fetta (1/128…1 bar, terzine, puntati, frazioni libere per poliritmia), con *decay* di volume, pitch-ramp e accorciamento progressivo (roll "a scalare") |
| **Slice / Shuffle** | divide la battuta in N fette e le riordina (random, reverse, palindromo, Euclideo) |
| **Reverse** | fetta al contrario, anche parziale |
| **Tape stop / start** | rallentamento a zero / ripartenza con curva regolabile |
| **Gate / Chop** | gate ritmico con forma di inviluppo per step |
| **Granular freeze** | congela l'istante corrente e lo granula |
| **Bitcrush / Downsample** | per step, con anti-alias opzionale |
| **Skip / Dropout** | silenzi casuali tipo CD che salta (anche "buffer underrun" digitale) |
| **Pitch shift** | ±24 st per step, anche microtonale |

- **Fill/Density**: un solo knob che aumenta la quantità di eventi (macro ideale per le performance).
- Crossfade anti-click di 1–10 ms su ogni taglio.
- Risposta al MIDI: le note C1–B1 attivano direttamente i moduli (performance alla Effectrix / Glitch2).

---

## 4. Catena FX — 8 slot riordinabili (drag & drop)

Ogni slot sceglie uno di questi moduli, ha Mix e Bypass, e tutti i suoi parametri sono destinazioni di modulazione.

### 4.1 TAPE MACHINE
- Saturazione a isteresi: modello **Jiles–Atherton** semplificato (rif. ChowTape) con clipper di protezione prima; modalità *Eco* con waveshaper razionale per CPU bassa.
- Drive, Bias (sottopolarizzazione = zona morta), Speed (7,5/15/30 ips → risposta in frequenza e head bump), Hiss, Compressione, Dropout.
- Wow/flutter propri (o legati al modulo Wobble).

### 4.2 VINYL SIMULATOR
(Da allineare a Groovulation: mi servono i suoi sorgenti per renderlo identico.)
- **Dust** (impulsi casuali Poisson filtrati), **Crackle** (scoppiettio denso), **Scratch** (click grossi periodici legati al giro: 33/45/78 rpm), **Rumble** (LF del motore), **Hum** 50/60 Hz.
- **Year**: filtri d'epoca (dal 1930 a oggi), **Wear** (perdita di HF + distorsione da solco consumato), **Warp** (wow lento a frequenza di rotazione), mono-izzazione dei bassi.
- **Spin down / spin up** (anche come evento glitch).

### 4.3 WOBBLE (stile RC-20)
Stessa architettura del modulo Wobble dell'RC-20:
- Due LFO sinusoidali: **Wow** 0,1–4 Hz e **Flutter** 6–20 Hz, con **slider di bilanciamento** Wow↔Flutter e un unico **Amount** (profondità), applicati come modulazione di una linea di ritardo frazionaria (interpolazione Hermite / Lagrange 3° ordine).
- **Flux**: aggiunge irregolarità (random lento) alla velocità e alla profondità — risolve il difetto "troppo LFO" segnalato dagli utenti.
- Stereo: Link / Spread di fase L-R.

### 4.4 DELAY
Modalità Digital / Tape / Ping-pong / **Multitap poliritmico** (4 tap con divisioni indipendenti, es. 3:4:5:7), feedback con filtri, saturazione e wobble nel loop, **pitch nel feedback** (anche microtonale → spirali Shepard), freeze, ducking.

### 4.5 REVERB
FDN a 8 linee (stile Dattorro/Feedback Delay Network): Size, Decay, Pre-delay (sync), Damping, Modulation, Diffusion, **Shimmer** (pitch nel loop, intervalli in scala), **Freeze**, **Gated** e **Reverse** (per gli "swell" minimal), ducking.

### 4.6 Altri moduli
- **Filter**: SVF LP/HP/BP/Notch/Comb/Formant, 12/24 dB, drive.
- **Resonator**: banco modale applicato all'audio (accorda qualsiasi suono alla scala).
- **Ring/Freq shifter**: ring mod e frequency shifter (barberpole).
- **Bitcrusher / Sample-rate**.
- **Chorus/Ensemble**, **Phaser**.
- **Compressor / Transient shaper**.
- **Stereo**: width, mid/side, Haas, auto-pan.

---

## 5. Clock, polimetro e poliritmia

- **4 corsie ritmiche** indipendenti che possono pilotare: glitch engine, le 6 voci PERC, la granulazione TEXTURE, gli LFO (reset), i tap del delay.
- Per corsia: lunghezza 1–64 step (**polimetro**: 16 contro 13 contro 7…), divisione libera (**poliritmia**: 3:2, 5:4, 7:8, tuplet arbitrari), **Euclideo** (passi, pulsazioni, rotazione), swing indipendente, probabilità per step, ratchet 1–8, condizioni (1:2, 3:4, "solo al primo giro", "se l'altra corsia ha suonato").
- Modalità **Free** (Hz, non sincronizzata) per derive e phasing alla Steve Reich.
- **Pattern** A–H con catena (avanza di un pattern a ogni battuta, da quello scelto fino a *Chain End*).
- **Stop della DAW** *(1.1)*: quando il trasporto si ferma, in circa 30 ms si zittiscono texture (letto, drone, grani), Shepard,
  percussioni, glitch e corsie (anche quelle in modalità libera o comandate da transienti/MIDI), insieme al rumore di superficie
  del vinile e al fruscio del nastro. **Solo le code di delay e riverbero si spengono da sole, in modo naturale**: a trasporto fermo
  il feedback del delay è limitato a 0,9 e i freeze di delay e riverbero vengono rilasciati, così anche le code finiscono.
  Le note MIDI suonate a mano (glitch C1–G#1, percussioni C2–F2) restano attive. Nella versione Standalone (senza trasporto) il plugin suona sempre.

---

## 6. Microtonalità

- Scale **Scala `.scl` + `.kbm`** caricabili; libreria interna: 12-TET, 19/24/31/53-EDO, giusta intonazione 5-limit e 7-limit, Bohlen-Pierce, pelog/slendro, maqam (quarti di tono), scala armonica, Partch 43.
- Client **MTS-ESP** (libreria ODDSound, licenza permissiva) → il plugin segue l'accordatura globale della DAW se c'è un master.
- La scala corrente quantizza: pitch di PERC, risonatori, drone, grani, stutter-pitch, shimmer, pitch del delay e intervallo del Shepard. Knob **Snap** (0 = continuo, 100 % = quantizzato).

### 6.1 Note del drone e progressione di accordi *(1.1)*
Visibile nella pagina TUNING solo quando TEXTURE → Chord = **Scale**.
- **4 accordi** (snapshot): per ognuno si accendono/spengono i gradi della scala corrente (nomi delle note per le scale a 12 semitoni,
  numero del grado e cent per le altre; fino a 64 gradi). Le note accordano drone e letto risonante. Nessuna nota = triade minore automatica.
- **Progression On**, **Chords** (1–4 accordi usati), **Order** (Forward, Reverse, Ping-Pong, Random).
- **Tempo**: *Chord Unit* sincronizzata (da 8 battute a 1/32, terzine, puntati, quintine, settimine) oppure libera in secondi;
  **Durata 1–4** = quante unità dura ogni accordo (1–16).
- **Chord Glide** (0–4 s, glissando tra un accordo e l'altro) e **Drone Voicing** (Close, Open, Wide).
- La progressione segue la posizione della DAW; a trasporto fermo resta sull'ultimo accordo. Gli accordi sono salvati con il progetto e nei preset.

---

## 7. Modulazione

### 7.1 Sorgenti
| Sorgente | Quantità | Note |
|---|---|---|
| **LFO** | 8 | sine/tri/saw/square/S&H/smooth random/**forma disegnabile** (multi-punto con curve); sync o Hz; fase, simmetria, smoothing; one-shot/loop; reset dalle corsie ritmiche |
| **Envelope** | 4 | ADSR/AHD con curve, triggerabili da MIDI, corsie o transienti |
| **Random** | 4 | S&H, drift (Perlin), Brown, quantizzato in scala |
| **Envelope follower** | 2 | dall'ingresso o dal sidechain |
| **Step modulator** | 4 | le corsie ritmiche emettono anche valori (sequencer di parametri) |
| **Macro** | 4 | knob grandi in primo piano, assegnabili |
| **MIDI** | — | velocity, nota, mod wheel, aftertouch, CC learn |

### 7.2 Matrice
- **32 slot**: Sorgente × **Via** (secondo moltiplicatore, es. LFO1 × Macro2) → Destinazione, Amount bipolare, curva (lineare/esp/log/S), Smoothing.
- **Ogni parametro automatizzabile è una destinazione**, compresi i parametri dei modulatori stessi (rate, depth, forma, fase) → **intermodulazione** (LFO che modula LFO, random che modula la probabilità di una corsia, ecc.).
- **Modulazione rapida**: trascinando una sorgente su qualsiasi knob si crea uno slot; l'anello colorato intorno al knob mostra la portata.
- Cicli di feedback ammessi con un blocco di controllo (32 campioni) di ritardo: stabile e prevedibile.
- Valutazione a blocchi di 32 campioni con interpolazione lineare (abbastanza fluido per audio-rate leggero, CPU contenuta).

---

## 8. Interfaccia (1280 × 790, ridimensionabile)

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ MICROVIBE  preset ◄ ►   │ MACRO 1 2 3 4 │ DENSITY │ DRY/WET │ NORM │ OUT        │
├──────────────┬──────────────────────────────────────────────┬─────────────────┤
│ SORGENTI      │  GLITCH: griglia step / corsie poliritmiche  │  MOD            │
│ IN  TEXTURE   │  (4 corsie, Euclideo, probabilità, ratchet)  │  LFO 1-8 (tab)  │
│ PERC SHEPARD  │                                              │  ENV/RND/FOLL   │
├──────────────┴──────────────────────────────────────────────┤  MATRICE 32     │
│ FX CHAIN: [TAPE][VINYL][WOBBLE][DELAY][REVERB][  ][  ][  ]   │                 │
│ pannello del modulo selezionato                               │  SCALA/TUNING   │
└──────────────────────────────────────────────────────────────┴─────────────────┘
```
Stile coerente con Twinbox/Twinsynth (knob con frecce e Shift per la regolazione fine, doppio clic per il default).

---

## 9. Preset (180, generati da script come in Twinbox)
Categorie: Microhouse Glitch · Minimal Perc · Textures & Beds · Tape & Vinyl · Wobble/Lo-fi · Dub Delays · Shepard Risers · Microtonal · Polyrhythm · Performance (macro pronte) · **808 Minimal** *(1.1)* · **Chord Drones** *(1.1, progressioni di accordi sul drone)*.

---

## 10. Architettura del codice

```
Microvibe/
  CMakeLists.txt               (stesso schema di 5ynth)
  Source/
    PluginProcessor.*  Params.*  Presets.*  PluginEditor.*
    dsp/
      Clock.h              corsie, Euclideo, poliritmia, probabilità
      ModMatrix.*          sorgenti, matrice, smoothing, intermod
      Lfo.h Envelope.h Random.h Follower.h
      Tuning.*             Scala .scl/.kbm, MTS-ESP client
      GlitchEngine.*       buffer, eventi, crossfade
      TextureGen.*  ModalPerc.*  Shepard.*
      fx/ Tape.h Vinyl.h Wobble.h Delay.h Reverb.h Filter.h Resonator.h ...
      DelayLine.h          interpolazione frazionaria comune
  packaging/macos/make_pkg.sh  (modello: 5ynth)
  tests/                       test offline del DSP (render, NaN/denormali, CPU)
```
- Parametri in `AudioProcessorValueTreeState`; le destinazioni di modulazione sono un *offset* separato dal valore del parametro (l'automazione della DAW resta pulita).
- Oversampling 2× opzionale su Tape e saturazioni.
- Obiettivo CPU: < 8 % di un core a 48 kHz/256 con preset tipici.

## 11. Fasi di sviluppo proposte
1. Scheletro plugin, parametri, clock + corsie, mod matrix + LFO, CMake/CI/pkg.
2. Glitch engine + FX base (Wobble, Tape, Vinyl, Delay, Reverb, Filter).
3. Generatori: PERC modale, TEXTURE, SHEPARD; microtonalità.
4. GUI completa, drag-to-modulate, preset, manuale PDF, test e `auval`.

---

## 12. Stato dell'implementazione (v1.0.0)

Implementato tutto quanto sopra, con queste differenze rispetto alla bozza:
- **Morph tra pattern**: non implementato (c'è la catena A–H).
- **Tape/Vinyl**: tecniche standard (isteresi Jiles–Atherton, filtri d'epoca, polvere/crackle Poisson), non copiati
  da Groovulation, che non è in questo repository.
- **Oversampling 2×** solo nel modello a isteresi del Tape.
- I cicli di modulazione hanno un ritardo di un blocco di controllo (32 campioni), non di un campione.
- Interfaccia 1280 × 790 invece di 1200 × 760.

## 13. Versione 1.1.0

- Percussioni **808** sintetizzate (kick, tom, snare, clap, closed hat, open hat con choke) e filtro HP / BP / LP per ogni voce (§2.3).
- **Note del drone** scelte nella scala e **progressione di 4 accordi** con tempi e suddivisioni regolabili (§2.2, §6.1).
- **Stop della DAW**: il plugin smette di suonare; solo le code di delay e riverbero si spengono da sole (§5).
- 30 preset nuovi (808 Minimal, Chord Drones).
- Valutata e **non realizzata** (su richiesta) una versione separata "Microvibe Fx" come effetto AU puro: Microvibe è un music effect
  (AU `aumf`) e su Logic Pro si inserisce già sulle tracce audio; il VST3 è un effetto audio in tutte le DAW.
