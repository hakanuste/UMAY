#!/bin/bash
# UMAY — cok platformlu derleme, sinama ve paketleme.
#
#   arac/capraz.sh              derle + calistirabildigini sina
#   arac/capraz.sh --paketle    ayrica kov/paket/ altina surum paketlerini uret
#
# Gerekli: zig (pip install ziglang  ya da  https://ziglang.org). Zig, tek
# makineden Windows, macOS ve Linux/Android(ARM64) icin derler.
# Sinama, bulunabilen calistiricilarla yapilir: Windows icin wine, ARM64 icin
# qemu-aarch64. Bulunamayan hedef YALNIZ DERLENIR ve tabloda "calistirilamadi"
# yazar — sessizce "gecti" sayilmaz.
set -u
cd "$(dirname "$0")/.."
PAKETLE=0; [ "${1:-}" = "--paketle" ] && PAKETLE=1

if command -v zig >/dev/null 2>&1; then ZIG="zig"
elif python3 -m ziglang version >/dev/null 2>&1; then ZIG="python3 -m ziglang"
else echo "HATA: zig yok (pip install ziglang)"; exit 2; fi

SURUM=$(sed -n 's/^#define UMAY_SURUM *"\(.*\)"/\1/p' include/umay.h)
KAYNAK=$(make -s liste-kaynak)
SINA=$(make -s liste-sina)
GOMULU=$(make -s liste-gomulu)
CFLAGS="-std=c11 -Wall -Wextra -Werror -Iinclude"

# CMakeLists.txt ile Makefile ayni kaynak listesini tasimali (kayma = sessiz kusur).
if [ "$(make -s liste-gomulu | tr ' ' '\n' | sort)" != \
     "$(grep -oE '[a-z_/0-9]+\.c' CMakeLists.txt | grep -v jni | sort -u)" ]; then
  echo "HATA: CMakeLists.txt kaynak listesi Makefile GOMULU_KAYNAK'tan ayrildi"; exit 1
fi

export WINEDEBUG=-all WINEPREFIX="${TMPDIR:-/tmp}/umay-wine"
WINE=""
for w in wine64 wine /usr/lib/wine/wine64; do command -v "$w" >/dev/null 2>&1 && { WINE="$w"; break; }; done
QEMU_ARM=""
for q in qemu-aarch64-static qemu-aarch64; do command -v "$q" >/dev/null 2>&1 && { QEMU_ARM="$q"; break; }; done

# ad : zig-hedefi : uzanti : ek-baglama : calistirici
HEDEFLER="
linux-x64:x86_64-linux-musl:::native
linux-arm64:aarch64-linux-musl:::arm
windows-x64:x86_64-windows-gnu:.exe:-lws2_32:wine
windows-arm64:aarch64-windows-gnu:.exe:-lws2_32:yok
macos-x64:x86_64-macos:::yok
macos-arm64:aarch64-macos:::yok
"
OUT=kov/capraz; mkdir -p "$OUT"
KOSAN=0; HATA=0
printf '%-14s %-9s %-22s %-22s\n' HEDEF DERLEME SINAMA GOMULU

kos() { # kos <calistirici> <komut...>
  local c="$1"; shift
  case "$c" in
    native) "$@" ;;
    wine)   [ -n "$WINE" ] && timeout 600 $WINE "$@" ;;
    arm)    [ -n "$QEMU_ARM" ] && timeout 600 $QEMU_ARM "$@" ;;
    *) return 99 ;;
  esac
}
var_mi() { case "$1" in native) return 0;; wine) [ -n "$WINE" ];; arm) [ -n "$QEMU_ARM" ];; *) return 1;; esac; }

echo "$HEDEFLER" | while IFS=: read -r ad hedef uz ek cal; do
  [ -z "$ad" ] && continue
  cal=$(echo "$cal" | tr -d ' ')
  d_ok="HATA"; s_ok="-"; g_ok="-"
  if $ZIG cc -target "$hedef" $CFLAGS -DUMAY_SES_ACIK=1 -o "$OUT/umay-$ad$uz" $KAYNAK $ek 2>"$OUT/$ad.derleme.log"; then
    d_ok="tamam"
    $ZIG cc -target "$hedef" $CFLAGS -DUMAY_SINAMA -o "$OUT/sina-$ad$uz" $SINA $ek 2>>"$OUT/$ad.derleme.log" || d_ok="HATA(sina)"
    GK=$(echo "$GOMULU")
    $ZIG cc -target "$hedef" $CFLAGS -o "$OUT/gomulu-$ad$uz" $GK sina/gomulu_sina.c $ek 2>>"$OUT/$ad.derleme.log" || d_ok="HATA(gomulu)"
  fi
  if [ "$d_ok" = "tamam" ]; then
    if var_mi "$cal"; then
      if r=$(kos "$cal" "$OUT/sina-$ad$uz" 2>&1 | tr -d '\r' | grep -a "SONUC:"); then
        s_ok=$(echo "$r" | sed 's/.*SONUC: *//; s/ *=* *$//'); echo "$r" | grep -q " 0 kaldi" || echo "SINA-KALDI $ad" >> "$OUT/hata.txt"
      else s_ok="CALISMADI"; [ "${UMAY_WINE_ESNEK:-0}" = 1 ] && case "$ad" in windows*) s_ok="wine-yok";; esac; [ "$s_ok" = CALISMADI ] && echo "SINA-CALISMADI $ad" >> "$OUT/hata.txt"; fi
      if r=$(kos "$cal" "$OUT/gomulu-$ad$uz" kut 2>&1 | tr -d "\r" | grep -a "GOMULU SONUC:"); then
        g_ok=$(echo "$r" | sed 's/.*SONUC: *//; s/ *=* *$//'); echo "$r" | grep -q " 0 kaldi" || echo "GOMULU-KALDI $ad" >> "$OUT/hata.txt"
      else g_ok="CALISMADI"; [ "${UMAY_WINE_ESNEK:-0}" = 1 ] && case "$ad" in windows*) g_ok="wine-yok";; esac; [ "$g_ok" = CALISMADI ] && echo "GOMULU-CALISMADI $ad" >> "$OUT/hata.txt"; fi
    else s_ok="calistirilamadi"; g_ok="calistirilamadi"; fi
  else echo "DERLEME-HATA $ad" >> "$OUT/hata.txt"; fi
  printf '%-14s %-9s %-22s %-22s\n' "$ad" "$d_ok" "$s_ok" "$g_ok"
done

if [ -s "$OUT/hata.txt" ]; then echo; echo "BASARISIZ:"; cat "$OUT/hata.txt"; rm -f "$OUT/hata.txt"; exit 1; fi
rm -f "$OUT/hata.txt"

if [ "$PAKETLE" = 1 ]; then
  P=kov/paket; rm -rf "$P"; mkdir -p "$P"
  for h in linux-x64 linux-arm64 windows-x64 windows-arm64 macos-x64 macos-arm64; do
    uz=""; case "$h" in windows-*) uz=".exe";; esac
    d="$P/umay-$SURUM-$h"; mkdir -p "$d"
    cp "$OUT/umay-$h$uz" "$d/umay$uz"
    # Program verisini kendi yaninda arar (kut/): tasinabilir kurulum.
    mkdir -p "$d/kut"; git ls-files kut | while read -r f; do mkdir -p "$d/$(dirname "$f")"; cp "$f" "$d/$f"; done
    cp README.md "$d/"
    case "$h" in
      windows-*) (cd "$P" && python3 -c "import shutil,sys; shutil.make_archive('umay-$SURUM-$h','zip','.','umay-$SURUM-$h')");;
      *)         tar -C "$P" -czf "$P/umay-$SURUM-$h.tar.gz" "umay-$SURUM-$h";;
    esac
    rm -rf "$d"
  done
  echo; echo "paketler: kov/paket/"; ls -1 "$P"
fi
