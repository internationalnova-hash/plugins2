#!/usr/bin/env bash
# Builds + signs store installers for several plugins in one go.
# Each line of the list file: <store-id>|<plugin dir>|<cmake target>|<product name>|<bundle id>
# Signed pkgs are copied to dist-store/<store-id>.pkg (the name the store's
# download route expects). Set SKIP_NOTARIZE=1 to stop before notarization.
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT="$ROOT/dist-store"
LOGS="$OUT/logs"
mkdir -p "$LOGS"
ok=(); failed=()
while IFS='|' read -r id dir target product bundle; do
    [ -z "$id" ] && continue
    echo "==> $product"
    if "$SCRIPT_DIR/sign_and_notarize.sh" "$dir" "$target" "$product" "$bundle" > "$LOGS/$id.log" 2>&1; then
        pkg=$(ls -t "$ROOT/$dir/dist/"*.pkg 2>/dev/null | grep -v unsigned | head -1)
        cp "$pkg" "$OUT/$id.pkg" && ok+=("$id") && echo "    ok: $(du -h "$OUT/$id.pkg" | cut -f1)"
    else
        failed+=("$id"); echo "    FAILED (see $LOGS/$id.log)"; tail -3 "$LOGS/$id.log" | sed 's/^/    /'
    fi
done < "$1"
echo ""
echo "Built ${#ok[@]}: ${ok[*]:-}"
echo "Failed ${#failed[@]}: ${failed[*]:-}"
