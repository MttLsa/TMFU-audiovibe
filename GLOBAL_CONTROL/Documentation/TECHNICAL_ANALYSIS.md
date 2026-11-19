# GLOBAL CONTROL — Milestone 1: analisi tecnica

Stato: analisi completata sulla base della documentazione pubblica del Live Object Model (LOM) e di Max for Live.
I punti marcati **[VERIFICA]** sono comportamenti che dipendono dalla versione di Live / dal plug-in e che vanno
confermati sul Mac con il client di prova (`Tests/AbletonIntegrationTests/gc_poc.py`, vedi `TEST_PLAN.md`).
Nessun risultato qui sotto è stato ottenuto con Ableton Live reale: questo repository è stato sviluppato in un
ambiente Linux senza Live.

## 1. Architettura scelta

```
GLOBAL CONTROL.app (C++17/JUCE, nessun audio)            Ableton Live
┌──────────────────────────────────┐   OSC/UDP   ┌──────────────────────────────────────────┐
│ GUI · MacroEngine · Mapping      │ 127.0.0.1   │ traccia MIDI "GLOBAL CONTROL"            │
│ ParameterScale · Reconciler      │◄──47120─────│  GLOBAL_CONTROL_BRIDGE.amxd (MIDI Effect)│
│ ProtocolCore · PresetEngine      │──47121─────►│   [udpreceive]→[js gc_bridge.js]→[udpsend]│
└──────────────────────────────────┘             │        │ LiveAPI (LOM, thread principale) │
                                                 │        ▼                                 │
                                                 │  tracce → device → (rack → catene)* →    │
                                                 │  DeviceParameter.value  (Pro-C 2 × 10…)  │
                                                 └──────────────────────────────────────────┘
```

- **Un solo bridge per Set**: il LOM è globale (`live_set`), un device su una qualunque traccia vede e modifica
  tutti i device del progetto. Non serve nulla sulle singole tracce, i plug-in restano dove sono, originali.
- La traccia del bridge non ha strumenti; il device fa solo passare il MIDI (`midiin → midiout`), non tocca l'audio.
- Il bridge usa l'oggetto `[js]` (JavaScript ES5 con `LiveAPI`) invece di una rete di `live.path/live.object/live.observer`:
  la scansione ricorsiva di tracce/rack e la gestione di centinaia di osservatori sono molto più semplici e
  verificabili in codice (il bridge ha 12 test automatici su un LOM simulato). `live.path`/`live.object`/`live.observer`
  sono gli stessi servizi esposti come oggetti grafici; `LiveAPI` in JS è la via documentata per usarli da script.

## 2. Cosa espone il Live Object Model (API pubblica)

| Esigenza | LOM | Note |
|---|---|---|
| Tracce audio/MIDI/return/master | `live_set tracks N`, `return_tracks N`, `master_track` | `has_midi_input`, `is_foldable` (gruppo), `name`, `color` |
| Dispositivi | `track devices N` | `name` (rinominabile dall'utente), `class_name`, `class_display_name`, `type`, `is_active` |
| Plug-in di terze parti | `class_name` = `PluginDevice` (VST2/VST3) o `AuPluginDevice` (AU) | **VST2 e VST3 non sono distinguibili** dal LOM |
| Rack annidati | `can_have_chains`, `chains N`, `return_chains N`, poi di nuovo `devices N` | Drum Rack: le catene dei pad sono in `chains` |
| Parametri | `device parameters N` → `DeviceParameter` | `name`, `original_name`, `value`, `min`, `max`, `is_quantized`, `value_items`, `state`, `is_enabled`, `automation_state`, `default_value` (solo non quantizzati), funzione `str_for_value(v)` |
| Identità | `id` di ogni oggetto | **valido solo per la sessione**: cambia alla riapertura del Set |
| Osservazione | `LiveAPI.property = "value"` / `live.observer` | callback sul thread principale; dentro una notifica non si possono modificare valori |
| Produttore | — | **non esposto**: `manufacturer` è sempre `null` |
| Nome/percorso del Set | — | **non esposto**: si usa un `projectId` salvato nel Set dal bridge |

## 3. Parametri dei plug-in VST3 / AU di terze parti

- Live espone un plug-in come `PluginDevice`/`AuPluginDevice` con una lista di `DeviceParameter`. Il parametro 0 è
  "Device On". I valori dei parametri dei plug-in sono **normalizzati 0…1** (`min`=0, `max`=1): Live non conosce
  la scala interna del plug-in (dB, ms, Hz).
- Il testo mostrato (es. "-12.0 dB") si ottiene con `str_for_value(value)`, che chiede la stringa al plug-in.
  GLOBAL CONTROL **campiona questa funzione** su 33–65 punti e ne ricava la scala reale (lineare, logaritmica,
  curva, discreta, enumerata) e l'inversione testo→valore: nessuna tabella statica per FabFilter o altri.
- **Quali parametri compaiono** dipende da Live: per i plug-in con molti parametri Live non li elenca tutti
  automaticamente; l'utente sceglie quali esporre con **Configure** nel titolo del device, e la scelta viene
  salvata nel Set. **[VERIFICA]** soglia e comportamento esatti nella versione di Live installata e numero di
  parametri di Pro-C 2 esposti di default: comando `gc_poc.py params --device "Pro-C 2" --json proc2.json`.
- Un parametro che non compare nella lista **non è accessibile** da Max for Live: l'app lo mostra come
  "non esposto da Live — usare Configure" e non lo simula mai.
- `is_enabled = 0` (parametro mappato a una macro di rack o modulato) → non scrivibile: il bridge risponde
  `E_NOT_ENABLED` e l'app lo mostra bloccato.
- Tracce congelate (Freeze): i loro device non sono modificabili. **[VERIFICA]** l'errore restituito.

## 4. Fattibilità dei requisiti

| Requisito | Fattibile | Come / limite |
|---|---|---|
| Scansione completa, rack annidati, istanze duplicate | Sì | ricorsione su `chains`/`return_chains` (profondità max 6), id per istanza + `key` di posizione |
| Lettura/scrittura parametri su N istanze con un gesto | Sì | un pacchetto `/global/parameter/set` con N coppie; il bridge applica in un solo tick |
| Absolute mode | Sì | inversione della curva misurata, limitata al range di ogni istanza |
| Relative mode con scale corrette | Sì | dominio additivo (lineare), moltiplicativo (log/curva positiva), a passi (discreti/enum); per scale sconosciute offset normalizzato marcato "approssimato" |
| Feedback da GUI del plug-in | Sì, se Live notifica | osservatore su `value`; **[VERIFICA]** alcuni plug-in notificano Live solo a fine gesto o con ritardo |
| Distinguere GC / automazione / altro | Parziale | GC: eco del valore scritto entro 1,5 s; automazione: `automation_state`=1 e trasporto in play; **GUI del plug-in e controller esterno non sono distinguibili** (il LOM non riporta l'origine) → entrambi `external` |
| Prevenzione feedback loop | Sì | le eco `gc` aggiornano solo il display; l'app non rimanda mai un valore ricevuto |
| Modifiche al progetto | Sì | osservatori su `tracks`, `return_tracks` e `devices` di ogni traccia → `structure_changed` → nuova scansione + riconciliazione. Modifiche *interne* ai rack (device aggiunto in una catena) non sono osservate: coperte dalla scansione manuale/periodica |
| Identità tra sessioni | Sì, con riconciliazione | classe + plug-in obbligatori; posizione (`t3/d1/c0/d0`), nome traccia, nome device personalizzato, ordinale sulla traccia, stessi parametri esposti (hash) come prove a punteggio; parità → *ambiguo*, mai associato in silenzio; il parametro mappato è poi ritrovato per nome sul device |
| Nome del progetto | No (LOM) | `projectId` generato dal bridge e salvato nel Set tramite `[pattr gc_project_id]` di tipo blob "Stored Only" **[VERIFICA persistenza]**; l'utente assegna un nome leggibile nell'app |
| Produttore del plug-in | No (LOM) | campo vuoto; si mostra `class_display_name` |
| Solo localhost | Sì, con una nota | app legata a 127.0.0.1; `[udpsend 127.0.0.1]`; `[udpreceive]` di Max ascolta su tutte le interfacce → protetto dal nonce (vedi protocollo). Alternativa: `[node.script]` con `dgram` legato a 127.0.0.1 |

## 5. Comportamenti di Live da conoscere

1. **Annulla (Undo)**: ogni scrittura via LOM può creare un passo di annullamento. Il coalescing limita il numero di
   scritture; **[VERIFICA]** se `Song.begin_undo_step/end_undo_step` (Live 11+) permette di raggruppare un gesto in
   un solo passo — previsto in Milestone 8.
2. **Automazione**: scrivere un parametro automatizzato lo mette in stato "sovrascritto" (come muoverlo a mano);
   l'app avvisa quando `automationState`=1 prima di controllarlo.
3. **Thread**: tutte le chiamate LiveAPI avvengono sul thread principale (bassa priorità) di Max, mai sull'audio
   thread; le scritture partono da un `Task` perché il LOM vieta modifiche dentro le notifiche.
4. **Precisione**: OSC/Max trasportano float32 (≈7 cifre): sufficiente per valori normalizzati 0…1.
5. **Carico**: la scansione elabora una traccia per tick; le notifiche verso l'app sono limitate a 30 Hz per parametro.

## 6. Alternative valutate e scartate

| Alternativa | Motivo dello scarto |
|---|---|
| Wrapper/plug-in host che ospita le istanze | vietato dai requisiti (non sostituire i device originali) |
| Remote Script Python (Control Surface) | API non documentata ufficialmente per terze parti; installazione più fragile; il M4L è supportato |
| MIDI mapping verso i parametri | un CC per parametro per istanza, mappatura manuale, nessuna lettura dei valori né scansione |
| Un bridge per traccia | inutile: il LOM è accessibile da un solo device |

## 7. Struttura del bridge rispetto al layout richiesto

Max `[js]` carica un solo file; le aree logiche richieste (`OSC/`, `DeviceDiscovery/`, `ParameterEngine/`, `LiveAPI/`)
sono sezioni di `AbletonBridge/LiveAPI/gc_bridge.js` (intestazioni `OSC input`, `DEVICE DISCOVERY ENGINE`,
`PARAMETER DISCOVERY ENGINE`, `PARAMETER ENGINE`). La patch è in `AbletonBridge/MaxPatch/`.
