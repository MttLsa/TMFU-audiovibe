#!/bin/bash
# Builds the macOS installer: GLOBAL_CONTROL-<version>-macOS.pkg  (TMFU audiovibe)
#   - GLOBAL CONTROL.app            -> /Applications                      (universal: Intel x86_64 + Apple Silicon arm64)
#   - GLOBAL_CONTROL_BRIDGE.amxd    -> /Library/Application Support/GLOBAL CONTROL/Bridge  (+ gc_bridge.js)
#     postinstall copies the bridge into the console user's Ableton User Library
#     (~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect) ONLY if that folder exists
#     and no different file is already there: user files are never overwritten (the app offers the
#     update with an explicit confirmation, SETUP panel).
# Run from the GLOBAL_CONTROL folder after a Release build:  ./packaging/macos/make_pkg.sh 1.0.0
# Signed / notarized distribution (optional): export DEV_ID_APP="Developer ID Application: ..."
#   DEV_ID_INSTALLER="Developer ID Installer: ..." NOTARY_PROFILE=<notarytool keychain profile>
set -euo pipefail
VERSION="${1:-1.0.0}"
NAME="GLOBAL CONTROL"
FILE="GLOBAL_CONTROL"
ID="com.tmfuaudiovibe.globalcontrol"
ART="build/GlobalControl_artefacts/Release"
OUT="build/installer"
RES="packaging/macos/resources"
rm -rf "$OUT" && mkdir -p "$OUT/app/Applications" "$OUT/bridge/Library/Application Support/GLOBAL CONTROL/Bridge" "$OUT/scripts-bridge"

cp -R "$ART/${NAME}.app" "$OUT/app/Applications/"
APP="$OUT/app/Applications/${NAME}.app"
[[ -f "$APP/Contents/Resources/Bridge/GLOBAL_CONTROL_BRIDGE.amxd" ]] || { echo "ERROR: bridge not bundled in the app"; exit 1; }
cp "$APP/Contents/Resources/Bridge/"* "$OUT/bridge/Library/Application Support/GLOBAL CONTROL/Bridge/"

# code signature: Developer ID + hardened runtime when available, ad-hoc otherwise
if [[ -n "${DEV_ID_APP:-}" ]]; then
  codesign --force --deep --options runtime --timestamp --sign "$DEV_ID_APP" "$APP"
else
  codesign --force --deep --sign - "$APP"
fi

# universal binary check
BIN="$APP/Contents/MacOS/${NAME}"
ARCHS="$(lipo -archs "$BIN")"
echo "$BIN -> $ARCHS"
[[ "$ARCHS" == *x86_64* && "$ARCHS" == *arm64* ]] || { echo "ERROR: not a universal (Intel + Apple Silicon) binary"; exit 1; }
/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$APP/Contents/Info.plist" | grep -q "^${ID}$" || { echo "ERROR: bundle id"; exit 1; }

# the app must not be relocated by Installer
pkgbuild --analyze --root "$OUT/app" "$OUT/app-components.plist"
PB=/usr/libexec/PlistBuddy
N=$($PB -c "Print" "$OUT/app-components.plist" | grep -c "BundleIsRelocatable" || true)
for ((i = 0; i < N; i++)); do $PB -c "Set :$i:BundleIsRelocatable false" "$OUT/app-components.plist"; done

cat > "$OUT/scripts-bridge/postinstall" <<'SH'
#!/bin/bash
# Copies the bridge into the Ableton User Library of the logged-in user, never overwriting a different file.
SRC="/Library/Application Support/GLOBAL CONTROL/Bridge"
USER_NAME="$(stat -f%Su /dev/console 2>/dev/null || true)"
[[ -z "$USER_NAME" || "$USER_NAME" == "root" ]] && exit 0
HOME_DIR="$(dscl . -read "/Users/$USER_NAME" NFSHomeDirectory 2>/dev/null | awk '{print $2}')"
LIB="$HOME_DIR/Music/Ableton/User Library"
if [[ ! -d "$LIB" ]]; then
  echo "GLOBAL CONTROL: Ableton User Library not found at $LIB - the app will guide the installation"; exit 0
fi
DST="$LIB/Presets/MIDI Effects/Max MIDI Effect"
for F in GLOBAL_CONTROL_BRIDGE.amxd gc_bridge.js; do
  if [[ -f "$DST/$F" ]] && ! cmp -s "$SRC/$F" "$DST/$F"; then
    echo "GLOBAL CONTROL: $DST/$F exists and differs - not overwritten (update it from the app: SETUP)"; exit 0
  fi
done
sudo -u "$USER_NAME" mkdir -p "$DST"
for F in GLOBAL_CONTROL_BRIDGE.amxd gc_bridge.js; do sudo -u "$USER_NAME" cp "$SRC/$F" "$DST/$F"; done
echo "GLOBAL CONTROL: bridge installed in $DST"
exit 0
SH
chmod +x "$OUT/scripts-bridge/postinstall"

pkgbuild --root "$OUT/app" --component-plist "$OUT/app-components.plist" \
         --identifier "$ID.app" --version "$VERSION" --install-location / "$OUT/${FILE}-App.pkg"
pkgbuild --root "$OUT/bridge" --scripts "$OUT/scripts-bridge" \
         --identifier "$ID.bridge" --version "$VERSION" --install-location / "$OUT/${FILE}-Bridge.pkg"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>TMFU audiovibe ${NAME} ${VERSION}</title>
  <welcome file="welcome.html" mime-type="text/html"/>
  <conclusion file="conclusion.html" mime-type="text/html"/>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <os-version min="11.0"/>
  <choices-outline>
    <line choice="app"/>
    <line choice="bridge"/>
  </choices-outline>
  <choice id="app" title="${NAME}.app" description="Standalone application in /Applications (no audio processing)" start_selected="true">
    <pkg-ref id="$ID.app"/>
  </choice>
  <choice id="bridge" title="Ableton Live bridge (Max for Live)" description="GLOBAL_CONTROL_BRIDGE.amxd, copied into your Ableton User Library if it exists (existing files are never overwritten)" start_selected="true">
    <pkg-ref id="$ID.bridge"/>
  </choice>
  <pkg-ref id="$ID.app">${FILE}-App.pkg</pkg-ref>
  <pkg-ref id="$ID.bridge">${FILE}-Bridge.pkg</pkg-ref>
</installer-gui-script>
XML

PRODUCT="$OUT/${FILE}-${VERSION}-macOS.pkg"
if [[ -n "${DEV_ID_INSTALLER:-}" ]]; then
  productbuild --distribution "$OUT/distribution.xml" --resources "$RES" --package-path "$OUT" --sign "$DEV_ID_INSTALLER" "$PRODUCT"
else
  productbuild --distribution "$OUT/distribution.xml" --resources "$RES" --package-path "$OUT" "$PRODUCT"
fi
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
  xcrun notarytool submit "$PRODUCT" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$PRODUCT"
fi

# verify the product
CHECK="$OUT/check"; rm -rf "$CHECK"
pkgutil --expand "$PRODUCT" "$CHECK"
for P in "${FILE}-App.pkg" "${FILE}-Bridge.pkg"; do [[ -d "$CHECK/$P" ]] || { echo "ERROR: $P missing"; exit 1; }; done
pkgutil --payload-files "$OUT/${FILE}-App.pkg" | grep -q "Applications/${NAME}.app/Contents/MacOS/${NAME}" || { echo "ERROR: app payload"; exit 1; }
pkgutil --payload-files "$OUT/${FILE}-Bridge.pkg" | grep -q "GLOBAL CONTROL/Bridge/GLOBAL_CONTROL_BRIDGE.amxd" || { echo "ERROR: bridge payload"; exit 1; }
pkgutil --payload-files "$OUT/${FILE}-Bridge.pkg" | grep -q "GLOBAL CONTROL/Bridge/gc_bridge.js" || { echo "ERROR: bridge script payload"; exit 1; }
if grep -q "<relocate>" "$CHECK/${FILE}-App.pkg/PackageInfo"; then echo "ERROR: app package is relocatable"; exit 1; fi
echo "Installer: $PRODUCT"
