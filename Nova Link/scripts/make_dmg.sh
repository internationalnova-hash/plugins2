#!/usr/bin/env bash
# Builds a signed + notarized Nova Link installer and wraps it in a DMG.
#   VST3 -> /Library/Audio/Plug-Ins/VST3, AU -> /Library/Audio/Plug-Ins/Components,
#   Standalone -> /Applications. AAX is left out (retail Pro Tools needs PACE signing).
# Usage: scripts/make_dmg.sh [output-dir]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:-$ROOT/dist}"
VERSION="$(grep -E '^project\(NovaLink VERSION' "$ROOT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
APP_ID="Developer ID Application: Giovanni Nova (HSX9N2UQ83)"
INST_ID="Developer ID Installer: Giovanni Nova (HSX9N2UQ83)"
NOTARY="nova-audio-notary"

ART="$ROOT/build/NovaLink_artefacts/Release"
WORK="$ROOT/dist/work"
rm -rf "$WORK"; mkdir -p "$WORK/root/Library/Audio/Plug-Ins/VST3" "$WORK/root/Library/Audio/Plug-Ins/Components" "$WORK/root/Applications" "$OUT"

echo "==> Building"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" --config Release -j "$(sysctl -n hw.ncpu)" >/dev/null

echo "==> Checking every binary is self-contained"
for b in "$ART/VST3/Nova Link.vst3" "$ART/AU/Nova Link.component" "$ART/Standalone/Nova Link.app"; do
    if otool -L "$b/Contents/MacOS/Nova Link" | grep -q '@rpath'; then echo "ERROR: $b links an external library" >&2; exit 1; fi
done

echo "==> Staging + signing (hardened runtime)"
cp -R "$ART/VST3/Nova Link.vst3" "$WORK/root/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/Nova Link.component" "$WORK/root/Library/Audio/Plug-Ins/Components/"
cp -R "$ART/Standalone/Nova Link.app" "$WORK/root/Applications/"
xattr -cr "$WORK/root"
codesign --force --options runtime --timestamp --sign "$APP_ID" "$WORK/root/Library/Audio/Plug-Ins/VST3/Nova Link.vst3"
codesign --force --options runtime --timestamp --sign "$APP_ID" "$WORK/root/Library/Audio/Plug-Ins/Components/Nova Link.component"
# The standalone app opens the mic and camera itself, so it needs those entitlements.
codesign --force --options runtime --timestamp --entitlements "$HERE/standalone.entitlements" --sign "$APP_ID" "$WORK/root/Applications/Nova Link.app"
for b in "$WORK/root/Library/Audio/Plug-Ins/VST3/Nova Link.vst3" "$WORK/root/Library/Audio/Plug-Ins/Components/Nova Link.component" "$WORK/root/Applications/Nova Link.app"; do
    codesign --verify --deep --strict "$b"
done

echo "==> Installer package"
COPYFILE_DISABLE=1 pkgbuild --root "$WORK/root" --identifier com.internationalnova.novalink.installer \
    --version "$VERSION" --install-location / "$WORK/unsigned.pkg" >/dev/null
# macOS tags every file written here with a protected attribute (com.apple.provenance) that
# pkgbuild stores as hidden "._" files. Rebuild the payload and file list without them.
pkgutil --expand "$WORK/unsigned.pkg" "$WORK/exp"
(cd "$WORK/root" && find . | COPYFILE_DISABLE=1 cpio -o --format odc -R 0:80 2>/dev/null) | gzip -c > "$WORK/exp/Payload"
lsbom "$WORK/exp/Bom" | grep -v '/\._' > "$WORK/bomlist"
rm "$WORK/exp/Bom"; mkbom -i "$WORK/bomlist" "$WORK/exp/Bom"
rm "$WORK/unsigned.pkg"; pkgutil --flatten "$WORK/exp" "$WORK/unsigned.pkg"
if pkgutil --payload-files "$WORK/unsigned.pkg" | grep -q '/\._'; then echo "ERROR: ._ files still in payload" >&2; exit 1; fi
productsign --sign "$INST_ID" "$WORK/unsigned.pkg" "$WORK/Install Nova Link.pkg" >/dev/null
echo "==> Notarizing installer (a few minutes)"
xcrun notarytool submit "$WORK/Install Nova Link.pkg" --keychain-profile "$NOTARY" --wait | tail -2
xcrun stapler staple "$WORK/Install Nova Link.pkg" >/dev/null

echo "==> Disk image"
mkdir -p "$WORK/dmg"
cp "$WORK/Install Nova Link.pkg" "$WORK/dmg/"
cat > "$WORK/dmg/Read Me.txt" <<TXT
Nova Link $VERSION

1. Double-click "Install Nova Link.pkg" and follow the steps.
   It installs the VST3 and AU plug-ins and the Nova Link app.
2. Engineer: put Nova Link on your master bus and click Start session.
   Send the session code to the artist.
3. Artist: put Nova Link on your vocal track (or open the Nova Link app),
   click Join session and type the code.

Allow microphone (and camera, for video) when macOS asks.
TXT
DMG="$OUT/Nova Link $VERSION.dmg"
rm -f "$DMG"
hdiutil create -volname "Nova Link" -srcfolder "$WORK/dmg" -fs HFS+ -format UDZO "$DMG" >/dev/null
codesign --force --timestamp --sign "$APP_ID" "$DMG"
echo "==> Notarizing disk image"
xcrun notarytool submit "$DMG" --keychain-profile "$NOTARY" --wait | tail -2
xcrun stapler staple "$DMG" >/dev/null
spctl -a -vv -t open --context context:primary-signature "$DMG" 2>&1 | head -2
rm -rf "$WORK"
echo "Done: $DMG"
