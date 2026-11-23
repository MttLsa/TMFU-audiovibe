# Microvibe — glitch, texture, percussioni ed effetti, music effect AU (aumf)/VST3

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 1.1.1 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: PLUGIN_CODE Mcvb, MANUFACTURER Tmfu, BUNDLE_ID com.tmfuaudiovibe.microvibe.
- Build e installer: `.github/workflows/microvibe.yml` (parte solo per modifiche in `Microvibe/`).

## Dove sta cosa
- `Source/dsp/`: Engine, Glitch, Texture, ModalPerc, Fx, Shepard, Tuning, Lanes, Modulators
- `Source/PluginEditor.cpp`: modello del font (`ui::typeface`)
- `resources/presets.json` da `tools/gen_presets.py`; manuale da `tools/make_manual.py`
- `docs/PROGETTO.md`: progetto dettagliato (leggi solo la sezione che serve)
- `third_party/MTS-ESP/`: non modificare

## Stato (max 5 righe, sostituire a ogni sessione)
- v1.1.1 unita in `main` il 2026-11-23 durante il riordino della repository.
- DA FARE: passare a Inter — logo/nome nell'intestazione e manuale (`tools/make_manual.py`) usano ancora Urbanist SemiBold; poi togliere i file Urbanist da `resources/fonts/`.
