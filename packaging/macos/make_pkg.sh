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

pkgbuild --root "$OUT/au"   --identifier com.twinbox.twinbox.au   --version "$VERSION" --install-location / "$OUT/Twinbox-AU.pkg"
pkgbuild --root "$OUT/vst3" --identifier com.twinbox.twinbox.vst3 --version "$VERSION" --install-location / "$OUT/Twinbox-VST3.pkg"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>Twinbox ${VERSION}</title>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="au"/>
    <line choice="vst3"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (AU)" description="Logic Pro, GarageBand, Ableton Live, ...">
    <pkg-ref id="com.twinbox.twinbox.au"/>
  </choice>
  <choice id="vst3" title="VST3" description="Ableton Live, Cubase, Reaper, Bitwig, ...">
    <pkg-ref id="com.twinbox.twinbox.vst3"/>
  </choice>
  <pkg-ref id="com.twinbox.twinbox.au">Twinbox-AU.pkg</pkg-ref>
  <pkg-ref id="com.twinbox.twinbox.vst3">Twinbox-VST3.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" "$OUT/Twinbox-${VERSION}-macOS.pkg"
lipo -info "$OUT/au/Library/Audio/Plug-Ins/Components/Twinbox.component/Contents/MacOS/Twinbox"
echo "Installer: $OUT/Twinbox-${VERSION}-macOS.pkg"
