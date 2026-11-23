# FamPat (famous patches, ex SynthZilla → 5ynth) — player di 200 suoni di synth famosi, strumento AU/VST3/Standalone

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 1.0.0 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: PLUGIN_CODE Fmpt, MANUFACTURER Tmfu, BUNDLE_ID com.tmfuaudiovibe.fampat.
- Build e installer: `.github/workflows/fampat.yml` (parte solo per modifiche in `FamPat/`).

## Dove sta cosa
- `Source/PlayerEditor.*`: interfaccia del player; `Source/PluginProcessor.*`, `Source/Params.*`
- `Source/dsp/`: motori synth e circuiti
- `resources/presets.json` generato da `tools/gen_presets.py` (livelli in `tools/levels.json`); elenco in `docs/PRESETS.md`
- manuale da `tools/make_manual.py`

## Stato (max 5 righe, sostituire a ogni sessione)
- v1.0.0 unita in `main` il 2026-11-23 durante il riordino della repository.
- DA FARE: passare a Inter — il manuale (`tools/make_manual.py`) usa ancora Urbanist; poi togliere i file Urbanist da `resources/fonts/`.
