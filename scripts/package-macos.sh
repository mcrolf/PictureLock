#!/usr/bin/env bash
# Builds dist/PictureLock-<version>-macOS.pkg from a Release build.
#
# Usage: scripts/package-macos.sh [build-dir]
#
# Optional signing (all three must be set to sign and notarize):
#   MACOS_CODESIGN_IDENTITY   "Developer ID Application: Name (TEAMID)"
#   MACOS_INSTALLER_IDENTITY  "Developer ID Installer: Name (TEAMID)"
#   MACOS_NOTARY_PROFILE      keychain profile created with `xcrun notarytool store-credentials`
# Without them the plug-ins are ad-hoc signed and the installer is unsigned.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
ARTEFACTS="$BUILD/PictureLock_artefacts/Release"
VERSION="$(sed -nE 's/^project\(PictureLock VERSION ([0-9.]+).*/\1/p' "$ROOT/CMakeLists.txt")"
DIST="$ROOT/dist"
WORK="$BUILD/package"
IDENTIFIER="org.picturelock"

for bundle in "VST3/PictureLock.vst3" "AU/PictureLock.component" "Standalone/PictureLock.app"; do
  [ -d "$ARTEFACTS/$bundle" ] || { echo "Missing $ARTEFACTS/$bundle; build Release first" >&2; exit 1; }
done

rm -rf "$WORK"; mkdir -p "$WORK/scripts" "$DIST"

sign() {
  if [ -n "${MACOS_CODESIGN_IDENTITY:-}" ]; then
    codesign --force --deep --options runtime --timestamp --sign "$MACOS_CODESIGN_IDENTITY" "$1"
  else
    codesign --force --deep --sign - "$1"
  fi
}

stage() { # stage <name> <artefact> <install-location>
  local root="$WORK/root-$1"
  mkdir -p "$root"
  cp -R "$ARTEFACTS/$2" "$root/"
  sign "$root/$(basename "$2")"
  pkgbuild --quiet --root "$root" --install-location "$3" --identifier "$IDENTIFIER.$1" \
           --version "$VERSION" ${4:+--scripts "$4"} "$WORK/$1.pkg"
}

# Refresh the Audio Unit cache so Live sees the new component without a reboot.
cat > "$WORK/scripts/postinstall" <<'POST'
#!/bin/sh
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
POST
chmod +x "$WORK/scripts/postinstall"

stage vst3 "VST3/PictureLock.vst3" "/Library/Audio/Plug-Ins/VST3"
stage au "AU/PictureLock.component" "/Library/Audio/Plug-Ins/Components" "$WORK/scripts"
stage app "Standalone/PictureLock.app" "/Applications"

cat > "$WORK/distribution.xml" <<DIST_XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>PictureLock $VERSION</title>
  <license file="LICENSE.txt"/>
  <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="vst3"/>
    <line choice="au"/>
    <line choice="app"/>
  </choices-outline>
  <choice id="vst3" title="VST3 plug-in" description="For Ableton Live and other VST3 hosts.">
    <pkg-ref id="$IDENTIFIER.vst3"/>
  </choice>
  <choice id="au" title="Audio Unit plug-in" description="For Ableton Live, Logic and other AU hosts.">
    <pkg-ref id="$IDENTIFIER.au"/>
  </choice>
  <choice id="app" title="Standalone app" start_selected="false" description="Opens movies without a host. Useful for checking a file plays.">
    <pkg-ref id="$IDENTIFIER.app"/>
  </choice>
  <pkg-ref id="$IDENTIFIER.vst3" version="$VERSION">vst3.pkg</pkg-ref>
  <pkg-ref id="$IDENTIFIER.au" version="$VERSION">au.pkg</pkg-ref>
  <pkg-ref id="$IDENTIFIER.app" version="$VERSION">app.pkg</pkg-ref>
</installer-gui-script>
DIST_XML

mkdir -p "$WORK/resources"
cp "$ROOT/LICENSE" "$WORK/resources/LICENSE.txt"

OUTPUT="$DIST/PictureLock-$VERSION-macOS.pkg"
productbuild --quiet --distribution "$WORK/distribution.xml" --package-path "$WORK" \
             --resources "$WORK/resources" \
             ${MACOS_INSTALLER_IDENTITY:+--sign "$MACOS_INSTALLER_IDENTITY"} "$OUTPUT"

if [ -n "${MACOS_INSTALLER_IDENTITY:-}" ] && [ -n "${MACOS_NOTARY_PROFILE:-}" ]; then
  xcrun notarytool submit "$OUTPUT" --keychain-profile "$MACOS_NOTARY_PROFILE" --wait
  xcrun stapler staple "$OUTPUT"
fi

echo "Wrote $OUTPUT"
