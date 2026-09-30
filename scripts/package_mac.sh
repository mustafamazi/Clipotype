#!/usr/bin/env bash
# Builds a universal Release of Clipotype, signs it with Developer ID,
# packages VST3 + AU + Standalone into one installer, then notarizes and staples it.
#
# Requirements:
#   - "Developer ID Application" and "Developer ID Installer" identities in the keychain
#   - notarytool credentials stored once with:
#       xcrun notarytool store-credentials clipotype-notary --apple-id ... --team-id ...
#
# The build folder must NOT be inside iCloud Drive (Desktop/Documents sync):
# iCloud adds extended attributes that make codesign fail.

set -euo pipefail

VERSION="1.1"
PKG_NAME="Clipotype-v${VERSION}-macOS.pkg"
NOTARY_PROFILE="clipotype-notary"
BUNDLE_PREFIX="com.bopsaudio.clipotype"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"
ARTEFACTS="$BUILD_DIR/Clipotype_artefacts/Release"
ENTITLEMENTS="$BUILD_DIR/Clipotype_artefacts/JuceLibraryCode"
STAGE="$BUILD_DIR/pkg"

step() { printf '\n==> %s\n' "$*"; }
die()  { printf '\nERROR: %s\n' "$*" >&2; exit 1; }

# Prints the SHA-1 of the first valid identity matching $1. Signing by hash avoids
# "ambiguous identity" errors when several certificates share the same name.
find_identity() {
    security find-identity -v $2 | grep "\"$1: " | head -1 | awk '{print $2}' || true
}

step "Checking signing identities and notary profile"
APP_ID="$(find_identity "Developer ID Application" "-p codesigning")"
INST_ID="$(find_identity "Developer ID Installer" "")"
[[ -n "$APP_ID" ]]  || die "No valid 'Developer ID Application' identity in the keychain."
[[ -n "$INST_ID" ]] || die "No valid 'Developer ID Installer' identity in the keychain."
xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null \
    || die "Keychain profile '$NOTARY_PROFILE' not found. Create it with: xcrun notarytool store-credentials $NOTARY_PROFILE"
echo "Application: $APP_ID"
echo "Installer:   $INST_ID"

step "Building universal Release in $BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -G Xcode \
    -DCLIPOTYPE_COPY_AFTER_BUILD=OFF \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$BUILD_DIR" --config Release -- -quiet

VST3="$ARTEFACTS/VST3/Clipotype.vst3"
AU="$ARTEFACTS/AU/Clipotype.component"
APP="$ARTEFACTS/Standalone/Clipotype.app"

step "Signing bundles"
sign() {
    xattr -cr "$1"
    codesign --force --options runtime --timestamp \
        --entitlements "$ENTITLEMENTS/Clipotype_$2.entitlements" \
        --sign "$APP_ID" "$1"
    codesign --verify --strict --verbose=2 "$1"
}
sign "$VST3" VST3
sign "$AU"   AU
sign "$APP"  Standalone

step "Building component packages"
rm -rf "$STAGE"
mkdir -p "$STAGE"

component_pkg() {  # <bundle> <name> <install location>
    local root="$STAGE/root-$2"
    mkdir -p "$root"
    ditto "$1" "$root/$(basename "$1")"
    # Stop the installer from "relocating" bundles to other copies it finds on disk.
    pkgbuild --analyze --root "$root" "$STAGE/$2.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$STAGE/$2.plist"
    pkgbuild --root "$root" \
        --component-plist "$STAGE/$2.plist" \
        --identifier "$BUNDLE_PREFIX.$2" \
        --version "$VERSION" \
        --install-location "$3" \
        "$STAGE/Clipotype-$2.pkg"
}
component_pkg "$VST3" vst3 /Library/Audio/Plug-Ins/VST3
component_pkg "$AU"   au   /Library/Audio/Plug-Ins/Components
component_pkg "$APP"  app  /Applications

step "Building signed product archive"
cat > "$STAGE/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Clipotype $VERSION</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <volume-check>
        <allowed-os-versions><os-version min="11.0"/></allowed-os-versions>
    </volume-check>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plug-in" description="Installs to /Library/Audio/Plug-Ins/VST3">
        <pkg-ref id="$BUNDLE_PREFIX.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit" description="Installs to /Library/Audio/Plug-Ins/Components">
        <pkg-ref id="$BUNDLE_PREFIX.au"/>
    </choice>
    <choice id="app" title="Standalone App" description="Installs to /Applications">
        <pkg-ref id="$BUNDLE_PREFIX.app"/>
    </choice>
    <pkg-ref id="$BUNDLE_PREFIX.vst3" version="$VERSION">Clipotype-vst3.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_PREFIX.au" version="$VERSION">Clipotype-au.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_PREFIX.app" version="$VERSION">Clipotype-app.pkg</pkg-ref>
</installer-gui-script>
EOF

PKG="$BUILD_DIR/$PKG_NAME"
productbuild --distribution "$STAGE/distribution.xml" \
    --package-path "$STAGE" \
    --sign "$INST_ID" \
    --timestamp \
    "$PKG"

step "Notarizing (this can take a few minutes)"
set +e
SUBMIT_OUT="$(xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait 2>&1)"
set -e
echo "$SUBMIT_OUT"
SUBMISSION_ID="$(echo "$SUBMIT_OUT" | awk '/^  id: / {print $2; exit}')"
STATUS="$(echo "$SUBMIT_OUT" | awk '/^  status: / {print $2}' | tail -1)"

if [[ "$STATUS" != "Accepted" ]]; then
    if [[ -n "$SUBMISSION_ID" ]]; then
        step "Notarization log"
        xcrun notarytool log "$SUBMISSION_ID" --keychain-profile "$NOTARY_PROFILE"
    fi
    die "Notarization failed (status: ${STATUS:-unknown})."
fi

step "Stapling"
xcrun stapler staple "$PKG"
xcrun stapler validate "$PKG"

step "Gatekeeper assessment"
spctl --assess --type install -v "$PKG"

step "Done: $PKG"
