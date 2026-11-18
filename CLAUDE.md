# Convenzioni del repository

## Plugin nuovi (da Twinsynth in poi)
- Produttore mostrato nel browser della DAW: **TMFU audiovibe**. In `juce_add_plugin`:
  `COMPANY_NAME "TMFU audiovibe"`, `PLUGIN_MANUFACTURER_CODE Tmfu`, `BUNDLE_ID "com.tmfuaudiovibe.<nomeplugin>"`
  (ogni plugin ha il proprio `PLUGIN_CODE` di 4 caratteri, unico).
- Installer macOS `.pkg` universale (Intel x86_64 + Apple Silicon arm64), costruito dalla CI sul runner `macos-15-intel`
  con `packaging/macos/make_pkg.sh` (modello: `Twinsynth/packaging/macos/make_pkg.sh`): AU in /Library/Audio/Plug-Ins/Components,
  VST3 in /Library/Audio/Plug-Ins/VST3, app Standalone in /Applications; controlla con `lipo` che i binari siano universali
  e che l'AU si presenti come "TMFU audiovibe: <nome>".
- Validazione AU in CI: `auval -v aumu <PLUGIN_CODE> Tmfu`.
