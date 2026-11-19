# GLOBAL CONTROL

Controllo centralizzato dei parametri dei plug-in **originali** in Ableton Live (macOS): 8 macro knob che muovono
lo stesso parametro su più istanze contemporaneamente (es. Threshold di 10 × FabFilter Pro-C 2), in modalità
**Absolute** o **Relative**, senza spostare, sostituire o incapsulare i device.

Due componenti:

| Componente | Cosa fa | Dove |
|---|---|---|
| **GLOBAL CONTROL.app** | GUI, macro, mapping, istanze, preset (C++17 / JUCE, nessun audio) | `GlobalControlApp/` |
| **GLOBAL_CONTROL_BRIDGE.amxd** | Max MIDI Effect, **uno per Set** su una traccia MIDI dedicata; parla con il Live Object Model | `AbletonBridge/` |

Comunicazione: OSC su UDP, solo `127.0.0.1` (app :47120 ⇄ bridge :47121) — [specifica del protocollo](Protocol/OSCSpecification/GC_OSC_PROTOCOL.md).

## Stato: proof of concept (Prima attività, Milestone 1–6)

Come richiesto dal capitolato, l'applicazione completa **non** è stata sviluppata prima di validare la prova.

| Milestone | Stato |
|---|---|
| 1 · Analisi API Live / parametri VST3-AU | ✅ [TECHNICAL_ANALYSIS.md](Documentation/TECHNICAL_ANALYSIS.md) — con le voci **[VERIFICA]** da confermare in Live |
| 2 · Bridge Max for Live | ✅ codice (`gc_bridge.js` + patch) e 12 test su LOM simulato · ⏳ da caricare in Live |
| 3 · Scansione reale del progetto | ✅ implementata (tracce, return, master, rack annidati) · ⏳ da eseguire in Live |
| 4–5 · Controllo di 1 e di 10 istanze | ✅ comando batch + client `gc_poc.py` · ⏳ da eseguire su 10 × Pro-C 2 reali |
| 6 · OSC bidirezionale | ✅ hello/nonce, heartbeat, timeout, anti-duplicati, rate limiting, feedback con origine · verificato su UDP reale con LOM simulato |
| 7–8 · GUI JUCE, 8 macro | ⏳ dopo la validazione in Live. Il motore di calcolo è già pronto e testato (`MacroEngine`, `ParameterScale`) |
| 9 · Absolute / Relative | ✅ motore (lineare, log, curva, discreta, enumerata, clamping per istanza) |
| 10 · Preset e riconnessione | 🟡 riconciliazione identità pronta (`InstanceReconciler`), `projectId` salvato nel Set · preset JSON da fare |
| 11–12 · Stabilità, installer | ⏳ |

**Nessun test è ancora stato eseguito con Ableton Live reale**: questo codice è stato scritto in un ambiente Linux.
I risultati "PASS" in [`Documentation/results/phase_test_MOCK.md`](Documentation/results/phase_test_MOCK.md) vengono da un
LOM simulato (fixture ipotetica) e validano solo protocollo, bridge e calcoli. La procedura reale è in
[TEST_PLAN.md](Documentation/TEST_PLAN.md).

## Struttura

```
GLOBAL_CONTROL/
  GlobalControlApp/
    Core/            ParameterScale  - scala reale di un parametro misurata da str_for_value
    MacroEngine/     MacroEngine     - absolute / relative, inversione, precisione, fine
    MappingEngine/   InstanceReconciler - re-identificazione delle istanze tra sessioni
    OSC/             ProtocolCore    - anti-duplicati, coalescing, blocchi JSON, stato connessione
  AbletonBridge/
    LiveAPI/         gc_bridge.js    - OSC, Device Discovery, Parameter Discovery/Engine (sezioni)
    MaxPatch/        GLOBAL_CONTROL_BRIDGE.maxpat
  Protocol/OSCSpecification/  GC_OSC_PROTOCOL.md
  Tests/
    CoreTests/                 test C++ (ctest "core")
    OSCtests/                  test del bridge su LOM simulato (ctest "bridge"), server UDP di prova
    AbletonIntegrationTests/   gc_poc.py - client per i test reali in Live
  Documentation/  TECHNICAL_ANALYSIS.md, TEST_PLAN.md, results/
```

## Comandi rapidi

```bash
cmake -S GLOBAL_CONTROL -B build-gc && cmake --build build-gc && ctest --test-dir build-gc --output-on-failure
```

```bash
python3 GLOBAL_CONTROL/Tests/AbletonIntegrationTests/gc_poc.py phase-test --device "Pro-C 2" --param Threshold --expect 10 --absolute -12 --delta -2
```

## Limiti noti (dettagli in TECHNICAL_ANALYSIS.md)
- Il LOM non espone produttore del plug-in, formato VST2/VST3, nome/percorso del Set.
- Sono controllabili solo i parametri che Live espone (per plug-in con molti parametri: *Configure*).
- GUI del plug-in e controller esterno non sono distinguibili come origine di una modifica (entrambi `external`).
- Scrivere un parametro automatizzato ne sovrascrive l'automazione, come un movimento manuale.
