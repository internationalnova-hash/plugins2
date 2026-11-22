#!/usr/bin/env bash
# Builds a Release Nova Sub, signs the VST3/AU/Standalone bundles with the
# Developer ID Application identity (hardened runtime), packages them into
# a single installer .pkg (VST3 -> /Library/Audio/Plug-Ins/VST3, AU ->
# /Library/Audio/Plug-Ins/Components, Standalone -> /Applications), signs
# that .pkg with the Developer ID Installer identity, submits it to Apple
# for notarization, and staples the resulting ticket so Gatekeeper can
# verify it offline.
set -euo pipefail

PLUGIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PLUGIN_DIR/build-release"
DIST_DIR="$PLUGIN_DIR/dist"
PKG_ROOT="$DIST_DIR/pkgroot"

PRODUCT_NAME="Nova Sub"
BUNDLE_ID="com.internationalnova.novasub"
VERSION="1.0.0"

APP_IDENTITY="Developer ID Application: Giovanni Nova (HSX9N2UQ83)"
INSTALLER_IDENTITY="Developer ID Installer: Giovanni Nova (HSX9N2UQ83)"
NOTARY_PROFILE="nova-audio-notary"

UNSIGNED_PKG="$DIST_DIR/${PRODUCT_NAME// /}-unsigned.pkg"
SIGNED_PKG="$DIST_DIR/${PRODUCT_NAME// /}-${VERSION}.pkg"

echo "==> [1/7] Configuring Release build"
cmake -S "$PLUGIN_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> [2/7] Building VST3 + AU + Standalone"
cmake --build "$BUILD_DIR" --target NovaSub_All -j "$(sysctl -n hw.ncpu)"

# With CMAKE_BUILD_TYPE set, JUCE nests output under a per-config
# subdirectory (Release/) rather than the flat layout used when no build
# type is set.
ARTEFACTS="$BUILD_DIR/NovaSub_artefacts/Release"
VST3_BUNDLE="$ARTEFACTS/VST3/${PRODUCT_NAME}.vst3"
AU_BUNDLE="$ARTEFACTS/AU/${PRODUCT_NAME}.component"
APP_BUNDLE="$ARTEFACTS/Standalone/${PRODUCT_NAME}.app"

for b in "$VST3_BUNDLE" "$AU_BUNDLE" "$APP_BUNDLE"; do
    if [ ! -e "$b" ]; then
        echo "ERROR: expected build output missing: $b" >&2
        exit 1
    fi
done

echo "==> [3/7] Codesigning bundles (hardened runtime, secure timestamp)"
# No embedded frameworks/dylibs in these bundles (confirmed via `find`), so
# a single top-level sign per bundle is correct - no need for --deep, which
# Apple advises against for anything with independently-signed nested code.
for b in "$VST3_BUNDLE" "$AU_BUNDLE" "$APP_BUNDLE"; do
    codesign --force --options runtime --timestamp \
        --sign "$APP_IDENTITY" "$b"
done

echo "==> [3/7] Verifying codesign"
for b in "$VST3_BUNDLE" "$AU_BUNDLE" "$APP_BUNDLE"; do
    codesign --verify --deep --strict --verbose=2 "$b"
done

echo "==> [4/7] Staging installer root"
rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/Components"
mkdir -p "$PKG_ROOT/Applications"
cp -R "$VST3_BUNDLE" "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU_BUNDLE" "$PKG_ROOT/Library/Audio/Plug-Ins/Components/"
cp -R "$APP_BUNDLE" "$PKG_ROOT/Applications/"

echo "==> [5/7] Building + signing installer package"
mkdir -p "$DIST_DIR"
pkgbuild --root "$PKG_ROOT" \
    --identifier "$BUNDLE_ID.installer" \
    --version "$VERSION" \
    --install-location "/" \
    "$UNSIGNED_PKG"

productsign --sign "$INSTALLER_IDENTITY" "$UNSIGNED_PKG" "$SIGNED_PKG"
rm -f "$UNSIGNED_PKG"

pkgutil --check-signature "$SIGNED_PKG"

echo "==> [6/7] Submitting for notarization (this can take a few minutes)"
xcrun notarytool submit "$SIGNED_PKG" \
    --keychain-profile "$NOTARY_PROFILE" \
    --wait

echo "==> [7/7] Stapling notarization ticket"
xcrun stapler staple "$SIGNED_PKG"

echo "==> Final verification"
spctl -a -vv -t install "$SIGNED_PKG"

echo ""
echo "Done: $SIGNED_PKG"
