#!/usr/bin/env bash
# Nova Silk Linux Build Script
# Ensures all dependencies are installed and builds Nova Silk for Linux targets (VST3/LV2/Standalone)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NS_DIR="$SCRIPT_DIR/Nova Silk"

echo "=========================================="
echo "  Nova Silk Linux Build"
echo "=========================================="
echo ""

# Check/install system dependencies
echo "1. Checking system dependencies..."
if ! command -v xvfb-run &> /dev/null; then
  echo "   Installing xvfb and GTK/WebKit dev packages..."
  sudo apt-get update > /dev/null
  sudo apt-get install -y libgtk-3-dev libwebkit2gtk-4.1-dev xvfb > /dev/null
  echo "   ✓ Dependencies installed"
else
  echo "   ✓ All dependencies present"
fi

# Configure CMake
echo ""
echo "2. Configuring CMake..."
cd "$NS_DIR"
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
echo "   ✓ CMake configured"

# Build under virtual display
echo ""
echo "3. Building Nova Silk..."
xvfb-run -a cmake --build build --config Release -j$(nproc)
echo "   ✓ Build completed"

# Report artifacts
echo ""
echo "=========================================="
echo "  Build Complete"
echo "=========================================="
echo "Artifacts created at:"
echo "  VST3:      $NS_DIR/build/NovaSilk_artefacts/Release/VST3/"
echo "  LV2:       $NS_DIR/build/NovaSilk_artefacts/Release/LV2/"
echo "  Standalone: $NS_DIR/build/NovaSilk_artefacts/Release/Standalone/"
echo ""
