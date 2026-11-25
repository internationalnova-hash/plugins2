#!/usr/bin/env bash
# Builds the one-shot "Nova All Access" installer: every store plugin in one signed,
# notarized macOS installer (dist-store/all-access.pkg, the name the store's downloads
# page links to). Run after build_store_pkgs.sh, which leaves each plugin's signed,
# notarized installer in dist-store/<store-id>.pkg.
#
# Usage: build_all_access_pkg.sh <store list>   (same list file as build_store_pkgs.sh)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT="$ROOT/dist-store"
WORK="$OUT/all-access-work"
INSTALLER_IDENTITY="Developer ID Installer: Giovanni Nova (HSX9N2UQ83)"
NOTARY_PROFILE="nova-audio-notary"
VERSION="${VERSION:-$(date +%Y.%m.%d)}"

rm -rf "$WORK"
mkdir -p "$WORK/packages"

count=0
while IFS='|' read -r id dir target product bundle; do
    [ -z "$id" ] && continue
    src="$OUT/$id.pkg"
    if [ ! -f "$src" ]; then echo "missing: $src" >&2; exit 1; fi
    cp "$src" "$WORK/packages/$id.pkg"
    count=$((count + 1))
done < "$1"
echo "==> $count plugin packages"

# Distribution: one choice per plugin, all selected, plus a welcome title
args=()
for p in "$WORK"/packages/*.pkg; do args+=(--package "$p"); done
productbuild --synthesize "${args[@]}" "$WORK/distribution.xml"
python3 - "$WORK/distribution.xml" "$VERSION" <<'PY'
import sys
path, version = sys.argv[1], sys.argv[2]
s = open(path).read()
s = s.replace('<installer-gui-script minSpecVersion="1">',
              '<installer-gui-script minSpecVersion="1">\n    <title>Nova All Access</title>\n'
              '    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>\n'
              '    <domains enable_localSystem="true"/>', 1)
open(path, "w").write(s)
PY

UNSIGNED="$WORK/all-access-unsigned.pkg"
SIGNED="$OUT/all-access.pkg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/packages" --version "$VERSION" "$UNSIGNED"
productsign --sign "$INSTALLER_IDENTITY" "$UNSIGNED" "$SIGNED"

echo "==> Notarizing ($(du -h "$SIGNED" | cut -f1))"
xcrun notarytool submit "$SIGNED" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$SIGNED"
spctl -a -vv -t install "$SIGNED"
echo "==> Done: $SIGNED"
