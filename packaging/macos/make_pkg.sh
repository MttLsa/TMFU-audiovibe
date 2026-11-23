#!/bin/bash
# Builds an (unsigned) macOS installer: DUALBOX-<version>-macOS.pkg
# containing the AU (Components) and VST3 plug-ins and the factory samples (/Users/Shared/Dualbox/Samples). Universal binaries: Intel + Apple Silicon.
set -euo pipefail
VERSION="${1:-1.0.0}"
ART="build/Dualbox_artefacts/Release"
OUT="build/installer"
rm -rf "$OUT" && mkdir -p "$OUT/au/Library/Audio/Plug-Ins/Components" "$OUT/vst3/Library/Audio/Plug-Ins/VST3"

# factory sample library -> /Users/Shared/Dualbox/Samples (the plug-in would also install it on first run)
mkdir -p "$OUT/samples/Users/Shared/Dualbox/Samples"
ditto -x -k resources/factory_samples.zip "$OUT/samples/Users/Shared/Dualbox/Samples"

cp -R "$ART/AU/DUALBOX.component" "$OUT/au/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/VST3/DUALBOX.vst3"    "$OUT/vst3/Library/Audio/Plug-Ins/VST3/"

# third-party licence notices (texts read from the sources of this build): inside both plug-in bundles and next to
# the factory samples (/Users/Shared/Dualbox/THIRD_PARTY_NOTICES.txt)
python3 tools/make_notices.py build/_deps/juce-src "$OUT/THIRD_PARTY_NOTICES.txt"
for BUNDLE in "$OUT/au/Library/Audio/Plug-Ins/Components/DUALBOX.component" "$OUT/vst3/Library/Audio/Plug-Ins/VST3/DUALBOX.vst3"; do
  mkdir -p "$BUNDLE/Contents/Resources"
  cp "$OUT/THIRD_PARTY_NOTICES.txt" "$BUNDLE/Contents/Resources/"
done
cp "$OUT/THIRD_PARTY_NOTICES.txt" "$OUT/samples/Users/Shared/Dualbox/"

# ad-hoc code signature (needed on recent macOS for unsigned plug-ins to load)
codesign --force --deep --sign - "$OUT/au/Library/Audio/Plug-Ins/Components/DUALBOX.component"
codesign --force --deep --sign - "$OUT/vst3/Library/Audio/Plug-Ins/VST3/DUALBOX.vst3"

# Component plists with BundleIsRelocatable = NO: otherwise, if a DUALBOX.component / .vst3 with the same bundle id
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

# pre-install (both packages): remove the old "Twinbox" bundles. DUALBOX is the same plug-in renamed, with the same
# AU / VST3 codes, so an old Twinbox.component / .vst3 left installed would clash with it in the hosts.
mkdir -p "$OUT/scripts-vst3"
cat > "$OUT/scripts-au/preinstall" <<'SH'
#!/bin/bash
CONSOLE_USER="$(stat -f%Su /dev/console 2>/dev/null || true)"
for D in "/Library/Audio/Plug-Ins/Components" "/Users/$CONSOLE_USER/Library/Audio/Plug-Ins/Components"; do
  rm -rf "$D/Twinbox.component" 2>/dev/null || true
done
exit 0
SH
cat > "$OUT/scripts-vst3/preinstall" <<'SH'
#!/bin/bash
CONSOLE_USER="$(stat -f%Su /dev/console 2>/dev/null || true)"
for D in "/Library/Audio/Plug-Ins/VST3" "/Users/$CONSOLE_USER/Library/Audio/Plug-Ins/VST3"; do
  rm -rf "$D/Twinbox.vst3" 2>/dev/null || true
done
exit 0
SH
chmod +x "$OUT/scripts-au/preinstall" "$OUT/scripts-vst3/preinstall"

# samples post-install: everybody can write the library (pitch cache files, Legacy import, factory upgrades)
mkdir -p "$OUT/scripts-samples"
cat > "$OUT/scripts-samples/postinstall" <<'SH'
#!/bin/bash
chmod -R a+rwX "/Users/Shared/Dualbox" 2>/dev/null || true
exit 0
SH
chmod +x "$OUT/scripts-samples/postinstall"

pkgbuild --root "$OUT/au"   --component-plist "$OUT/au-components.plist"   --scripts "$OUT/scripts-au" \
         --identifier com.tmfuaudiovibe.dualbox.au   --version "$VERSION" --install-location / "$OUT/DUALBOX-AU.pkg"
pkgbuild --root "$OUT/vst3" --component-plist "$OUT/vst3-components.plist" --scripts "$OUT/scripts-vst3" \
         --identifier com.tmfuaudiovibe.dualbox.vst3 --version "$VERSION" --install-location / "$OUT/DUALBOX-VST3.pkg"
pkgbuild --root "$OUT/samples" --scripts "$OUT/scripts-samples" \
         --identifier com.tmfuaudiovibe.dualbox.samples --version "$VERSION" --install-location / "$OUT/DUALBOX-Samples.pkg"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>TMFU audiovibe - DUALBOX ${VERSION}</title>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="au"/>
    <line choice="vst3"/>
    <line choice="samples"/>
  </choices-outline>
  <choice id="au" title="Audio Unit (AU)" description="Logic Pro, GarageBand, Ableton Live (also Live 9), ..." start_selected="true" start_enabled="true" start_visible="true">
    <pkg-ref id="com.tmfuaudiovibe.dualbox.au"/>
  </choice>
  <choice id="vst3" title="VST3" description="Ableton Live 10.1+, Cubase, Reaper, Bitwig, ..." start_selected="true" start_enabled="true" start_visible="true">
    <pkg-ref id="com.tmfuaudiovibe.dualbox.vst3"/>
  </choice>
  <choice id="samples" title="Factory samples" description="Sample library in /Users/Shared/Dualbox/Samples" start_selected="true" start_enabled="true" start_visible="true">
    <pkg-ref id="com.tmfuaudiovibe.dualbox.samples"/>
  </choice>
  <pkg-ref id="com.tmfuaudiovibe.dualbox.au">DUALBOX-AU.pkg</pkg-ref>
  <pkg-ref id="com.tmfuaudiovibe.dualbox.vst3">DUALBOX-VST3.pkg</pkg-ref>
  <pkg-ref id="com.tmfuaudiovibe.dualbox.samples">DUALBOX-Samples.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT" "$OUT/DUALBOX-${VERSION}-macOS.pkg"
for BIN in "$OUT/au/Library/Audio/Plug-Ins/Components/DUALBOX.component/Contents/MacOS/DUALBOX" \
           "$OUT/vst3/Library/Audio/Plug-Ins/VST3/DUALBOX.vst3/Contents/MacOS/DUALBOX"; do
  ARCHS="$(lipo -archs "$BIN")"
  echo "$BIN -> $ARCHS"
  [[ "$ARCHS" == *x86_64* && "$ARCHS" == *arm64* ]] || { echo "ERROR: not a universal (Intel + Apple Silicon) binary"; exit 1; }
done
# verify the final product: both packages inside, AU + VST3 binaries in the payloads, installed to /Library, not relocatable
CHECK="$OUT/check"; rm -rf "$CHECK"
pkgutil --expand "$OUT/DUALBOX-${VERSION}-macOS.pkg" "$CHECK"
ls "$CHECK"
for P in DUALBOX-AU.pkg DUALBOX-VST3.pkg DUALBOX-Samples.pkg; do [[ -d "$CHECK/$P" ]] || { echo "ERROR: $P missing from the installer"; exit 1; }; done
AUFILES="$(pkgutil --payload-files "$OUT/DUALBOX-AU.pkg")"
VSTFILES="$(pkgutil --payload-files "$OUT/DUALBOX-VST3.pkg")"
echo "$AUFILES" | grep -q "Library/Audio/Plug-Ins/Components/DUALBOX.component/Contents/MacOS/DUALBOX" || { echo "ERROR: AU binary missing from the payload"; exit 1; }
echo "$VSTFILES" | grep -q "Library/Audio/Plug-Ins/VST3/DUALBOX.vst3/Contents/MacOS/DUALBOX" || { echo "ERROR: VST3 binary missing from the payload"; exit 1; }
pkgutil --payload-files "$OUT/DUALBOX-Samples.pkg" | grep -q "Users/Shared/Dualbox/Samples/Dualbox Kick/" || { echo "ERROR: factory samples missing from the payload"; exit 1; }
pkgutil --payload-files "$OUT/DUALBOX-Samples.pkg" | grep -q "Users/Shared/Dualbox/THIRD_PARTY_NOTICES.txt" || { echo "ERROR: third-party notices missing"; exit 1; }
echo "$AUFILES" | grep -q "DUALBOX.component/Contents/Resources/THIRD_PARTY_NOTICES.txt" || { echo "ERROR: notices missing from the AU bundle"; exit 1; }
if grep -q "<relocate>" "$CHECK/DUALBOX-AU.pkg/PackageInfo"; then echo "ERROR: AU package is relocatable"; exit 1; fi
echo "Installer contents OK: AU -> /Library/Audio/Plug-Ins/Components, VST3 -> /Library/Audio/Plug-Ins/VST3, samples -> /Users/Shared/Dualbox/Samples"
cat "$CHECK/DUALBOX-AU.pkg/PackageInfo"
echo "Installer: $OUT/DUALBOX-${VERSION}-macOS.pkg"
