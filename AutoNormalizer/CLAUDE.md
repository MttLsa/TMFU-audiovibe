# AutoNormalizer — porta il volume a un livello LUFS a ogni cambio preset, effetto AU/VST3/VST2

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 1.1.3 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: PLUGIN_CODE AtNz, MANUFACTURER Tmfu, BUNDLE_ID com.tmfuaudiovibe.autonormalizer.
- Build e installer: `.github/workflows/autonormalizer.yml` (parte solo per modifiche in `AutoNormalizer/`).

## Dove sta cosa
- `Source/NormalizerEngine.*`: misura e guadagno
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`
- test in `tests/EngineTest.cpp`, `tests/PluginTest.cpp`

## Stato (max 5 righe, sostituire a ogni sessione)
- v1.1.3 unita in `main` il 2026-11-23 durante il riordino della repository.
