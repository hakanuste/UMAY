#!/bin/bash
# macOS uygulaması (Umay.app) — YALNIZ Xcode komut satırı araçlarıyla (tam Xcode gerekmez).
# Kullanım: ./arac/mac_uygulama.sh [--model]   -> kov/Umay.app ; --model: orta modeli kut/model'e indirir
set -eu
cd "$(dirname "$0")/.."
command -v swiftc >/dev/null || { echo "HATA: swiftc yok (xcode-select --install)."; exit 1; }
./arac/uygulama_kitaplik.sh macos
APP=kov/Umay.app
rm -rf "$APP"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
# veri: kut/ (büyük modeller ve kullanıcı klasörleri hariç)
mkdir -p "$APP/Contents/Resources/kut"
for f in kut/*; do
  case "$(basename "$f")" in model|gelen|_ogren) continue ;; esac
  cp -R "$f" "$APP/Contents/Resources/kut/"
done
# simge ve amblem (arac/ikon_uret.py üretir; depoda hazır tutulur)
cp uygulama/Umay/Umay.icns uygulama/Umay/umay-amblem.png uygulama/Umay/dede-korkut-amblem.png uygulama/Umay/dede-korkut-simge.png uygulama/Umay/dede-korkut-yuz.png uygulama/Umay/umay-yuz.png uygulama/Umay/amblem-*.png "$APP/Contents/Resources/"
# kaynak belgeleri (docs/kaynak): ilk açılışta gelen/ klasörüne bırakılıp kendiliğinden içeri alınır
mkdir -p "$APP/Contents/Resources/kaynak"
cp docs/kaynak/*.txt "$APP/Contents/Resources/kaynak/" 2>/dev/null || true
swiftc -O -parse-as-library -target "$(uname -m)-apple-macos12.0" \
  -import-objc-header uygulama/Umay/UmayKopru.h -Xcc -Iinclude -Xcc -Ithird_party/monocypher \
  uygulama/Umay/UmayApp.swift uygulama/Umay/Motor.swift uygulama/Umay/SohbetGorunumu.swift uygulama/Umay/AyarGorunumu.swift uygulama/Umay/BelgeGorunumu.swift uygulama/Umay/Tarama.swift uygulama/Umay/KasaGorunumu.swift uygulama/Umay/Panolar.swift \
  kov/uyg/macos/libumay_tum.a -lc++ -o "$APP/Contents/MacOS/Umay"
cat > "$APP/Contents/Info.plist" <<'PL'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>UMAY</string>
<key>CFBundleDisplayName</key><string>UMAY</string>
<key>CFBundleIconFile</key><string>Umay</string>
<key>CFBundleShortVersionString</key><string>1.0</string>
<key>CFBundleIdentifier</key><string>org.umay.mac</string>
<key>CFBundleExecutable</key><string>Umay</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>1</string>
<key>LSMinimumSystemVersion</key><string>12.0</string>
<key>NSHighResolutionCapable</key><true/>
</dict></plist>
PL
codesign --force --sign - "$APP" 2>/dev/null || true     # yerel çalıştırma için geçici imza
touch "$APP"     # Finder/Dock simge önbelleğini tazeler
echo "hazır: $APP   (aç: open $APP)   Dock simgesi eski görünürse: killall Dock"
[ "${1:-}" = "--model" ] && ./arac/model.sh orta "$HOME/Library/Application Support/Umay/kut"
exit 0
