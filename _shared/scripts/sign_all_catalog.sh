#!/usr/bin/env bash
# Runs sign_and_notarize.sh across the whole plugin catalog, sequentially
# (Apple's notary service + local disk/CPU contention make parallel runs
# risky), logging a per-plugin result line to $LOG so progress can be
# checked without tailing the full noisy build/notarize output.
#
# Excludes: Nova Pitch (standing instruction - user gave up on it after
# months, real bugs fixed but didn't fix the actual sound, do not include
# in catalog-wide batch tasks), Nova Sub and Nova Chord (already
# signed/notarized in this pass before this script existed).
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIGN_SCRIPT="$SCRIPT_DIR/sign_and_notarize.sh"
LOG="$SCRIPT_DIR/sign_all_catalog.log"

# dir-name | cmake-target | product-name | bundle-id
PLUGINS=(
"Juice Gang|JuiceGang|Juice Gang|com.internationalnova.juicegang"
"Nova AI Master|NovaAIMaster|Nova AI Master|com.internationalnova.novaaimaster"
"Nova Apex|NovaApex|Nova Apex|com.internationalnova.novaapex"
"Nova Aura|NovaAura|Nova Aura|com.internationalnova.novaaura"
"Nova Autotune|NovaAutotune|Nova Autotune|com.internationalnova.novaautotune"
"Nova Clean|NovaCleanV2|Nova Clean|com.internationalnova.novacleanv2"
"Nova Clip|NovaClip|Nova Clip|com.internationalnova.novaclip"
"Nova Console|NovaConsole|Nova Console|com.internationalnova.novaconsole"
"Nova Curve|NovaCurve|Nova Curve|com.internationalnova.novacurve"
"Nova Delay|NovaDelay|Nova Delay|com.internationalnova.novadelay"
"Nova Doubler|NovaDoubler|Nova Doubler|com.internationalnova.novadoubler"
"Nova Harmony|NovaHarmony|Nova Harmony|com.internationalnova.novaharmony"
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

echo "=== Batch signing run started $(date) ===" >> "$LOG"

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

echo "=== Batch signing run finished $(date) ===" >> "$LOG"
