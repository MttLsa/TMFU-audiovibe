#!/bin/bash
# Builds the macOS installer AutoNormalizer-<version>-macOS.pkg
# (AU + VST3, plus VST2 if it was built). Universal binaries: Intel + Apple Silicon.
#
# Usage (from the AutoNormalizer folder, after building with CMake):
#   ./packaging/macos/make_pkg.sh [version]
#
# Optional signing / notarization (without these the plug-ins get an ad-hoc signature and the
# installer is unsigned: macOS then asks to confirm with right-click > Open on the .pkg):
#   DEV_ID_APP="Developer ID Application: Name (TEAMID)"
#   DEV_ID_INSTALLER="Developer ID Installer: Name (TEAMID)"
#   NOTARY_PROFILE="profile-name"   # created once with: xcrun notarytool store-credentials
set -euo pipefail
cd "$(dirname "$0")/../.."

VERSION="${1:-$(sed -n 's/^project(AutoNormalizer VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)}"
NAME="AutoNormalizer"
ID="com.tmfuaudiovibe.autonormalizer"
ART="build/${NAME}_artefacts/Release"
OUT="build/installer"
PKG="$OUT/${NAME}-${VERSION}-macOS.pkg"

rm -rf "$OUT"
mkdir -p "$OUT/res"

# ---------------------------------------------------------------- collect formats
FORMATS=()
add_format() { # key  source-bundle  install-folder
  local key="$1" src="$2" dest="$3"
  if [ -e "$src" ]; then
    mkdir -p "$OUT/$key$dest"
    cp -R "$src" "$OUT/$key$dest/"
    FORMATS+=("$key")
  else
    echo "note: $src not found, skipping $key"
  fi
}
add_format au   "$ART/AU/${NAME}.component" "/Library/Audio/Plug-Ins/Components"
add_format vst3 "$ART/VST3/${NAME}.vst3"    "/Library/Audio/Plug-Ins/VST3"
add_format vst  "$ART/VST/${NAME}.vst"      "/Library/Audio/Plug-Ins/VST"
[ ${#FORMATS[@]} -gt 0 ] || { echo "ERROR: nothing to package, build first"; exit 1; }

# ---------------------------------------------------------------- sign + verify binaries
for key in "${FORMATS[@]}"; do
  BUNDLE="$(find "$OUT/$key" -maxdepth 5 \( -name '*.component' -o -name '*.vst3' -o -name '*.vst' \) | head -1)"
  BIN="$BUNDLE/Contents/MacOS/$NAME"
  ARCHS="$(lipo -archs "$BIN")"
  echo "$BIN -> $ARCHS"
  [[ "$ARCHS" == *x86_64* && "$ARCHS" == *arm64* ]] || { echo "ERROR: not a universal (Intel + Apple Silicon) binary"; exit 1; }
  if [ -n "${DEV_ID_APP:-}" ]; then
    codesign --force --deep --timestamp --options runtime --sign "$DEV_ID_APP" "$BUNDLE"
  else
    codesign --force --deep --sign - "$BUNDLE"   # ad-hoc (needed for Apple Silicon)
  fi
  codesign --verify --deep --strict "$BUNDLE"
done

# ---------------------------------------------------------------- component packages
SCRIPTS="$OUT/scripts"
mkdir -p "$SCRIPTS"
# After installing the AU, make macOS rescan Audio Units so Logic/GarageBand see it immediately.
cat > "$SCRIPTS/postinstall" <<'SH'
#!/bin/bash
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
SH
chmod +x "$SCRIPTS/postinstall"

for key in "${FORMATS[@]}"; do
  EXTRA=()
  [ "$key" = au ] && EXTRA=(--scripts "$SCRIPTS")
  # Non-relocatable: always install into /Library/Audio/Plug-Ins, even if another copy of the
  # bundle exists elsewhere on the disk (otherwise Installer would "update" that copy instead).
  PLIST="$OUT/$key-component.plist"
  pkgbuild --analyze --root "$OUT/$key" "$PLIST"
  i=0
  while plutil -extract "$i" xml1 -o /dev/null "$PLIST" >/dev/null 2>&1; do
    plutil -replace "$i.BundleIsRelocatable" -bool NO "$PLIST"
    i=$((i + 1))
  done
  pkgbuild --root "$OUT/$key" --component-plist "$PLIST" --identifier "$ID.$key" --version "$VERSION" \
           --install-location / ${EXTRA[@]+"${EXTRA[@]}"} "$OUT/${NAME}-$key.pkg"
done

# ---------------------------------------------------------------- installer texts
cat > "$OUT/res/welcome.html" <<HTML
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>AutoNormalizer ${VERSION} — TMFU audiovibe</h2>
<p>Normalizza il plugin che lo precede a un livello <b>LUFS</b> una sola volta per ogni cambio di preset, poi mantiene il guadagno fisso.
Un limiter con look-ahead garantisce che l'uscita non superi mai <b>-1 dBFS</b>.</p>
<p>Verranno installati i formati selezionati in <code>/Library/Audio/Plug-Ins</code>.
Dopo l'installazione riapri la tua DAW (o rifai la scansione dei plugin).</p>
</body></html>
HTML

CHOICES_OUTLINE=""
CHOICES=""
REFS=""
for key in "${FORMATS[@]}"; do
  case "$key" in
    au)   T="Audio Unit (AU)";  D="Logic Pro, GarageBand, MainStage, Ableton Live, ..." ;;
    vst3) T="VST3";             D="Ableton Live, Cubase, Reaper, Bitwig, Studio One, ..." ;;
    vst)  T="VST2";             D="Hosts that only support VST 2.4" ;;
  esac
  CHOICES_OUTLINE+="    <line choice=\"$key\"/>"$'\n'
  CHOICES+="  <choice id=\"$key\" title=\"$T\" description=\"$D\"><pkg-ref id=\"$ID.$key\"/></choice>"$'\n'
  REFS+="  <pkg-ref id=\"$ID.$key\" version=\"$VERSION\">${NAME}-$key.pkg</pkg-ref>"$'\n'
done

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>TMFU audiovibe AutoNormalizer ${VERSION}</title>
  <welcome file="welcome.html" mime-type="text/html"/>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <os-version min="10.13"/>
  <choices-outline>
${CHOICES_OUTLINE}  </choices-outline>
${CHOICES}${REFS}</installer-gui-script>
XML

SIGN_ARGS=()
[ -n "${DEV_ID_INSTALLER:-}" ] && SIGN_ARGS=(--sign "$DEV_ID_INSTALLER" --timestamp)
productbuild --distribution "$OUT/distribution.xml" --resources "$OUT/res" --package-path "$OUT" \
             ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PKG"

# ---------------------------------------------------------------- notarization (optional)
if [ -n "${NOTARY_PROFILE:-}" ] && [ -n "${DEV_ID_INSTALLER:-}" ]; then
  xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$PKG"
fi

pkgutil --check-signature "$PKG" || true
echo "Installer: $PKG"
