# Convenzioni del repository

## Plugin nuovi (da SynthZilla, ex Twinsynth, in poi)
- Produttore mostrato nel browser della DAW: **TMFU audiovibe**. In `juce_add_plugin`:
  `COMPANY_NAME "TMFU audiovibe"`, `PLUGIN_MANUFACTURER_CODE Tmfu`, `BUNDLE_ID "com.tmfuaudiovibe.<nomeplugin>"`
  (ogni plugin ha il proprio `PLUGIN_CODE` di 4 caratteri, unico).
- I plugin già pubblicati prima del cambio (Twinbox, Groovulation) hanno `COMPANY_NAME "TMFU audiovibe"` ma tengono il
  vecchio `PLUGIN_MANUFACTURER_CODE Twnb`, così i progetti salvati nelle DAW continuano a ritrovarli: non cambiarlo.
- Installer macOS `.pkg` universale (Intel x86_64 + Apple Silicon arm64), costruito dalla CI sul runner `macos-15-intel`
  con `packaging/macos/make_pkg.sh` (modello: `SynthZilla/packaging/macos/make_pkg.sh`): AU in /Library/Audio/Plug-Ins/Components,
  VST3 in /Library/Audio/Plug-Ins/VST3, app Standalone in /Applications; controlla con `lipo` che i binari siano universali
  e che l'AU si presenti come "TMFU audiovibe: <nome>".
- Validazione AU in CI: `auval -v aumu <PLUGIN_CODE> Tmfu` per gli strumenti; per gli effetti che ricevono MIDI (music effect,
  `AU_MAIN_TYPE kAudioUnitType_MusicEffect`, es. Microvibe) `auval -v aumf <PLUGIN_CODE> Tmfu`.

## Font e lingua dell'interfaccia
- Font unico per tutte le scritte di ogni plugin e documento: **Urbanist** (The Urbanist Project Authors, SIL OFL 1.1),
  geometric sans serif: **Light** per i testi, **SemiBold** per titoli, pulsanti e tutto ciò che è in grassetto.
  Va incorporato nel binario (`Urbanist-Light.ttf` + `Urbanist-SemiBold.ttf` in `resources/fonts/`, aggiunti a
  `juce_add_binary_data`, con `OFL.txt` accanto) e usato tramite `Typeface::createSystemTypefaceFor` +
  `setDefaultSansSerifTypeface`/`getTypefaceForFont` nel LookAndFeel, così anche menu, popup e tooltip lo usano.
  Modello: `Microvibe/Source/PluginEditor.cpp` (`ui::typeface`). Anche i manuali PDF usano Urbanist.
- Nomi di moduli, parametri, pulsanti e titoli dei pannelli in **inglese**; restano in italiano solo menu contestuali,
  tooltip, suggerimenti e testi di aiuto.
