#!/bin/bash
# Builds an (unsigned) macOS installer: Groovulation-<version>-macOS.pkg
# with the AU (Components) and VST3 plug-ins (universal: Intel + Apple Silicon) and the
# /Users/Shared/Groovulation folders (Loops, PRINT, Snapshots, Database, Skins).
# Run from the Groovulation/ folder after building:  ./packaging/macos/make_pkg.sh 1.0.0
set -euo pipefail
VERSION="${1:-1.0.0}"
ART="build/Groovulation_artefacts/Release"
OUT="build/installer"
HERE="$(cd "$(dirname "$0")" && pwd)"
rm -rf "$OUT" && mkdir -p "$OUT/au/Library/Audio/Plug-Ins/Components" "$OUT/vst3/Library/Audio/Plug-Ins/VST3"

cp -R "$ART/AU/Groovulation.component" "$OUT/au/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/Groovulation.vst3"    "$OUT/vst3/Library/Audio/Plug-Ins/VST3/"

# ad-hoc code signature (needed on recent macOS for unsigned plug-ins to load)
codesign --force --deep --sign - "$OUT/au/Library/Audio/Plug-Ins/Components/Groovulation.component"
codesign --force --deep --sign - "$OUT/vst3/Library/Audio/Plug-Ins/VST3/Groovulation.vst3"

pkgbuild --root "$OUT/au"   --identifier com.twinbox.groovulation.au   --version "$VERSION" --install-location / "$OUT/Groovulation-AU.pkg"
pkgbuild --root "$OUT/vst3" --identifier com.twinbox.groovulation.vst3 --version "$VERSION" --install-location / "$OUT/Groovulation-VST3.pkg"
pkgbuild --nopayload --scripts "$HERE/scripts" --identifier com.twinbox.groovulation.folders --version "$VERSION" "$OUT/Groovulation-Folders.pkg"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>Groovulation ${VERSION}</title>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="au"/>
    <line choice="vst3"/>
    <line choice="folders"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (AU)" description="Logic Pro, GarageBand, Ableton Live, ...">
    <pkg-ref id="com.twinbox.groovulation.au"/>
  </choice>
  <choice id="vst3" title="VST3" description="Ableton Live, Cubase, Reaper, Bitwig, ...">
    <pkg-ref id="com.twinbox.groovulation.vst3"/>
  </choice>
  <choice id="folders" title="Groovulation folders" description="/Users/Shared/Groovulation: Loops, PRINT, Snapshots, Database" enabled="false">
    <pkg-ref id="com.twinbox.groovulation.folders"/>
  </choice>
  <pkg-ref id="com.twinbox.groovulation.au">Groovulation-AU.pkg</pkg-ref>
  <pkg-ref id="com.twinbox.groovulation.vst3">Groovulation-VST3.pkg</pkg-ref>
  <pkg-ref id="com.twinbox.groovulation.folders">Groovulation-Folders.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" "$OUT/Groovulation-${VERSION}-macOS.pkg"
for BIN in "$OUT/au/Library/Audio/Plug-Ins/Components/Groovulation.component/Contents/MacOS/Groovulation" \
           "$OUT/vst3/Library/Audio/Plug-Ins/VST3/Groovulation.vst3/Contents/MacOS/Groovulation"; do
  ARCHS="$(lipo -archs "$BIN")"
  echo "$BIN -> $ARCHS"
  [[ "$ARCHS" == *x86_64* && "$ARCHS" == *arm64* ]] || { echo "ERROR: not a universal (Intel + Apple Silicon) binary"; exit 1; }
done
echo "Installer: $OUT/Groovulation-${VERSION}-macOS.pkg"
