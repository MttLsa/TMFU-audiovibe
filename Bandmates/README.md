# BANDMATES — session player per Ableton Live (Max for Live)

Musicisti virtuali che suonano parti MIDI per te in Ableton Live, seguendo accordi e struttura del brano:
batterista acustico, batterista elettronico, bassista, tastierista e una traccia accordi. Produttore **TMFU audiovibe**.

| Device | Generi / funzione |
|---|---|
| **Bandmates Chords** | Traccia accordi: progressioni scritte a testo o da preset, Chord ID dai clip MIDI |
| **Bandmates Drums** | Soul, Jazz, R&B, Rock, Pop, Funk |
| **Bandmates Beats** | House, Tech House, Techno, Microhouse, Rominimal: ogni pezzo del kit con on/off e densità |
| **Bandmates Bass** | Soul, Jazz, R&B, Rock, Pop, Funk, House, Tech House, Techno, Microhouse/Rominimal |
| **Bandmates Keys** | Soul, Jazz, R&B, Rock, Pop, Funk, House, Tech House, Techno, Microhouse/Rominimal |

Progetto completo, corrispondenza con i Session Player di Logic Pro, note sui diritti e sull'AI: **[docs/PROGETTO.md](docs/PROGETTO.md)**.

## Installazione
1. Serve **Live 11/12 Suite** (o Standard con Max for Live). Consigliato Live 12.
2. Copia l'intera cartella [`devices/`](devices) (i `.amxd` **e** i file `bm_*.js` insieme) nella User Library di Live,
   ad esempio `User Library/Presets/MIDI Effects/Max MIDI Effect/Bandmates/`.
3. Nel browser di Live trascina i device su tracce MIDI.

## Uso in 1 minuto
1. Traccia MIDI "Accordi" con **Bandmates Chords** (anche senza strumento, o con un pad): scrivi `Am7 | Dm7 | G7 | Cmaj7`
   (oppure scegli una progressione e premi *Usa questa progressione*), seleziona uno slot e premi **Scrivi nel clip**.
2. Traccia con un Drum Rack + **Bandmates Drums** (o **Beats**): seleziona lo slot della stessa scena e premi **Genera**.
3. Traccia con un basso + **Bandmates Bass**, traccia con un piano + **Bandmates Keys**: **Genera**.
4. Muovi il pad XY, Fill, Swing…: con **Auto** acceso il clip selezionato si rigenera subito. **Nuova variazione** cambia esecuzione.
5. Dai nomi alle scene (Intro, Strofa, Ritornello, Build, Drop, Break, Outro) e premi **Tutte le scene**: ogni sezione viene suonata
   in modo diverso. In Arrangement usa i locator e il pulsante **Arrangement**.

Ogni clip ricorda le sue impostazioni: selezionalo e il device le ricarica.

## Sviluppo
```bash
node --test                 # motore + device su una sessione di Live simulata
node tools/build.js         # rigenera devices/ (da fare dopo ogni modifica a engine/ o max/)
node tools/build.js --check # verifica che devices/ sia aggiornato (usato dalla CI)
```
Il motore (`engine/`) è JavaScript ES5 puro: lo stesso codice gira in Node per i test e nell'oggetto `js` di Max.

> Logic Pro è un marchio di Apple Inc. Bandmates è un progetto indipendente che riprende il *funzionamento* dei Session Player;
> non contiene codice, grafica, suoni, pattern o modelli di Apple.
