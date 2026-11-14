# AutoNormalizer

Plugin audio (**AU, VST3, VST2**) che porta a **0 dB** il volume del plugin che lo precede **una sola volta per ogni cambio di preset**, poi blocca il guadagno e smette di misurare.

![AutoNormalizer](screenshot.png)

Non è un compressore né un AGC: dopo la misura il guadagno resta **fisso**. Le dinamiche del suono non vengono toccate.

```
[ Synth / Effetto ] ──► [ AutoNormalizer ] ──► resto della catena
```

## Come funziona

1. **Rilevamento del cambio preset**
2. **Attesa del segnale**: se c'è silenzio aspetta finché il segnale non supera il *Signal gate*.
3. **Misura**: analizza *Measure time* secondi di segnale attivo. Il silenzio non viene contato.
4. **Normalizzazione**: calcola `guadagno = Target − livello misurato` e lo applica con una rampa di 20 ms, senza click.
5. **Blocco** (`LOCKED`): la misura si ferma e il guadagno resta fisso fino al prossimo cambio preset.

Quando lo inserisci per la prima volta normalizza il primo suono che arriva. Quando riapri il progetto il guadagno salvato viene ripristinato senza rifare la misura.

### Come capisce che il preset è cambiato

Un plugin non può "vedere" dentro un altro plugin: la DAW li isola, e nessun formato (AU, VST, VST3) permette di leggere il preset del plugin precedente. AutoNormalizer quindi usa tre metodi, attivabili anche insieme:

| Metodo | Affidabilità | Come si usa |
|---|---|---|
| **Auto-detect** | buona, euristica | Confronta continuamente l'"impronta" del suono (spettro su 8 bande + livello) con quella memorizzata all'ultima normalizzazione. Se cambia in modo netto e stabile per almeno 300 ms, considera il preset cambiato. Suonare note diverse o con dinamiche diverse sullo stesso preset **non** fa scattare il rilevamento (vedi i test). |
| **MIDI Program Change** | 100 % | Se cambi preset con un messaggio Program Change, mandalo anche ad AutoNormalizer (in AU è di tipo `aumf`, effetto con ingresso MIDI). |
| **Normalize Now** | 100 % | Pulsante nell'interfaccia oppure parametro automatizzabile/MIDI-learn `Normalize Now` (scatta sul passaggio off→on). Puoi collegarlo allo stesso controller che usi per cambiare preset. |

Se due preset hanno timbro e volume quasi identici, l'auto-detect può non accorgersene. In quel caso però non serve correggere il volume. Per avere controllo totale usa il Program Change o *Normalize Now*.

## Parametri

| Parametro | Default | Descrizione |
|---|---|---|
| Measure Mode | Peak | **Peak (dBFS)**: il picco va al Target (0 dB = picco a 0 dBFS). **RMS (dBFS)**: livello medio. **Loudness (LUFS)**: volume percepito con pesatura K secondo ITU-R BS.1770. È la modalità migliore per avere preset che *suonano* tutti allo stesso volume. Con LUFS usa un target come −14 o −18. |
| Target Level | 0 dB | Livello a cui portare il segnale |
| Measure Time | 1.5 s | Secondi di segnale attivo da analizzare |
| Signal Gate | −50 dB | Sotto questa soglia il segnale conta come silenzio |
| Auto Detect | on | Rilevamento automatico del cambio preset |
| Sensitivity | 50 % | Più alta = scatta con differenze più piccole |
| MIDI Program Change | on | Un Program Change avvia la normalizzazione |
| Max Boost | +24 dB | Limite di amplificazione, per non alzare rumore o code |
| Safety Ceiling | on | Durante attesa e misura l'uscita non supera mai 0 dBFS, anche se il nuovo preset è molto più forte |
| Freeze Gain | off | Ignora qualunque trigger e mantiene il guadagno attuale |
| Normalize Now | — | Trigger manuale o automatizzabile |
| RESET 0 dB | — | Riporta il guadagno a 0 dB (unity) |

L'interfaccia mostra lo stato (`LOCKED` / `WAITING FOR SIGNAL` / `MEASURING`), il guadagno applicato, l'ultimo livello misurato, chi ha fatto scattare la misura e un indicatore "Preset change": quando arriva al 100 % scatta il rilevamento automatico.

> **Nota su 0 dBFS in modalità Peak:** il picco misurato va esattamente a 0 dBFS. Note suonate dopo la misura più forti di quelle misurate possono superarlo. Se vuoi margine imposta il Target a −1 dB oppure usa la modalità LUFS.

## Compilare

Servono CMake ≥ 3.22 e un compilatore C++17. JUCE 8.0.4 viene scaricato automaticamente, oppure lo indichi con `-DJUCE_DIR=...`.

```bash
cd AutoNormalizer
```

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --config Release
```

I file prodotti sono in `build/AutoNormalizer_artefacts/Release/`:

- `AU/AutoNormalizer.component` (solo macOS) → `~/Library/Audio/Plug-Ins/Components/`
- `VST3/AutoNormalizer.vst3` → `~/Library/Audio/Plug-Ins/VST3/` (mac) oppure `C:\Program Files\Common Files\VST3\` (Windows)
- `VST/AutoNormalizer.vst` / `.dll` → solo se abiliti il VST2 (vedi sotto)

### VST2

Steinberg non distribuisce più l'SDK VST2 e non è permesso ridistribuirlo, quindi non può essere incluso nel progetto. Se possiedi l'SDK legacy (`vstsdk2.4`, firmato prima del 2018), compila così:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVST2_SDK_DIR=/percorso/vstsdk2.4
```

### Test

```bash
ctest --test-dir build -C Release --output-on-failure
```

`tests/EngineTest.cpp` simula un plugin che cambia preset e verifica:

- normalizzazione a 0 dBFS
- rilevamento automatico del cambio preset
- nessun falso trigger cambiando note o dinamica o con pause di silenzio
- trigger MIDI
- modalità RMS e LUFS
- safety ceiling
- freeze
- ripristino del progetto

`tests/PluginTest.cpp` (`-DAN_BUILD_PLUGIN_TEST=ON`) prova il plugin vero e proprio: Program Change, automazione, salvataggio e ricaricamento dello stato, screenshot della GUI.

La GitHub Action `.github/workflows/autonormalizer.yml` compila su macOS, Windows e Linux, valida l'AU con `auval` e il VST3 con `pluginval`, e carica i plugin compilati come artifact scaricabili.
