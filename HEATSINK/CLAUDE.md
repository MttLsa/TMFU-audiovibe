# HEATSINK — distorsione / filtro / EQ analogici, effetto AU/VST3 (VST2 opzionale)

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 1.0.0 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: PLUGIN_CODE Htsk, MANUFACTURER Tmfu, BUNDLE_ID com.tmfuaudiovibe.heatsink.
- Build e installer: `.github/workflows/heatsink.yml` (parte solo per modifiche in `HEATSINK/`).

## Dove sta cosa
- `Source/DSP/`: stadi di distorsione, filtro, EQ
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`
- manca ancora il README

## Stato (max 5 righe, sostituire a ogni sessione)
- v1.0.0 unita in `main` il 2026-11-23 durante il riordino della repository.
- DA FARE: passare a Inter — tutta l'interfaccia usa ancora Urbanist (Light/SemiBold); poi togliere i file Urbanist da `resources/fonts/`.
