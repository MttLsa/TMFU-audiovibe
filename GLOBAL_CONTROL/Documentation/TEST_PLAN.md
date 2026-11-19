# GLOBAL CONTROL — Prova di fattibilità su Mac (Fasi 1–10)

Questa è la procedura per la "Prima attività" del capitolato: va eseguita su un Mac con Ableton Live Suite
(o Live + Max for Live) e FabFilter Pro-C 2. **Il prodotto non è da considerarsi validato finché questi test non
sono stati eseguiti in Live reale e il report allegato al repository.**

## 0. Preparazione (una volta)

### Installare `GLOBAL_CONTROL_BRIDGE.amxd`
Via installer o app: `GLOBAL_CONTROL-<ver>-macOS.pkg`, oppure GLOBAL CONTROL › **SETUP › Install bridge in User Library**.
Il device è generato da `tools/make_amxd.py` (in `AbletonBridge/Build/`, non congelato: `gc_bridge.js` deve stare
nella stessa cartella). **[VERIFICA]** che Live apra il `.amxd` generato e trovi `gc_bridge.js`: la Max Console deve
riportare `[GLOBAL CONTROL] bridge 1.0 ready` e il device "Waiting for GLOBAL CONTROL...".

Se Live rifiuta il file generato, crearlo a mano e sostituirlo in `AbletonBridge/Build/`:
1. In Live: trascinare **Max MIDI Effect** su una nuova traccia MIDI `GLOBAL CONTROL`, cliccare *Edit*.
2. In Max: *File → Open…* `AbletonBridge/MaxPatch/GLOBAL_CONTROL_BRIDGE.maxpat`, Cmd-A, Cmd-C; nella finestra del
   device eliminare il contenuto del template e incollare (Cmd-V).
3. Copiare `AbletonBridge/LiveAPI/gc_bridge.js` nella cartella di destinazione, *Save As…*
   `GLOBAL_CONTROL_BRIDGE.amxd` nella User Library (`~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/`).
   Facoltativo: **Freeze Device** per incorporare il JS.

**[VERIFICA]** che `[pattr gc_project_id]` risulti parametro "Stored Only" di tipo blob (Inspector). Se no, impostarlo
a mano: *Parameter Mode Enable* ✔, *Type* = Blob, *Parameter Visibility* = Stored Only.

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

## FASE 4 — Associazione a Macro 1 (nell'app)
In GLOBAL CONTROL: Device browser → Pro-C 2; Parameters → Threshold; "to Macro 1" → **ASSIGN**.
Il knob 1 deve mostrare "Pro-C 2 / Threshold / 10 instances" e la scala `linear (dB)` nel Log.
Ripetere le Fasi 5–7 muovendo il knob: Absolute (valore uguale ovunque), Relative (differenze preservate), con
le GUI di alcune istanze aperte in Live. Provare anche Exclude su un'istanza e un gruppo.

## FASE 8 — Feedback dalla GUI del plug-in
```bash
python3 gc_poc.py watch --device "Pro-C 2" --param Threshold
```
Muovere Threshold nella finestra di una istanza → deve comparire una riga `[external]` con il nuovo valore.
Disegnare automazione su Threshold, avviare il play → righe `[automation]`.
Muovere con un controller MIDI mappato → `[external]` (limite documentato: non distinguibile dalla GUI).
**[VERIFICA]** e annotare: latenza percepita, se il plug-in notifica durante il trascinamento o solo al rilascio.

## FASI 9–10 — Persistenza (nell'app)
1. Con le macro assegnate, salvare un **Project preset**; salvare il Set, chiudere Live, riaprire il Set.
2. GLOBAL CONTROL (lasciata aperta o riavviata) deve ricollegarsi da sola, riconoscere il progetto (stesso id nel
   pannello Project), rifare la scansione e **ripristinare le mappature** (10 istanze per Macro 1), senza istanze rosse.
3. Spostare una traccia, rinominarne un'altra, eliminare un'istanza, riaprire: le istanze spostate vengono ritrovate,
   quella eliminata compare come *not found*, nessuna viene associata in modo errato.

### Verifica con il client di prova
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
- `bridge`: `gc_bridge.js` su un LOM simulato (Node);

`app_integration`: logica dell'app (controller + connessione OSC) contro il bridge vero via UDP, compresa la
riapertura del Set con id diversi e il cambio di Set.

End-to-end OSC reale su UDP con il LOM simulato (fixture ipotetica, non dati di Pro-C 2):
```bash
node GLOBAL_CONTROL/Tests/OSCtests/mock_bridge_server.js &
python3 GLOBAL_CONTROL/Tests/AbletonIntegrationTests/gc_poc.py phase-test --device "Pro-C 2" --param Threshold --absolute -12
```

## Report da allegare
`Documentation/results/phase_test_live.md` (generato), `proc2_params.json`, versione di Live/macOS/Pro-C 2,
formato (VST3/AU), output del comando `watch`, note sulle voci **[VERIFICA]** di `TECHNICAL_ANALYSIS.md`.
