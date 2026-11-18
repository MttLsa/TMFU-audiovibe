#!/bin/bash
# Builds an (unsigned) macOS installer: Twinbox-<version>-macOS.pkg
# containing the AU (Components) and VST3 plug-ins. Universal binaries: Intel + Apple Silicon.
set -euo pipefail
VERSION="${1:-1.0.0}"
ART="build/Twinbox_artefacts/Release"
OUT="build/installer"
rm -rf "$OUT" && mkdir -p "$OUT/au/Library/Audio/Plug-Ins/Components" "$OUT/vst3/Library/Audio/Plug-Ins/VST3"

cp -R "$ART/AU/Twinbox.component" "$OUT/au/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/Twinbox.vst3"    "$OUT/vst3/Library/Audio/Plug-Ins/VST3/"

# ad-hoc code signature (needed on recent macOS for unsigned plug-ins to load)
codesign --force --deep --sign - "$OUT/au/Library/Audio/Plug-Ins/Components/Twinbox.component"
codesign --force --deep --sign - "$OUT/vst3/Library/Audio/Plug-Ins/VST3/Twinbox.vst3"

# Component plists with BundleIsRelocatable = NO: otherwise, if a Twinbox.component / .vst3 with the same bundle id
# already exists anywhere on the Mac (an older copy, a build folder), Installer "relocates" the update there and
# nothing is written to /Library/Audio/Plug-Ins.
for KIND in au vst3; do
  pkgbuild --analyze --root "$OUT/$KIND" "$OUT/$KIND-components.plist"
  PB=/usr/libexec/PlistBuddy
  N=$($PB -c "Print" "$OUT/$KIND-components.plist" | grep -c "BundleIsRelocatable" || true)
  for ((i = 0; i < N; i++)); do
    $PB -c "Set :$i:BundleIsRelocatable false" "$OUT/$KIND-components.plist"
  done
  $PB -c "Print" "$OUT/$KIND-components.plist"
done

# post-install: refresh the Audio Unit cache so hosts (Logic, Live, ...) see the new AU immediately
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
         --identifier com.twinbox.twinbox.au   --version "$VERSION" --install-location / "$OUT/Twinbox-AU.pkg"
pkgbuild --root "$OUT/vst3" --component-plist "$OUT/vst3-components.plist" \
         --identifier com.twinbox.twinbox.vst3 --version "$VERSION" --install-location / "$OUT/Twinbox-VST3.pkg"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>TMFU audiovibe - Twinbox ${VERSION}</title>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="au"/>
    <line choice="vst3"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (AU)" description="Logic Pro, GarageBand, Ableton Live (also Live 9), ..." start_selected="true" start_enabled="true" start_visible="true">
    <pkg-ref id="com.twinbox.twinbox.au"/>
  </choice>
  <choice id="vst3" title="VST3" description="Ableton Live 10.1+, Cubase, Reaper, Bitwig, ..." start_selected="true" start_enabled="true" start_visible="true">
    <pkg-ref id="com.twinbox.twinbox.vst3"/>
  </choice>
  <pkg-ref id="com.twinbox.twinbox.au">Twinbox-AU.pkg</pkg-ref>
  <pkg-ref id="com.twinbox.twinbox.vst3">Twinbox-VST3.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" "$OUT/Twinbox-${VERSION}-macOS.pkg"
for BIN in "$OUT/au/Library/Audio/Plug-Ins/Components/Twinbox.component/Contents/MacOS/Twinbox" \
           "$OUT/vst3/Library/Audio/Plug-Ins/VST3/Twinbox.vst3/Contents/MacOS/Twinbox"; do
  ARCHS="$(lipo -archs "$BIN")"
  echo "$BIN -> $ARCHS"
  [[ "$ARCHS" == *x86_64* && "$ARCHS" == *arm64* ]] || { echo "ERROR: not a universal (Intel + Apple Silicon) binary"; exit 1; }
done
# verify the final product: both packages inside, AU + VST3 binaries in the payloads, installed to /Library, not relocatable
CHECK="$OUT/check"; rm -rf "$CHECK"
pkgutil --expand "$OUT/Twinbox-${VERSION}-macOS.pkg" "$CHECK"
ls "$CHECK"
for P in Twinbox-AU.pkg Twinbox-VST3.pkg; do [[ -d "$CHECK/$P" ]] || { echo "ERROR: $P missing from the installer"; exit 1; }; done
AUFILES="$(pkgutil --payload-files "$OUT/Twinbox-AU.pkg")"
VSTFILES="$(pkgutil --payload-files "$OUT/Twinbox-VST3.pkg")"
echo "$AUFILES" | grep -q "Library/Audio/Plug-Ins/Components/Twinbox.component/Contents/MacOS/Twinbox" || { echo "ERROR: AU binary missing from the payload"; exit 1; }
echo "$VSTFILES" | grep -q "Library/Audio/Plug-Ins/VST3/Twinbox.vst3/Contents/MacOS/Twinbox" || { echo "ERROR: VST3 binary missing from the payload"; exit 1; }
if grep -q "<relocate>" "$CHECK/Twinbox-AU.pkg/PackageInfo"; then echo "ERROR: AU package is relocatable"; exit 1; fi
echo "Installer contents OK: AU -> /Library/Audio/Plug-Ins/Components, VST3 -> /Library/Audio/Plug-Ins/VST3"
cat "$CHECK/Twinbox-AU.pkg/PackageInfo"
echo "Installer: $OUT/Twinbox-${VERSION}-macOS.pkg"
