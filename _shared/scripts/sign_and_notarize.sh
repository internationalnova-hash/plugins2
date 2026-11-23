#!/usr/bin/env bash
# Generalized version of Nova Sub's original sign_and_notarize.sh - builds a
# Release config of the given plugin, signs the VST3/AU/Standalone bundles
# with the Developer ID Application identity (hardened runtime), packages
# them into a single installer .pkg (VST3 -> /Library/Audio/Plug-Ins/VST3,
# AU -> /Library/Audio/Plug-Ins/Components, Standalone -> /Applications),
# signs that .pkg with the Developer ID Installer identity, submits it to
# Apple for notarization, and staples the resulting ticket.
#
# AAX is deliberately never touched here even when the plugin's FORMATS
# list includes it (building "_All" builds it as a side effect but it is
# not signed/packaged) - matches this catalog's established "AAX path
# wired, not shipped" posture; AAX has its own PACE/Avid signing
# requirements separate from Apple notarization.
#
# Usage: sign_and_notarize.sh <plugin-dir-name> <cmake-target> <product-name> <bundle-id> [version]
set -euo pipefail

if [ "$#" -lt 4 ]; then
    echo "Usage: $0 <plugin-dir-name> <cmake-target> <product-name> <bundle-id> [version]" >&2
    exit 1
fi

PLUGIN_DIR_NAME="$1"
CMAKE_TARGET="$2"
PRODUCT_NAME="$3"
BUNDLE_ID="$4"
VERSION="${5:-1.0.0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGINS_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PLUGIN_DIR="$PLUGINS_ROOT/$PLUGIN_DIR_NAME"

if [ ! -d "$PLUGIN_DIR" ]; then
    echo "ERROR: plugin directory not found: $PLUGIN_DIR" >&2
    exit 1
fi

BUILD_DIR="$PLUGIN_DIR/build-release"
DIST_DIR="$PLUGIN_DIR/dist"
PKG_ROOT="$DIST_DIR/pkgroot"

APP_IDENTITY="Developer ID Application: Giovanni Nova (HSX9N2UQ83)"
INSTALLER_IDENTITY="Developer ID Installer: Giovanni Nova (HSX9N2UQ83)"
NOTARY_PROFILE="nova-audio-notary"

UNSIGNED_PKG="$DIST_DIR/${PRODUCT_NAME// /}-unsigned.pkg"
SIGNED_PKG="$DIST_DIR/${PRODUCT_NAME// /}-${VERSION}.pkg"

echo "==> [$PRODUCT_NAME 1/7] Configuring Release build"
cmake -S "$PLUGIN_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> [$PRODUCT_NAME 2/7] Building VST3 + AU + Standalone (via ${CMAKE_TARGET}_All)"
cmake --build "$BUILD_DIR" --target "${CMAKE_TARGET}_All" -j "$(sysctl -n hw.ncpu)"

# With CMAKE_BUILD_TYPE set, JUCE nests output under a per-config
# subdirectory (Release/) rather than the flat layout used when no build
# type is set.
ARTEFACTS="$BUILD_DIR/${CMAKE_TARGET}_artefacts/Release"
VST3_BUNDLE="$ARTEFACTS/VST3/${PRODUCT_NAME}.vst3"
AU_BUNDLE="$ARTEFACTS/AU/${PRODUCT_NAME}.component"
APP_BUNDLE="$ARTEFACTS/Standalone/${PRODUCT_NAME}.app"

BUNDLES=()
for b in "$VST3_BUNDLE" "$AU_BUNDLE" "$APP_BUNDLE"; do
    if [ -e "$b" ]; then
        BUNDLES+=("$b")
    else
        echo "WARNING: expected build output missing, skipping: $b" >&2
    fi
done

if [ "${#BUNDLES[@]}" -eq 0 ]; then
    echo "ERROR: no bundles were produced for $PRODUCT_NAME - nothing to sign" >&2
    exit 1
fi

echo "==> [$PRODUCT_NAME 3/7] Codesigning bundles (hardened runtime, secure timestamp)"
for b in "${BUNDLES[@]}"; do
    codesign --force --options runtime --timestamp \
        --sign "$APP_IDENTITY" "$b"
done

echo "==> [$PRODUCT_NAME 3/7] Verifying codesign"
for b in "${BUNDLES[@]}"; do
    codesign --verify --deep --strict --verbose=2 "$b"
done

echo "==> [$PRODUCT_NAME 4/7] Staging installer root"
rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/Components"
mkdir -p "$PKG_ROOT/Applications"
[ -e "$VST3_BUNDLE" ] && cp -R "$VST3_BUNDLE" "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/"
[ -e "$AU_BUNDLE" ] && cp -R "$AU_BUNDLE" "$PKG_ROOT/Library/Audio/Plug-Ins/Components/"
[ -e "$APP_BUNDLE" ] && cp -R "$APP_BUNDLE" "$PKG_ROOT/Applications/"

# No extended attributes / AppleDouble "._" entries in the payload: on some
# systems they install as stray "._Plugin.vst3" files that DAWs try to scan.
xattr -cr "$PKG_ROOT"

echo "==> [$PRODUCT_NAME 5/7] Building + signing installer package"
mkdir -p "$DIST_DIR"
COPYFILE_DISABLE=1 pkgbuild --root "$PKG_ROOT" \
    --identifier "$BUNDLE_ID.installer" \
    --version "$VERSION" \
    --install-location "/" \
    "$UNSIGNED_PKG"

productsign --sign "$INSTALLER_IDENTITY" "$UNSIGNED_PKG" "$SIGNED_PKG"
rm -f "$UNSIGNED_PKG"

pkgutil --check-signature "$SIGNED_PKG"

# SKIP_NOTARIZE=1: stop at a signed (not yet notarized) installer, e.g. while
# Apple's developer agreement needs accepting. Notarize later with
# notarize_pkg.sh <pkg>.
if [ "${SKIP_NOTARIZE:-0}" = "1" ]; then
    echo ""
    echo "Signed, not notarized (SKIP_NOTARIZE=1): $SIGNED_PKG"
    exit 0
fi

echo "==> [$PRODUCT_NAME 6/7] Submitting for notarization (this can take a few minutes)"
xcrun notarytool submit "$SIGNED_PKG" \
    --keychain-profile "$NOTARY_PROFILE" \
    --wait

echo "==> [$PRODUCT_NAME 7/7] Stapling notarization ticket"
xcrun stapler staple "$SIGNED_PKG"

echo "==> [$PRODUCT_NAME] Final verification"
spctl -a -vv -t install "$SIGNED_PKG"

echo ""
echo "Done: $SIGNED_PKG"
