# GLOBAL CONTROL — Protocollo OSC v1

Trasporto: OSC 1.0 su UDP, **solo 127.0.0.1**.

| Lato | Porta | Oggetto |
|---|---|---|
| GLOBAL CONTROL.app (riceve) | **47120** | socket legato a `127.0.0.1` (mai `0.0.0.0`) |
| GLOBAL_CONTROL_BRIDGE.amxd (riceve) | **47121** | `[udpreceive 47121]` |

Tipi OSC usati: `i` (int32), `f` (float32), `s` (stringa). Max invia un numero intero come `i` anche quando
rappresenta un float (es. `0` invece di `0.0`): **chi riceve deve accettare `i` e `f` per ogni campo numerico.**

## 1. Envelope

Ogni messaggio inizia con un envelope fisso; il payload segue.

| Direzione | Envelope |
|---|---|
| app → bridge | `proto:i` `appSession:s` `seq:i` `token:s` |
| bridge → app | `proto:i` `bridgeSession:s` `seq:i` |

- `proto` = 1. Versione diversa → `/global/error E_PROTO`.
- `appSession` / `bridgeSession`: id casuale a 64 bit (hex) generato all'avvio di ciascun processo. Un cambio di
  `bridgeSession` significa che il bridge è stato ricaricato (Set riaperto, device duplicato, Live riavviato).
- `seq`: contatore crescente per mittente. Il ricevente scarta i duplicati (stessa sessione + stesso `seq`,
  finestra di 512/1024 messaggi).
- `token`: il **nonce** restituito dal bridge nella risposta a `/global/hello`. Vuoto solo per `/global/hello`.
  Il nonce viaggia solo verso 127.0.0.1, quindi un host della rete locale che riuscisse a raggiungere
  `[udpreceive]` (che in Max ascolta su tutte le interfacce) non può comandare il bridge (`E_AUTH`).

Le risposte a una richiesta riportano come primo campo del payload `reqSeq` = `seq` della richiesta.

## 2. Trasferimenti in blocchi (JSON)

Elenco dispositivi, parametri e curve viaggiano come un documento JSON spezzato in blocchi:
`… reqSeq:i key:s|i part:i total:i chunk:s`. Ogni `chunk` è un frammento del JSON **codificato con
`encodeURIComponent`** (così nessun simbolo Max contiene `{ } , ;`); il ricevente decodifica ogni blocco,
li ordina per `part` e li concatena. Blocco = max 1000 caratteri JSON prima della codifica (< 8 KB per pacchetto).
Se un trasferimento non si completa entro 3 s, l'app ripete la richiesta.

## 3. Messaggi

### `/global/hello`
- app → bridge: `… token="" clientName:s clientVersion:s`
- bridge → app: `reqSeq:i nonce:s projectId:s liveVersion:s bridgeVersion:s bridgeDeviceId:s`
  Il bridge lo invia anche **spontaneamente** al caricamento (`reqSeq=0`, `nonce=""`): l'app risponde con un hello.
  Un hello da una nuova `appSession` azzera osservatori e anti-duplicati del bridge.

### `/global/heartbeat`
- bridge → app ogni 1 s: `uptimeMs:i appConnected:i projectId:s`
- app → bridge ogni 1 s: nessun payload.
- Timeout: 4 s senza traffico → stato *Lost*, l'app ritenta `/global/hello` ogni secondo (riconnessione automatica,
  qualunque sia l'ordine di avvio di Live e dell'app).

### `/global/scan/request` → `/global/device/list` (blocchi) + `/global/scan/complete`
- richiesta: `scanId:i`
- `/global/device/list`: `reqSeq scanId part total chunk` → JSON `{scanId, projectId, tracks[], devices[]}`
- `/global/scan/complete`: `reqSeq:i scanId:i trackCount:i deviceCount:i durationMs:i`

Traccia: `id key kind(track|return|master) index name color isMidi isGroup isBridgeTrack`

Dispositivo: `id key trackId trackName trackKey trackKind position depth rackPath name className
classDisplayName type isPlugin format(VST|AU|Live) manufacturer(null) isActive canHaveChains paramCount
paramNamesHash isBridge`

`key` è il percorso stabile all'interno del Set: `t3/d1` (traccia 3, device 1), `t4/d0/c1/d0` (dentro la catena 1
di un rack), `r0/…` (return), `m/…` (master). `paramNamesHash` = FNV-1a dei nomi originali dei parametri esposti.
La scansione procede una traccia per tick dello scheduler di Max, per non bloccare l'interfaccia di Live.

### `/global/device/parameters`
- richiesta: `deviceId:i`
- risposta (blocchi): `reqSeq deviceId part total chunk` → JSON `{deviceId, name, className, classDisplayName, parameters[]}`

Parametro: `id index deviceId name originalName value min max normalized isQuantized valueItems[] display state
isEnabled automationState defaultValue`
(`state`: 0 attivo, 1 inattivo, 2 disabilitato; `isEnabled`=0 quando è controllato da una macro di rack/modulazione;
`automationState`: 0 nessuna, 1 automazione attiva, 2 automazione sovrascritta.)

### `/global/parameter/describe` → `/global/parameter/curve` (blocchi)
- richiesta: `paramId:i samples:i [lo:f hi:f]`
- risposta: `reqSeq paramId part total chunk` → JSON `{paramId, min, max, lo, hi, isQuantized, samples:[[raw, "testo"], …]}`
  I testi sono quelli che il parametro stesso restituisce (`str_for_value`): da questi l'app ricava la scala reale
  (lineare, logaritmica, curva, discreta, enumerata). `lo/hi` permettono di raffinare un'inversione in un intervallo ristretto.

### `/global/parameter/set`
- app → bridge: `txn:i n:i (paramId:i value:f) × n` — un solo pacchetto per tutte le istanze di un movimento di macro.
- Il bridge accoda e **coalesce** (per parametro vince l'ultimo valore), applica dopo 5 ms da un Task (le modifiche
  al LOM non sono ammesse dentro una notifica), limita al range `[min,max]`, arrotonda i quantizzati, rifiuta i
  parametri non abilitati (`E_NOT_ENABLED`) o scomparsi (`E_NOT_FOUND`).
- Conferma: `/global/parameter/value` con `source="gc"` e lo stesso `txn`.

### `/global/parameter/get`
- `n:i paramId:i × n` → un `/global/parameter/value` (`source="query"`) per ciascuno.

### `/global/parameter/observe`
- `on:i n:i paramId:i × n` → avvia/ferma l'osservazione (LiveAPI `property = "value"`), poi invia il valore attuale.

### `/global/parameter/value` (bridge → app)
`paramId:i value:f display:s(URI) source:s txn:i`

| source | significato |
|---|---|
| `gc` | eco di una scrittura di GLOBAL CONTROL (stesso valore entro 1,5 s) — l'app la usa come conferma, **non** la rimanda indietro (anti feedback-loop) |
| `automation` | l'automazione del parametro è attiva e il trasporto è in play |
| `external` | qualunque altra origine: GUI del plug-in, device view di Live, controller MIDI/Push, script remoti (il LOM non le distingue) |
| `query` | risposta a get/observe |

Le notifiche sono coalescenti: al massimo una per parametro ogni 33 ms (~30 Hz).

### `/global/instance/status` (bridge → app)
- `structure_changed 0`: tracce o device aggiunti/rimossi/spostati (debounce 400 ms) → l'app riesegue la scansione e riconcilia.
- `param_removed paramId`: un parametro osservato non esiste più.

### `/global/project/setid` (app → bridge)
- `projectId:s`: imposta l'identificativo di progetto memorizzato nel Set (es. per assegnare un id noto).

### `/global/error` (bridge → app)
`reqSeq:i code:s message:s(URI)`

| code | caso |
|---|---|
| `E_PROTO` | versione o formato non validi |
| `E_AUTH` | sessione/nonce mancanti: inviare `/global/hello` |
| `E_UNKNOWN` | indirizzo sconosciuto |
| `E_NOT_FOUND` | device o parametro inesistente (rimosso) |
| `E_NOT_ENABLED` | parametro non modificabile (mappato a una macro di rack, modulato) |
| `E_BUSY` | scansione già in corso |
| `E_RATE` | più di 3000 messaggi/s: i successivi vengono scartati |
| `E_INTERNAL` | eccezione nel bridge |

## 4. Limiti di frequenza

| Percorso | Limite |
|---|---|
| app → bridge, scritture | coalescenza per parametro, un batch ogni 16 ms (≈ 60 Hz) |
| bridge → Live | coalescenza per parametro, applicazione ogni 5 ms |
| bridge → app, valori | coalescenza per parametro, ≤ 30 Hz |
| bridge, protezione flood | 3000 msg/s per sessione |
