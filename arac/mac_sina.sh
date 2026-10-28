#!/bin/bash
# macOS sınaması: derler, sınar, motoru kurar, rapor yazar.  kullanım: ./arac/mac_sina.sh [--model]
# Çıktının tamamı mac_rapor.txt dosyasına da yazılır; o dosyayı bana gönder.
cd "$(dirname "$0")/.." || exit 1
R=mac_rapor.txt
exec > >(tee "$R") 2>&1
ad(){ echo; echo "=== $1 ==="; }
ad "makine"
sw_vers 2>/dev/null; uname -mrs; sysctl -n machdep.cpu.brand_string 2>/dev/null
echo "bellek MB: $(( $(sysctl -n hw.memsize 2>/dev/null || echo 0) / 1048576 ))"
xcode-select -p 2>&1; cc --version 2>&1 | head -1
command -v cmake >/dev/null && cmake --version | head -1 || echo "cmake YOK (brew install cmake)"
ad "1) çekirdek derleme"
make clean >/dev/null 2>&1; make 2>&1 | tail -15; echo "make çıkış: ${PIPESTATUS[0]}"
ad "2) sınama"
make sina 2>&1 | grep -E "KALDI|SONUC|\[KALDI\]|error" | tail -20
ad "3) ÖTÜKEN dikişi"
make otuken-denetle 2>&1 | tail -2
ad "4) kısa konuşma (yedek motor)"
UMAY_OGREN=0 ./kov/umay "yer altı kileri kaç derece" 2>&1 | head -12
ad "5) gömülü motor yapımı"
if command -v cmake >/dev/null; then
  ( time ./arac/motor.sh ) 2>&1 | tail -25
else echo "cmake yok, atlandı"; fi
if [ "${1:-}" = "--model" ] && [ -x kov/umay-motorlu ]; then
  ad "6) model indir + soru"
  ./arac/model.sh kucuk kut
  ( time UMAY_OGREN=0 ./kov/umay-motorlu "yer altı kileri kaç derece" ) 2>&1 | head -25
fi
ad "bitti"
echo "Rapor: $(pwd)/$R"
