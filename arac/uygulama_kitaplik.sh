#!/bin/bash
# UMAY uygulama kitaplığı (C çekirdek + gömülü llama.cpp motoru) — macOS ve iOS için.
#   arac/uygulama_kitaplik.sh macos        macOS (bu bilgisayarın işlemcisi; evrensel için: MAC_EVRENSEL=1)
#   arac/uygulama_kitaplik.sh ios          iPhone/iPad aygıtı (arm64)
#   arac/uygulama_kitaplik.sh ios-benzetim iOS benzetici (arm64 + x86_64)
#   arac/uygulama_kitaplik.sh xcframework  üçünü yapar ve kov/uyg/Umay.xcframework üretir (tam Xcode gerekir)
# Çıktı: kov/uyg/<hedef>/libumay_tum.a — çekirdek + motor tek arşivde.
# Gerekli: cmake, Xcode komut satırı araçları (iOS için tam Xcode: iphoneos SDK).
set -eu
cd "$(dirname "$0")/.."
[ "$(uname -s)" = Darwin ] || { echo "HATA: yalnız macOS'ta çalışır."; exit 1; }
command -v cmake >/dev/null || { echo "HATA: cmake yok."; exit 1; }

yap() {
  local hedef="$1" ek="$2"
  local dizin="kov/uyg/$hedef"
  mkdir -p "$dizin"
  # try_compile imzalama/çalıştırma istemesin diye statik kitaplık kipi
  cmake -S . -B "$dizin/cmake" -DCMAKE_BUILD_TYPE=Release -DUMAY_MOTOR=ON \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DGGML_NATIVE=OFF $ek >/dev/null
  cmake --build "$dizin/cmake" --target umay -j"$(sysctl -n hw.ncpu)"
  # çekirdek + motor + llama + ggml arşivlerini birleştir
  local arsivler
  arsivler=$(find "$dizin/cmake" -name '*.a' | sort)
  # shellcheck disable=SC2086
  libtool -static -o "$dizin/libumay_tum.a" $arsivler
  echo "  -> $dizin/libumay_tum.a ($(du -h "$dizin/libumay_tum.a" | cut -f1))"
}

mac()  { local m="$(uname -m)"; [ "${MAC_EVRENSEL:-0}" = 1 ] && m="arm64;x86_64"
         yap macos "-DCMAKE_OSX_ARCHITECTURES=$m -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0"; }
ios()  { yap ios "-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0"; }
iosb() { yap ios-benzetim "-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0"; }

case "${1:-}" in
  macos) mac ;;
  ios) ios ;;
  ios-benzetim) iosb ;;
  xcframework)
    MAC_EVRENSEL=1 mac; ios; iosb
    mkdir -p kov/uyg/baslik && cp include/umay_gomulu.h kov/uyg/baslik/
    rm -rf kov/uyg/Umay.xcframework
    xcodebuild -create-xcframework \
      -library kov/uyg/macos/libumay_tum.a -headers kov/uyg/baslik \
      -library kov/uyg/ios/libumay_tum.a -headers kov/uyg/baslik \
      -library kov/uyg/ios-benzetim/libumay_tum.a -headers kov/uyg/baslik \
      -output kov/uyg/Umay.xcframework
    echo "  -> kov/uyg/Umay.xcframework" ;;
  *) sed -n 2,9p "$0"; exit 2 ;;
esac
