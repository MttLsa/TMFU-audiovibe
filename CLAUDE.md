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
