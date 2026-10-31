#!/bin/bash
# UMAY'ı Mac'e KURAR: /Applications/Umay.app (Launchpad, Spotlight ve Dock'ta simgesiyle görünür).
# Kullanım: ./arac/mac_kur.sh            (kov/Umay.app yoksa önce derler)
#           ./arac/mac_kur.sh --derle    (her seferinde yeniden derler)
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = "--derle" ] || [ ! -d kov/Umay.app ]; then ./arac/mac_uygulama.sh; fi
HEDEF=/Applications/Umay.app
pkill -x Umay 2>/dev/null || true
rm -rf "$HEDEF" 2>/dev/null || sudo rm -rf "$HEDEF"
cp -R kov/Umay.app "$HEDEF" 2>/dev/null || sudo cp -R kov/Umay.app "$HEDEF"
codesign --force --sign - "$HEDEF" 2>/dev/null || true
touch "$HEDEF"
# simge ve Launchpad önbelleğini tazele
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$HEDEF" 2>/dev/null || true
defaults write com.apple.dock ResetLaunchPad -bool true 2>/dev/null || true
killall Dock 2>/dev/null || true
echo "kuruldu: $HEDEF"
echo "Aç: open -a Umay   (Dock'ta simgeye sağ tık → Seçenekler → Dock'ta Tut)"
echo "İlk açışta 'tanımlanamayan geliştirici' derse: Applications'ta sağ tık → Aç."
