# GLOBAL CONTROL — app macOS + bridge Max for Live: 8 macro che muovono lo stesso parametro su più plug-in in Ableton Live

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 1.0.0 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: app (non è un plugin); BUNDLE_ID com.tmfuaudiovibe.globalcontrol.
- Build e installer: `.github/workflows/globalcontrol.yml` (parte solo per modifiche in `GLOBAL_CONTROL/`).

## Dove sta cosa
- `GlobalControlApp/`: app (Core, GUI, MacroEngine, MappingEngine, OSC, PresetEngine)
- `AbletonBridge/`: device Max for Live (generato da `tools/make_amxd.py`)
- `Protocol/OSCSpecification/`: protocollo OSC tra app e bridge
- `Documentation/`: guida, analisi tecnica, piano di test (leggi solo se serve)

## Stato (max 5 righe, sostituire a ogni sessione)
- v1.0.0 unita in `main` il 2026-11-23 durante il riordino della repository.
