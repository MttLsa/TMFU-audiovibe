# DUALBOX (ex Twinbox) — groovebox ibrida, strumento AU/VST3/Standalone

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 2.17.0 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: PLUGIN_CODE Twbx, MANUFACTURER Twnb (non cambiare), BUNDLE_ID com.tmfuaudiovibe.dualbox.
- Build e installer: `.github/workflows/dualbox.yml` (parte solo per modifiche in `DUALBOX/`).

## Dove sta cosa
- `Source/PluginProcessor.*`, `Source/Params.*`, `Source/Presets.*`: processore, parametri, preset
- `Source/dsp/`: motori (MicroEngine = sezione A, LimeEngine/LimeFX = sezione B, ModMatrix, SampleLibrary)
- `Source/gui/`: pannelli (MicroSection, LimeSection, GlobalPanel, LfoPanel, PresetBrowser)
- `resources/presets_A.json`, `presets_B.json` generati da `tools/gen_presets.py`; manuale da `tools/make_manual.py`
- `third_party/` (signalsmith): non leggere né modificare

## Stato (max 5 righe, sostituire a ogni sessione)
- v2.17.0 unita in `main` il 2026-11-23 durante il riordino della repository.
