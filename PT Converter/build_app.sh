#!/usr/bin/env bash
# Builds PT Converter.app with the ptformat session reader (ptx2json) inside it,
# and installs it to /Applications.
#   ./build_app.sh            build + install
#   ./build_app.sh --no-install
set -euo pipefail
cd "$(dirname "$0")"
PTF="../_shared/third_party/ptformat"

echo "==> Session reader (ptx2json, Apple silicon + Intel)"
clang++ -O2 -std=c++17 -I"$PTF" -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 \
    -o "$PTF/ptx2json" "$PTF/ptx2json.cc" "$PTF/ptformat.cc"

echo "==> App"
xcodebuild -project PTConverter.xcodeproj -scheme PTConverter -configuration Release \
    -derivedDataPath build/xc ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO build -quiet
APP="build/xc/Build/Products/Release/PTConverter.app"

cp "$PTF/ptx2json" "$APP/Contents/MacOS/ptx2json"
codesign --force --sign - "$APP/Contents/MacOS/ptx2json"
codesign --force --deep --sign - "$APP"

if [[ "${1:-}" != "--no-install" ]]; then
    rm -rf "/Applications/PT Converter.app"
    cp -R "$APP" "/Applications/PT Converter.app"
    echo "==> Installed: /Applications/PT Converter.app"
fi
