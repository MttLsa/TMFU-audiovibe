# TMFU audiovibe — regole comuni (leggere per primo)

Ogni prodotto sta nella SUA cartella, con un proprio `CLAUDE.md` (regole e stato di quel prodotto).

| Cartella | Prodotto | Tipo |
|---|---|---|
| `DUALBOX/` | DUALBOX (ex Twinbox) | strumento AU/VST3 |
| `Groovulation/` | Groovulation | strumento AU/VST3 |
| `FamPat/` | FamPat (ex SynthZilla → 5ynth) | strumento AU/VST3 |
| `Microvibe/` | Microvibe | music effect AU/VST3 |
| `AutoNormalizer/` | AutoNormalizer | effetto AU/VST3/VST2 |
| `HEATSINK/` | HEATSINK | effetto AU/VST3 |
| `GLOBAL_CONTROL/` | GLOBAL CONTROL | app macOS + bridge Max for Live |
| `Bandmates/` | Bandmates | device Max for Live |

## Risparmio token (obbligatorio)
- Lavora SOLO nella cartella del prodotto indicato nel prompt. Non aprire, cercare o leggere le altre cartelle.
- Leggi il `CLAUDE.md` della cartella, poi solo i file necessari: usa grep/ricerche mirate, non aprire file interi grandi.
- Non leggere mai PDF, immagini, zip, font o `third_party/` se non richiesto.
- Modifiche mirate (edit), mai riscrivere file interi esistenti.
- Output in chat: solo file toccati, esito build/test, problemi aperti. Niente riepiloghi del codice.
- A fine lavoro: aggiorna la sezione "Stato" del `CLAUDE.md` del prodotto (max 5 righe, sostituisci, non accumulare) → commit → pull request verso `main`.

## Produttore e identità dei plugin
- Produttore nel browser della DAW: **TMFU audiovibe**. In `juce_add_plugin`:
  `COMPANY_NAME "TMFU audiovibe"`, `PLUGIN_MANUFACTURER_CODE Tmfu`, `BUNDLE_ID "com.tmfuaudiovibe.<nomeplugin>"`,
  `PLUGIN_CODE` di 4 caratteri unico (vedi tabella codici sotto, aggiornala per ogni plugin nuovo).
- DUALBOX e Groovulation, già pubblicati prima del cambio, tengono `PLUGIN_MANUFACTURER_CODE Twnb` (e DUALBOX anche
  `PLUGIN_CODE Twbx`), così i progetti salvati nelle DAW li ritrovano: **non cambiarli**.
- Codici in uso: Twbx (DUALBOX), Grvl (Groovulation), Fmpt (FamPat), Mcvb (Microvibe), AtNz (AutoNormalizer),
  Htsk (HEATSINK), Chr5 (CHROMA5YNTH, in arrivo). Non riusarli.

## Build e installer
- Build ufficiali e installer SOLO via GitHub Actions: un workflow per prodotto in `.github/workflows/<prodotto>.yml`,
  con `working-directory: <Cartella>` e `paths: ["<Cartella>/**"]`, così parte solo quando cambia quel prodotto.
  (Il Mac dello sviluppatore è un MacBook Pro 2012 Intel con hotspot: niente build/download pesanti in locale.)
- Installer macOS `.pkg` universale (x86_64 + arm64) sul runner `macos-15-intel` con `<Cartella>/packaging/macos/make_pkg.sh`
  (modello: `FamPat/packaging/macos/make_pkg.sh`): AU in /Library/Audio/Plug-Ins/Components, VST3 in /Library/Audio/Plug-Ins/VST3,
  Standalone in /Applications; controlla con `lipo` i binari universali e che l'AU si presenti come "TMFU audiovibe: <nome>".
- Validazione AU: `auval -v aumu <PLUGIN_CODE> Tmfu` (strumenti); `auval -v aumf <PLUGIN_CODE> Tmfu` (music effect, es. Microvibe).
- Release: tag con prefisso del prodotto, es. `dualbox-v2.17.0`, `fampat-v1.0.0`.

## Font e lingua dell'interfaccia
- Font per tutte le scritte: **Inter** (SIL OFL 1.1) — **Regular** testi, **SemiBold** titoli/pulsanti/grassetto.
  Incorporato nel binario (`resources/fonts/Inter-Regular.ttf` + `Inter-SemiBold.ttf` in `juce_add_binary_data`, licenza OFL accanto),
  usato via `Typeface::createSystemTypefaceFor` + `setDefaultSansSerifTypeface`/`getTypefaceForFont` nel LookAndFeel.
  Modello: `Microvibe/Source/PluginEditor.cpp` (`ui::typeface`).
- **Urbanist non si usa più**, nemmeno per logo e nome del plugin nell'intestazione (unica eccezione: logo FamPat in Rostex Regular).
  I plugin che lo usano ancora vanno convertiti a Inter (vedi "Stato" nel loro `CLAUDE.md`); anche i manuali PDF usano Inter.
- Nomi di moduli, parametri, pulsanti e titoli in **inglese**; in italiano solo menu contestuali, tooltip, aiuti e manuali.

