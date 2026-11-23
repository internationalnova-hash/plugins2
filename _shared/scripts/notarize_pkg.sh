#!/usr/bin/env bash
# Notarizes and staples installer packages that sign_and_notarize.sh built
# with SKIP_NOTARIZE=1.  Usage: notarize_pkg.sh <signed.pkg> [more.pkg ...]
set -euo pipefail
NOTARY_PROFILE="nova-audio-notary"
for pkg in "$@"; do
    echo "==> Notarizing $(basename "$pkg")"
    xcrun notarytool submit "$pkg" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$pkg"
    spctl -a -vv -t install "$pkg"
done
