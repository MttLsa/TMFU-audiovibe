#!/bin/bash
# Builds an (unsigned) macOS installer: HEATSINK-<version>-macOS.pkg  (TMFU audiovibe)
# Contains: AU (/Library/Audio/Plug-Ins/Components), VST3 (/Library/Audio/Plug-Ins/VST3), Standalone app (/Applications).
# Universal binaries: Intel x86_64 + Apple Silicon arm64 (checked below).
# Run from the HEATSINK folder after a Release build:  ./packaging/macos/make_pkg.sh 1.0.0
set -euo pipefail
VERSION="${1:-1.0.0}"
NAME="HEATSINK"
ID="com.tmfuaudiovibe.heatsink"
ART="build/${NAME}_artefacts/Release"
OUT="build/installer"
rm -rf "$OUT" && mkdir -p "$OUT/au/Library/Audio/Plug-Ins/Components" "$OUT/vst3/Library/Audio/Plug-Ins/VST3" "$OUT/app/Applications"

cp -R "$ART/AU/${NAME}.component" "$OUT/au/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/${NAME}.vst3"    "$OUT/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/Standalone/${NAME}.app" "$OUT/app/Applications/"

# ad-hoc code signature (needed on recent macOS for unsigned plug-ins to load)
codesign --force --deep --sign - "$OUT/au/Library/Audio/Plug-Ins/Components/${NAME}.component"
codesign --force --deep --sign - "$OUT/vst3/Library/Audio/Plug-Ins/VST3/${NAME}.vst3"
codesign --force --deep --sign - "$OUT/app/Applications/${NAME}.app"

# universal binary check
for BIN in "$OUT/au/Library/Audio/Plug-Ins/Components/${NAME}.component/Contents/MacOS/${NAME}" \
           "$OUT/vst3/Library/Audio/Plug-Ins/VST3/${NAME}.vst3/Contents/MacOS/${NAME}" \
           "$OUT/app/Applications/${NAME}.app/Contents/MacOS/${NAME}"; do
  ARCHS="$(lipo -archs "$BIN")"
  echo "$BIN -> $ARCHS"
  [[ "$ARCHS" == *x86_64* && "$ARCHS" == *arm64* ]] || { echo "ERROR: not a universal (Intel + Apple Silicon) binary"; exit 1; }
done

# component plists with BundleIsRelocatable = NO, so Installer always writes to the standard folders
for KIND in au vst3 app; do
  pkgbuild --analyze --root "$OUT/$KIND" "$OUT/$KIND-components.plist"
  PB=/usr/libexec/PlistBuddy
  N=$($PB -c "Print" "$OUT/$KIND-components.plist" | grep -c "BundleIsRelocatable" || true)
  for ((i = 0; i < N; i++)); do $PB -c "Set :$i:BundleIsRelocatable false" "$OUT/$KIND-components.plist"; done
done

# post-install: refresh the Audio Unit cache so hosts see the new AU immediately
mkdir -p "$OUT/scripts-au"
cat > "$OUT/scripts-au/postinstall" <<'SH'
#!/bin/bash
killall -9 AudioComponentRegistrar 2>/dev/null || true
CONSOLE_USER="$(stat -f%Su /dev/console 2>/dev/null || true)"
[ -n "$CONSOLE_USER" ] && rm -f "/Users/$CONSOLE_USER/Library/Caches/AudioUnitCache/com.apple.audiounits.cache" 2>/dev/null || true
exit 0
SH
chmod +x "$OUT/scripts-au/postinstall"

pkgbuild --root "$OUT/au"   --component-plist "$OUT/au-components.plist"   --scripts "$OUT/scripts-au" \
         --identifier "$ID.au"   --version "$VERSION" --install-location / "$OUT/${NAME}-AU.pkg"
pkgbuild --root "$OUT/vst3" --component-plist "$OUT/vst3-components.plist" \
         --identifier "$ID.vst3" --version "$VERSION" --install-location / "$OUT/${NAME}-VST3.pkg"
pkgbuild --root "$OUT/app"  --component-plist "$OUT/app-components.plist" \
         --identifier "$ID.app"  --version "$VERSION" --install-location / "$OUT/${NAME}-App.pkg"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>TMFU audiovibe ${NAME} ${VERSION}</title>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="au"/>
    <line choice="vst3"/>
    <line choice="app"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (AU)" description="Logic Pro, GarageBand, Ableton Live, ..." start_selected="true">
    <pkg-ref id="$ID.au"/>
  </choice>
  <choice id="vst3" title="VST3" description="Ableton Live, Cubase, Reaper, Bitwig, Studio One, ..." start_selected="true">
    <pkg-ref id="$ID.vst3"/>
  </choice>
  <choice id="app" title="Standalone app" description="${NAME}.app in /Applications" start_selected="true">
    <pkg-ref id="$ID.app"/>
  </choice>
  <pkg-ref id="$ID.au">${NAME}-AU.pkg</pkg-ref>
  <pkg-ref id="$ID.vst3">${NAME}-VST3.pkg</pkg-ref>
  <pkg-ref id="$ID.app">${NAME}-App.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" "$OUT/${NAME}-${VERSION}-macOS.pkg"

# verify the product
CHECK="$OUT/check"; rm -rf "$CHECK"
pkgutil --expand "$OUT/${NAME}-${VERSION}-macOS.pkg" "$CHECK"
for P in ${NAME}-AU.pkg ${NAME}-VST3.pkg ${NAME}-App.pkg; do [[ -d "$CHECK/$P" ]] || { echo "ERROR: $P missing"; exit 1; }; done
pkgutil --payload-files "$OUT/${NAME}-AU.pkg"   | grep -q "Components/${NAME}.component/Contents/MacOS/${NAME}" || { echo "ERROR: AU payload"; exit 1; }
pkgutil --payload-files "$OUT/${NAME}-VST3.pkg" | grep -q "VST3/${NAME}.vst3/Contents/MacOS/${NAME}"        || { echo "ERROR: VST3 payload"; exit 1; }
pkgutil --payload-files "$OUT/${NAME}-App.pkg"  | grep -q "Applications/${NAME}.app/Contents/MacOS/${NAME}"  || { echo "ERROR: app payload"; exit 1; }
if grep -q "<relocate>" "$CHECK/${NAME}-AU.pkg/PackageInfo"; then echo "ERROR: AU package is relocatable"; exit 1; fi
# the AU must present itself as "TMFU audiovibe: HEATSINK"
/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:name" "$OUT/au/Library/Audio/Plug-Ins/Components/${NAME}.component/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:name" "$OUT/au/Library/Audio/Plug-Ins/Components/${NAME}.component/Contents/Info.plist" | grep -q "^TMFU audiovibe: ${NAME}$" \
  || { echo "ERROR: AU manufacturer name is not TMFU audiovibe"; exit 1; }
echo "Installer: $OUT/${NAME}-${VERSION}-macOS.pkg"
