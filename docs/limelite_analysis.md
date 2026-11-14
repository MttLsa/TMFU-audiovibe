# Limelite 1.3 — analisi del file `.ens` (riferimento per la Sezione B)

Fonte: `Limelite 1.3.ens` (Reaktor, versione integrata per Maschine 2; 146 MB con i campioni incorporati).
Crediti nel file: Instrument Design James Walker-Hall, Sound Design Joerg Remmer-Mueller, Interface Design Ian Warner, © Native Instruments 2004.
Estratti: nomi interni dei controlli e relative descrizioni (info text), voci dei menu, liste dei campioni e nomi degli snapshot.
Non ancora decodificati: valori numerici (range e default), posizioni grafiche e dati audio.

## 1. Strumenti (6 voci, non 5)

| Voce | Controlli (nome interno → significato) |
|---|---|
| **Kick** | KickWave (campione), KickTune, KickStart (posizione di inizio), KickLowcut, KickCutoff, KickRes, KickMod (destinazione Mod seq: `-- / wav / tune / cut`), KickSat (saturazione), KickShape `{thin … normal … fat}`, KickShapeSm (smoothness), KickHold, KickDecay, KickAcc, KickGain, KickPan |
| **Snare** | SnrWave, SnrTune, SnrStart, **Flam**, SnrMod (`-- / wav / tune / cut / flam`), SnrHold, SnrDecay, SnrFilter (tipo), SnrFiltFreq, SnrFiltRes, SnrSat, SnrShape, SnrShapeSm, SnrAcc, SnrGain, SnrPan |
| **Hat** | "Dual Hihat module. Noise based or multi-oscillator (808 style)": HatWave, HatTune, HatCutoff (passa-alto), HatRes, HatCloseDec, HatOpenDec, HatLink ("CH cuts OH decay"), HatMod (`-- / wav / tune / cut`), HatAcc, HatGain, HatPan |
| **Tie** | Voce tonale (bassi, chitarre, rhodes, accordi): TieWave, TieTune, TieStart, TieAttack, TieDecay, TieMP (pitch di modulazione), TieFM (profondità FM), TieV>A (velocity → ampiezza), TieVel (destinazione velocity: `-- / wav / tune / att / dcy / st / fm / cut`), TieFilter (`lp-bp-hp`, 2 poli), TieFiltFreq, TieFiltRes, **TiePorta**, **TieNoise**, TieSat, TieGain, TiePan |
| **Pod** | PodWave, PodTune, PodStart, PodAttack, PodDecay, PodZap, PodV>A, PodVel (destinazione: `-- / wav / tune / att / dcy / zap / cut`), PodFilter, PodFiltFreq, PodFiltRes, PodGain, PodPan |
| **Pulp** *(mancante in Twinbox)* | "Percussion oscillator that tracks the frequency of the other sounds": PulpOscSel (`Sine – Input – Noise`), PulpOscDec, PulpTune, PulpPorta, PulpHigh e PulpLow (limiti della frequenza rilevata), PulpInput (sorgente da seguire), filtro comb: PulpCombFreq, PulpCombFM, PulpCombCutoff, PulpCombFB, PulpCombMix; PulpGain, PulpPan |

Mixer: Gain e Pan per ciascuna delle 6 voci, On/Mute, Solo. Ingresso esterno (TieG / TieP "Enable external input"). Tune globale ("Global pitch transposition"), Output level. "FX mode" disattiva il motore sonoro e usa Limelite solo come effetto.

## 2. Effetti master

- **Magic + Dirt**: MagixPitch (pitch shift), MagicGrain (dimensione del grano), MagicRemix (posizione Remix → pitch), MagicMod (Mod seq → pitch), MagicNotch (notch), MagicMix (dry ↔ wet), **Dirt** ("Get dirty…").
- **Delay** (stereo, "tape-like" con risonanza): DelOne e DelTwo (tempi in sedicesimi), DelWide (stereo), DelCutoff, DelRes, DelFM (modulazione del cutoff), DelLFO (velocità), DelDM (modulazione del tempo), DelFB (feedback), DelLowcut, DelGain. Routing "Delay > XFX" (`dry … post-delay`).
- **xFx**: 23 modi. off, highpass, bandpass, lowpass, phaser, vowel, flanger, fm, reson, feedback, reflex, reverb, ufo, rainman, distort, ringmod, lofi, pitchshift, gate, chop, slice, freeze, spin. Controlli: xFxParam, xFxAddParam ("additional parameter"), xFxSpeed, xFxLength (loop), xFxSmooth, xFxHigh, xFxLow, xFxPlay, Rec XY, xFxGain.
- **FXMix** (xFx + delay, dry … wet), **ModInput** (Mod seq → ingresso fx), **ModMix** (Mod seq → mix fx).
- **Filtro con inviluppo**: FiltCutoff, FiltEF (envelope follower), FiltEFSm, FiltEFMod (Mod seq → cutoff, bipolare).
- **EQ / saturatore master**: EQLow, EQMid, EQHigh, EQSplit (Hz), EQAmp (guadagno del saturatore in dB).
- **Norm**: NormAmp, NormRel ("peak release", fast … slow), NormSmooth.

## 3. Sequencer, Remix e clock

- Sequencer principale Kick/Snare/Hat (con accento per voce: KickAcc, SnrAcc, HatAcc), più le sequenze Tie, Pod e Mod con lunghezze proprie (SeqLen, SeqTie, SeqPod, SeqMod) e **Stretch** ("to main sequence length"). "Tie centre".
- Mod display: modo proporzionale oppure binario.
- **Pattern sequencer**: 8 pattern, catena con lunghezza del loop (`seq_length`), on/off, visualizzazione "Track current pattern or locked".
- **Remix**: Remix pattern (127 schemi predefiniti), Remix shift (valori positivi spostano la posizione, negativi l'inizio), Start Offset, Remix On, Remix rate (ogni 2, 4, 8… battute), "Remix now", auto fill.
- **Clock**: Half tempo, clock interno (BPS), Swing, Start step, Song Position.
- **Voice Router**: copia e incolla di una voce tra pattern o snapshot.

## 4. Librerie di campioni (solo i nomi)

Kick 68, Snare 116, Hat 83, Tie 111 (tonali), Pod 120. Elenco completo in `ens_samplelists.json` (non incluso nel repository).

## 5. Snapshot di fabbrica

176 preset, tra cui: For Your Pleasure, Stage Drums, Tripod, Little Pieces, Practise Your Aim, Nutshell, Rock Steady, DiscoLite, Uptight, Boys Toys, Bump and Grind, Go For That, PopLite, Boom Time, Roppongi, Hold the Line, Dirty Times, Metropolis, This is Not a Test, Garden Party, Handbrake Turn, Muzak, NerdLite, The Usual Suspects, 808 to Ground Control, Outer Limits, Slice and Dice, Ghost Talk, Steam Power, Freaky Spin, RDAVIS 1–15, …

## 6. Pagine di parametri per Maschine/NKS (8 per pagina)

R5 Kick / R5 Snare: Waveform, Accent, Start, Pan, Tune, Shape, Saturation, Gain · R5 Hat: Waveform, Accent, Open Decay, Pan, Tune, Cutoff, Closed Decay, Gain · R5 Tie / R5 Pod: Waveform, Velocity to Amplitude, Start, Pan, Tune, Attack, Decay, Gain · R5 Pulp: Portamento, Source, Comb Filter, Pan, Tune, Osc, Frequency, Gain · R5 XFX: Parameter X, High, Speed, Delay, Additional Parameter, Low, Length, Gain · R5 Add FX: Delay One, Delay Two, Cutoff, Magic Pitch, Delay Feedback, Delay Gain, Filter Mod, Magic Mix · R5 Mix And Out: Tune, Normalize Amplitude, Dirt, FX Mix, Normalize Release, Output Gain.

## Differenze principali rispetto a Twinbox v1.0

1. Manca la 6ª voce **Pulp**.
2. Kick e Snare: mancano Start, Hold, Saturation, Shape e Shape smooth, destinazione Mod per voce, accento per voce; alla Snare manca anche **Flam**.
3. La destinazione della Mod sequence è **per voce** (wav / tune / cut / flam). In Twinbox v1.0 è un'unica destinazione globale.
4. Tie è una voce tonale con Attack/Decay, Portamento, Noise, Saturation, filtro LP/BP/HP e destinazione della velocity; il "Tie FM" originale è FM con pitch di modulazione (MP).
5. Pod: Attack, Start, filtro e destinazione della velocity.
6. FX: mancano Magic (pitch shifter granulare + notch), xFx (23 modi), EQ/saturatore master e filtro con envelope follower. Il Delay originale ha due tempi in sedicesimi, width, filtro risonante e LFO.
7. Sequencer: mancano il pattern sequencer (catena di 8 pattern), il motore **Remix** (127 schemi, rate, shift, offset), Half tempo, Start step, Voice Router e la modalità binaria/proporzionale della corsia Mod.
