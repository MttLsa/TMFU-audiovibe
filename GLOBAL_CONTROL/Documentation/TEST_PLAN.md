# GLOBAL CONTROL — Prova di fattibilità su Mac (Fasi 1–10)

Questa è la procedura per la "Prima attività" del capitolato: va eseguita su un Mac con Ableton Live Suite
(o Live + Max for Live) e FabFilter Pro-C 2. **Il prodotto non è da considerarsi validato finché questi test non
sono stati eseguiti in Live reale e il report allegato al repository.**

## 0. Preparazione (una volta)

### Creare `GLOBAL_CONTROL_BRIDGE.amxd`
1. In Live: trascinare **Max MIDI Effect** (Browser → Max for Live) su una nuova traccia MIDI chiamata `GLOBAL CONTROL`.
2. Cliccare l'icona *Edit* del device: si apre l'editor di Max.
3. In Max: *File → Open…* `GLOBAL_CONTROL/AbletonBridge/MaxPatch/GLOBAL_CONTROL_BRIDGE.maxpat`; selezionare tutto
   (Cmd-A), copiare (Cmd-C), tornare alla finestra del device, selezionare tutto ed eliminare il contenuto del
   template, incollare (Cmd-V).
4. Copiare `AbletonBridge/LiveAPI/gc_bridge.js` nella stessa cartella dove si salverà il device.
5. *File → Save As…* `GLOBAL_CONTROL_BRIDGE.amxd` nella User Library
   (`~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/`), poi **Freeze Device** (icona fiocco di
   neve nella barra dell'editor) e salvare di nuovo: il JS viene incorporato nel `.amxd`.
6. Chiudere l'editor. Il device deve mostrare "Waiting for GLOBAL CONTROL...". La Max Console (finestra Max)
   deve riportare `[GLOBAL CONTROL] bridge 0.1.0 ready`.

**[VERIFICA]** che l'oggetto `[pattr gc_project_id]` risulti come parametro "Stored Only" di tipo blob
(Inspector del pattr). Se Max non accetta il tipo blob dal file, impostarlo a mano nell'Inspector:
*Parameter Mode Enable* ✔, *Type* = Blob, *Parameter Visibility* = Stored Only.

### Client di prova
Python 3.8+ già presente in macOS (o `brew install python`). Nessuna dipendenza.
```bash
cd GLOBAL_CONTROL/Tests/AbletonIntegrationTests
python3 gc_poc.py hello
```
Deve stampare `liveVersion`, `bridgeVersion`, `projectId`.

## FASE 1 — Progetto di test
Nuovo Set con **almeno 5 tracce** e **10 istanze di FabFilter Pro-C 2** con Threshold diversi, ad esempio:

| Traccia | Istanze | Threshold |
|---|---|---|
| Kick | 2 | -12, -6 dB |
| Snare | 1 | -8 dB |
| Bass | 1 | -15 dB |
| Vocal | 2 | -10, -20 dB |
| Drum Bus | 2 dentro un Audio Effect Rack (2 catene) | -30, -3 dB |
| Synth | 1 | -18 dB |
| Return A | 1 | -24 dB |

Più la traccia `GLOBAL CONTROL` con il bridge. Salvare come `GC_Test_ProC2.als`.

Prima di tutto, catturare i parametri **realmente esposti** da Live per Pro-C 2 (VST3 e, se installato, AU):
```bash
python3 gc_poc.py params --device "Pro-C 2" --json proc2_params.json
python3 gc_poc.py curve --device "Pro-C 2" --param Threshold
```
Se `Threshold` non compare: aprire *Configure* sul titolo del device Pro-C 2, toccare il knob Threshold nella GUI
del plug-in, uscire da Configure, ripetere. Annotare nel report quali parametri compaiono senza Configure.

## FASI 2–7 — automatiche
```bash
python3 gc_poc.py phase-test --device "Pro-C 2" --param Threshold --expect 10 --absolute -12 --delta -2 --report ../../Documentation/results/phase_test_live.md
```
Esegue e verifica, rileggendo i valori effettivi da Live:
- Fase 2: tutte le istanze trovate (≥ 10, anche dentro il rack e sulla return);
- Fase 3: Threshold leggibile su tutte;
- Fase 5/6: un solo comando porta **tutte** le istanze a -12 dB (conferma `source=gc` per ciascuna);
- Fase 7: -2 dB relativo preserva le differenze (-12→-14, -8→-10 …);
- ripristino dei valori originali.

In Live: osservare che i knob Threshold delle GUI aperte si muovono insieme.

## FASE 4 — Associazione a Macro 1
In questa fase PoC l'associazione è simulata dal comando `set`/`relative`, che usa lo stesso MacroEngine logico
(scala misurata + absolute/relative). L'associazione reale al knob avviene nell'app (Milestone 7–8).

## FASE 8 — Feedback dalla GUI del plug-in
```bash
python3 gc_poc.py watch --device "Pro-C 2" --param Threshold
```
Muovere Threshold nella finestra di una istanza → deve comparire una riga `[external]` con il nuovo valore.
Disegnare automazione su Threshold, avviare il play → righe `[automation]`.
Muovere con un controller MIDI mappato → `[external]` (limite documentato: non distinguibile dalla GUI).
**[VERIFICA]** e annotare: latenza percepita, se il plug-in notifica durante il trascinamento o solo al rilascio.

## FASI 9–10 — Persistenza
1. `python3 gc_poc.py hello` → annotare `projectId`.
2. Salvare il Set, chiudere Live, riaprire il Set.
3. `python3 gc_poc.py hello` → il `projectId` deve essere **identico** (persistenza del pattr blob).
4. `python3 gc_poc.py scan` → stesse 10 istanze; gli `id` saranno diversi (atteso), le `key` e i `paramNamesHash` uguali.
5. La riconciliazione delle mappature (InstanceReconciler, coperta dai test unitari) viene verificata con i preset
   nell'app (Milestone 10).

## Test di regressione senza Live (CI / Linux)
```bash
cmake -S GLOBAL_CONTROL -B build-gc && cmake --build build-gc && ctest --test-dir build-gc --output-on-failure
```
- `core`: scale, absolute/relative, riconciliazione, protocollo (C++);
- `bridge`: `gc_bridge.js` su un LOM simulato (Node).

End-to-end OSC reale su UDP con il LOM simulato (fixture ipotetica, non dati di Pro-C 2):
```bash
node GLOBAL_CONTROL/Tests/OSCtests/mock_bridge_server.js &
python3 GLOBAL_CONTROL/Tests/AbletonIntegrationTests/gc_poc.py phase-test --device "Pro-C 2" --param Threshold --absolute -12
```

## Report da allegare
`Documentation/results/phase_test_live.md` (generato), `proc2_params.json`, versione di Live/macOS/Pro-C 2,
formato (VST3/AU), output del comando `watch`, note sulle voci **[VERIFICA]** di `TECHNICAL_ANALYSIS.md`.
