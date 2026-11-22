#!/bin/bash
# Copies the plugin UI into build/uipreview with a fake backend, for checking
# the UI in a normal browser:  python3 -m http.server 8765 --directory build/uipreview
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build/uipreview"
rm -rf "${OUT:?}"; mkdir -p "$OUT"
cp -R "$ROOT/Source/ui/public/." "$OUT/"
cp "$ROOT/tools/mock-backend.js" "$OUT/mock-backend.js"
sed -i '' 's#<script type="module" src="js/index.js"></script>#<script src="mock-backend.js"></script><script type="module" src="js/index.js"></script>#' "$OUT/index.html"
echo "UI preview ready in $OUT"
