# Bandmates — session player per Ableton Live (device Max for Live, JavaScript)

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 1.0.2 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: device .amxd (non è un plugin JUCE); produttore TMFU audiovibe.
- Build e installer: `.github/workflows/bandmates.yml` (parte solo per modifiche in `Bandmates/`).

## Dove sta cosa
- `engine/`: logica musicale (drummer, bass, chords, keys, theory, groove)
- `max/`: adattatori per Max; `devices/`: .amxd/.maxpat generati da `tools/build.js` (non modificare a mano)
- test: `tests/*.test.js` (`npm test`)
- `docs/PROGETTO.md`: progetto (leggi solo la sezione che serve)

## Stato (max 5 righe, sostituire a ogni sessione)
- v1.0.2 unita in `main` il 2026-11-23 durante il riordino della repository.
