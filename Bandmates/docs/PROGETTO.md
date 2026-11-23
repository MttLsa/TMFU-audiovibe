# BANDMATES — session player virtuali per Ableton Live (Max for Live)

Produttore **TMFU audiovibe**. Cinque device MIDI Max for Live che fanno in Live quello che i *Session Player* fanno in Logic Pro:
un musicista virtuale che **suona una parte MIDI** a partire da pochi controlli musicali (complessità, intensità, fill, swing…)
e che **segue la traccia accordi e la struttura del brano**.

| Device | Ruolo | Corrisponde in Logic a |
|---|---|---|
| **Bandmates Chords** | Traccia accordi: scrivi/scegli la progressione, Chord ID dai clip MIDI | Traccia accordi globale + Chord ID |
| **Bandmates Drums** | Batterista acustico (Soul, Jazz, R&B, Rock, Pop, Funk) | Drummer (generi acustici) |
| **Bandmates Beats** | Batterista elettronico (House, Tech House, Techno, Microhouse, Rominimal) | Drummer elettronico |
| **Bandmates Bass** | Bassista: segue accordi e cassa | Bass Player |
| **Bandmates Keys** | Tastierista: mano sinistra + destra, voicing con condotta delle voci | Keyboard Player |

Come in Logic, dove il player genera **regioni** che si modificano con l'editor, qui i device generano **clip MIDI** (Session View
o Arrangement). Ogni clip **ricorda le proprie impostazioni**: selezionandolo, il device ricarica i controlli con cui è stato creato;
muovendo un controllo, il clip selezionato viene rigenerato.

---

## 1. Diritti: cosa si può replicare e cosa no

Funzioni, flusso di lavoro e *tipi* di parametri (pad complessità/intensità, fill, swing, ghost notes, slide, voicing…) sono idee e
convenzioni musicali: non sono protetti da copyright e si possono riprodurre. Bandmates quindi **riprende il funzionamento** dei
Session Player, ma:

- **nessun materiale Apple**: niente codice, grafica, icone, foto, campioni, Apple Loops, pattern o modelli addestrati di Logic;
- **nomi originali**: i musicisti virtuali (Gil, Nina, Dalia, Marco, Volt, Mica…) e i loro stili sono inventati per questo progetto;
  non si usano i nomi dei batteristi/player di Logic né i marchi "Logic", "Session Player", "Drummer" come nome di prodotto;
- **interfaccia propria**: layout, colori e testi sono quelli standard dei device Live (`live.dial`, `live.tab`…), non la grafica di Logic;
- **pattern propri**: tutti i ritmi della libreria (`engine/*.js`) sono scritti da zero; le progressioni preset sono formule
  armoniche di pubblico dominio (I–V–vi–IV, ii–V–I, blues, andalusa…).

Nella documentazione Logic Pro si cita solo per descrivere la compatibilità d'uso ("Logic Pro è un marchio di Apple Inc.").
*Non è una consulenza legale: prima di vendere il prodotto conviene una verifica di nomi e marchi.*

## 2. "Logic usa l'AI: anche Bandmates?"

**No, non nella versione 1.** I Session Player di Logic si basano su modelli addestrati da Apple su esecuzioni di musicisti reali,
che restano di Apple. Bandmates usa invece un **motore generativo algoritmico**:

- librerie di pattern scritte a mano, ordinate dal semplice al complesso, scelte e variate con regole musicali e numeri casuali
  controllati (seed: stessa impostazione = stessa esecuzione, "Nuova variazione" = nuovo seed);
- teoria musicale esplicita: riconoscimento accordi, condotta delle voci, note di avvicinamento, scale per accordo;
- "umanizzazione": swing, feel avanti/indietro per singolo strumento, micro-timing, dinamiche.

Vantaggi: gira dentro Max for Live senza dipendenze, è deterministico, è controllabile e testabile (16 test automatici).
Limite: l'esecuzione è credibile ma meno varia di un modello addestrato su ore di musicisti veri.

**Roadmap AI (v2, opzionale):** si può aggiungere un livello di machine learning senza usare nulla di Apple:
- dataset con licenza aperta, ad esempio *Groove MIDI Dataset* (Google Magenta, CC BY 4.0, ~13 ore di batteristi reali), o registrazioni
  proprie suonate da musicisti pagati per questo;
- modelli piccoli (es. VAE/transformer per groove e umanizzazione) esportati in ONNX ed eseguiti da **Node for Max**
  (`node.script`) o da un piccolo helper nativo;
- il motore attuale resta come base e come "fallback" quando il modello non c'è.

## 3. Parametri: Logic → Bandmates

Legenda: ✅ implementato · ≈ equivalente adattato a Live · ⏳ in roadmap. Le voci segnate *(verificare)* descrivono l'editor di
Logic come documentato da fonti terze e vanno ricontrollate sulla propria versione di Logic.

### Comuni a tutti i player
| Logic | Bandmates | |
|---|---|---|
| Scelta del player per genere | Menu **Player** (nome · genere) | ✅ |
| Preset del player | Menu **Preset**: Standard, Strofa, Ritornello, Morbido, Pieno, Essenziale | ✅ |
| Pad XY Semplice↔Complesso / Morbido↔Forte | **Pad XY** (Complessità / Intensità, automatizzabili) | ✅ |
| Fill | **Fill** (quantità e lunghezza, fine frase 4/8 battute e fine sezione) | ✅ |
| Swing (1/8 o 1/16) | **Swing** + **Griglia** 8vi/16mi (0 = dritto, 67 ≈ terzina) | ✅ |
| Feel (avanti/indietro) | **Feel** −100…+100 (per la batteria pesato per strumento: il rullante si sposta di più) | ✅ |
| Rigenerazione della regione | **Genera**, **Nuova variazione**, **Auto** (rigenera il clip selezionato a ogni modifica) | ✅ |
| Impostazioni per regione | Salvate nel nome del clip (`[bm1 …]`) e ricaricate selezionandolo | ✅ |
| Segue i marker di arrangiamento | **Segui sezioni**: nome scena (Session) o locator (Arrangement): Intro, Strofa/Verse, Pre/Build, Ritornello/Chorus/Drop, Bridge, Solo, Break, Outro | ✅ |
| Riempie l'intero brano | **Tutte le scene** / **Arrangement** (un clip per ogni clip della traccia accordi) | ✅ |
| Umanizzazione | **Umanizza** (tempo + dinamica) | ✅ |

### Batterista acustico (Drums)
| Logic | Bandmates | |
|---|---|---|
| Kit: percussioni (tamburello, shaker, clap…) + slider pattern | **Percussioni** on/off, tipo (Tamb, Shaker, Clap, Cowbell, Conga), variazione 1–5 | ✅ |
| Kit: piatti hi-hat / ride + slider pattern | **Piatti** on/off, **Hi-hat/Ride**, variazione 1–5 | ✅ |
| Kit: cassa e rullante + slider pattern | **Cassa+Rull.** on/off, variazione 1–5 | ✅ |
| Tom | **Tom** on/off (usati nei fill e nei groove "tribali") | ✅ |
| Ghost notes | **Ghost** (rullante e, nel funk/R&B, cassa) | ✅ |
| Hi-hat automatico / apertura hi-hat | **HH auto** + **Apertura hi-hat** | ✅ |
| Half time / double time | **Tempo**: ½, Normale, ×2 | ✅ |
| Follow (cassa e rullante seguono una traccia) | **Segui ritmo**: la cassa segue gli attacchi di una traccia MIDI scelta | ≈ (solo MIDI; Logic segue anche l'audio) |
| Sidestick a dinamiche basse | Automatico (Pop, Soul, R&B) | ✅ |
| Kit Drum Kit Designer | Mappa GM (C1 = cassa) per Drum Rack di Live | ≈ |

### Batterista elettronico (Beats)
| Logic | Bandmates | |
|---|---|---|
| Ogni pezzo del kit con on/off e slider di complessità | **Kick, Clap, Rim, Hat, Open, Perc, Ride, Crash**: on/off + densità | ✅ |
| Pad XY che sposta tutto | Pad XY somma ±30% alle densità | ✅ |
| Generi | **House** (Volt), **Tech House** (Tess), **Techno** (Kern), **Microhouse** (Mica), **Rominimal** (Ilio) | ✅ |
| Fill / build | Fill a fine frase (snare roll, kick drop, open hat, perc run); sezione **Build** = rullata in accelerazione; **Break** = senza cassa; **Drop** = crash | ✅ |
| — | **Micro-timing**: rim, perc e hat "sbandano" dalla griglia (tipico microhouse/rominimal); la cassa resta sempre in griglia | ✅ (aggiunto) |
| — | Pattern su 2 battute e variazioni "saltellanti" per Microhouse/Rominimal | ✅ (aggiunto) |

### Bassista (Bass)
| Logic | Bandmates | |
|---|---|---|
| Segue la traccia accordi | Legge gli accordi dalla traccia **Bandmates Chords** (stessa scena / stessa posizione) | ✅ |
| Slides + modo (Pitch Bend / note) *(verificare)* | **Slide** + **Modo slide**: *Nota di grazia* o *Legato* (glide del synth) | ≈ (il pitch bend non è scrivibile nei clip via API) |
| Dead notes | **Dead notes** (note corte a bassa velocity) | ✅ |
| Segue la cassa del batterista | **Segui cassa**: legge la cassa dal clip di Drums/Beats nella stessa scena | ✅ |
| Fill | Fill: scale run e avvicinamenti verso l'accordo successivo | ✅ |
| — | **Ottava** (−1/0/+1), **Durata note**, **Variazione ritmo** 1–5 | ✅ (aggiunti) |
| Generi | Soul, Jazz (walking), R&B, Rock, Pop, Funk, House, Tech House, Techno, Microhouse/Rominimal | ✅ |

### Tastierista (Keys)
| Logic | Bandmates | |
|---|---|---|
| Mano sinistra: stile *(verificare)* | **Mano sinistra**: Off, Fondamentale, Ottave, Fond.+quinta, Arpeggiata, Shell, Stride | ✅ |
| Mano destra: voicing *(verificare)* | **Voicing**: Triade, Stretto 4 voci, Aperto, Drop 2, Shell, Quarte, Ottava sopra, Ricco | ✅ |
| Movimento *(verificare)* | **Movimento**: Tenuto, Pulsato, Comping (per genere), Spezzato, Arpeggio, In levare | ✅ |
| Registro / range | **Registro** mano destra | ✅ |
| Mute di una mano | **Muto sinistra**, **Muto destra** | ✅ |
| Estensioni / colori | **Estensioni** (7ª, 9ª, 11ª, 13ª in base all'accordo) | ✅ |
| Pedale | **Pedale** (prolunga le note fino al cambio accordo) | ≈ (CC64 non scrivibile via API) |
| — | **Direzione arpeggio**, **Variazione** | ✅ (aggiunti) |

### Traccia accordi (Chords)
| Logic | Bandmates | |
|---|---|---|
| Traccia accordi globale | Una traccia MIDI con **Bandmates Chords**: i suoi clip *sono* la traccia accordi (scene o arrangement) | ≈ |
| Modifica accordi (fondamentale, qualità, basso) | Testo: `C G/B \| Am7 - \| Fmaj7 \| G7sus4 G7` (33 qualità, slash chord, numeri romani `ii7 V7 I`) | ✅ |
| Chord ID da regione MIDI | **Chord ID dal clip selezionato** (risoluzione ¼, ½, 1 battuta) | ✅ |
| Chord ID da audio | — | ⏳ (si può fare: Live "Converti armonia in MIDI" + Chord ID) |
| Progressioni pronte | 20 progressioni (Pop, Jazz, Blues, Minore, Gospel, House, Neo-soul…) in qualsiasi tonalità o nella **scala del brano di Live 12** | ✅ |
| Trasposizione | **+1 st / −1 st** | ✅ |

## 4. Architettura

```
 engine/  (JavaScript ES5 puro, testato in Node, eseguito identico dentro Max)
   rng.js        numeri casuali con seed            theory.js   accordi, scale, numeri romani, Chord ID
   voicing.js    voicing + condotta delle voci      chords.js   progressioni, testo, preset, accordi <-> note
   groove.js     swing, feel, umanizzazione, sezioni styles.js  musicisti virtuali e preset
   drummer.js  electro.js  bass.js  keys.js        generatori: parametri -> note MIDI
 max/
   spec.js       controlli di ogni device (usato sia per disegnare i device sia per leggerli)
   runtime.js    Live API: trova traccia accordi / batteria, legge e scrive clip, scene, locator, tag nel nome clip
   drums.js beats.js bass.js keys.js chords.js     logica specifica di ogni device
   bm_xy.js      pad XY (jsui)
 tools/build.js  genera devices/*.amxd (+ .maxpat) e un bm_<device>.js autosufficiente per ciascuno
 tests/          motore + device eseguiti contro una sessione di Live simulata
```

Flusso di "Genera":
1. il device individua il **bersaglio**: clip selezionato sulla propria traccia, oppure slot evidenziato, oppure slot della scena selezionata;
2. legge **lunghezza** (clip esistente → clip accordi della stessa scena → 8 battute), **sezione** (nome scena o locator), **tempo in chiave**;
3. legge gli **accordi** dal clip della traccia Chords nella stessa scena / posizione (Chord ID); senza traccia accordi usa la tonica della scala di Live;
4. il basso legge la **cassa** dal clip di Drums/Beats; la batteria con *Segui ritmo* legge gli attacchi della traccia scelta;
5. il motore genera le note; il runtime le scrive (`add_new_notes`) e rinomina il clip `Nome · Sezione [bm1 …]`.

## 5. Limiti noti e cose da verificare in Live

Il motore e la logica dei device sono testati automaticamente; **l'integrazione dentro Live va provata a mano** (in questo ambiente
non c'è Live). Punti da verificare al primo avvio, in ordine di rischio:
1. il file `.amxd` generato si apre (formato: header `ampf`, tipo `mmmm`, chunk `ptch` con il patcher JSON). Se Live non lo apre,
   aprire `Bandmates X.maxpat` in Max, copiare tutto in un nuovo "Max MIDI Effect" e salvare;
2. Max trova gli script `bm_*.js` e `bm_xy.js` nella stessa cartella del device (se no: congelare il device dall'editor Max);
3. `add_new_notes` con oggetto JS e `get_notes_extended` (Live 11+);
4. `create_midi_clip` per il pulsante *Arrangement* (Live 12);
5. textedit della traccia accordi: invio con Return.

Requisiti: **Live 11 o 12 Suite (o Standard + Max for Live)**. Funzioni Live 12: scala del brano come tonalità, creazione di clip in Arrangement.

## 6. Roadmap
- v1.1: modalità "dal vivo" (generazione in tempo reale durante il playback, senza scrivere clip), mappa note personalizzabile per Drum Rack.
- v1.2: Chord ID da audio, generatore come **MIDI Tool** di Live 12 (Generators) direttamente nell'editor dei clip.
- v2: livello di machine learning per groove e umanizzazione (vedi §2).
