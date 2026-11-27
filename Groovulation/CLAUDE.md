# Groovulation — loop mixer a 12 moduli, strumento AU/VST3/Standalone

Regole comuni: `../CLAUDE.md`. Lavora solo dentro questa cartella.

- Versione: 1.2.0 — aggiornala in `CMakeLists.txt` (o nel file di progetto), README e workflow insieme.
- Identità: PLUGIN_CODE Grvl, MANUFACTURER Twnb (non cambiare), BUNDLE_ID com.twinbox.groovulation (non cambiare).
- Build e installer: `.github/workflows/groovulation.yml` (parte solo per modifiche in `Groovulation/`).

## Dove sta cosa
- `Source/PluginProcessor.*`, `Source/Params.*`: processore e parametri
- `Source/dsp/`: motore audio; `Source/gui/`: interfaccia
- `tools/gen_loops.py`: loop di fabbrica
- `third_party/` (signalsmith): non leggere né modificare

## Stato (max 5 righe, sostituire a ogni sessione)
- v1.2.0 (2026-11-27): undo/redo; cutoff HP→BP→LP indipendenti (ON + RES, scala log speculare) in moduli e master; chorus (port CHROMA5YNTH), width, mono; loop markers LS/LE; slice/grid senza click (crossfade tra voci); tolti TEMPO, RES master, NO TRIPLETS.
- Manuale: `tools/gen_manual.py <versione> <cartella screenshot del test>` → `docs/Groovulation_Manuale_v<versione>.pdf`.
- DA FARE: passare a Inter — logo/nome nell'intestazione usa ancora Urbanist Light (`Source/gui/LookAndFeel.*`); poi togliere i file Urbanist da `resources/fonts/`.
