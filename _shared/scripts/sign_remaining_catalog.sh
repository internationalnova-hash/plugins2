#!/usr/bin/env bash
# Resume run: covers only the plugins not yet successfully signed as of the
# first sign_all_catalog.sh pass (11 already PASSed: Juice Gang, Nova AI
# Master, Nova Apex, Nova Aura, Nova Autotune, Nova Clean, Nova Clip, Nova
# Console, Nova Curve, Nova Delay, Nova Harmony - skipped here).
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIGN_SCRIPT="$SCRIPT_DIR/sign_and_notarize.sh"
LOG="$SCRIPT_DIR/sign_all_catalog.log"

PLUGINS=(
"Nova Doubler|NovaDoubler|Nova Doubler|com.internationalnova.novadoubler"
"Nova Heat|NovaHeat|Nova Heat|com.internationalnova.novaheat"
"Nova Keys|NovaKeys|Nova Keys|com.internationalnova.novakeys"
"Nova Level|NovaLevel|Nova Level|com.internationalnova.novalevel"
"Nova Master|NovaMaster|Nova Master|com.internationalnova.novamaster"
"Nova Motion FX|NovaMotionFX|Nova Motion FX|com.internationalnova.novamotionfx"
"Nova One|NovaOne|Nova One|com.internationalnova.novaone"
"Nova Silk|NovaSilk|Nova Silk|com.internationalnova.novasilk"
"Nova Tone|NovaTone|Nova Tone|com.internationalnova.novatone"
"Nova Translate|NovaTranslate|Nova Translate|com.internationalnova.novatranslate"
"Nova Voice|NovaVoice|Nova Voice|com.internationalnova.novavoice"
"Nova Vox|NovaVox|Nova Vox|com.internationalnova.novavox"
"Space By Nova|SpaceByNova|Nova Space|com.internationalnova.spacebynova"
)

echo "=== Resume signing run started $(date) ===" >> "$LOG"

for entry in "${PLUGINS[@]}"; do
    IFS='|' read -r DIR_NAME TARGET PRODUCT BUNDLE <<< "$entry"
    START=$(date +%s)
    echo "--- Starting $PRODUCT at $(date) ---" >> "$LOG"

    if "$SIGN_SCRIPT" "$DIR_NAME" "$TARGET" "$PRODUCT" "$BUNDLE" > "$SCRIPT_DIR/last_run_${TARGET}.log" 2>&1; then
        END=$(date +%s)
        echo "PASS  $PRODUCT  ($((END - START))s)" >> "$LOG"
    else
        END=$(date +%s)
        echo "FAIL  $PRODUCT  ($((END - START))s)  - see last_run_${TARGET}.log" >> "$LOG"
    fi
done

echo "=== Resume signing run finished $(date) ===" >> "$LOG"
