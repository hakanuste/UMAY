#!/bin/bash
# =====================================================================
# UMAY kurulumu
#
# Derler, kurar, istege bagli olarak sesi de hazirlar.
# Kurulmus bir programin kurulu oldugu sistemde sorunsuz calismasi
# icin ne gerekiyorsa burada denetlenir: derleyici, make, veri yeri,
# PATH. Eksik olan sessizce gecilmez, soylenir.
# =====================================================================
set -u
ONEK="${ONEK:-$HOME/.local}"
SES=0
for a in "$@"; do
  case "$a" in
    --ses) SES=1 ;;
    --onek=*) ONEK="${a#--onek=}" ;;
    --yardim|-y|-h)
      echo "kullanim: ./kur.sh [--ses] [--onek=DIZIN]"
      echo "  --ses        Piper ses arka ucunu da kurar"
      echo "  --onek=DIZIN varsayilan: \$HOME/.local"
      exit 0 ;;
    *) echo "bilinmeyen secenek: $a"; exit 2 ;;
  esac
done

yaz(){ printf '  %s\n' "$1"; }
hata(){ printf '  HATA: %s\n' "$1"; exit 1; }

echo "UMAY kurulumu  (onek: $ONEK)"
echo

# --- 1) gereksinimler ---
# Isletim sistemi: oneriler ona gore verilir. Eskiden her hata
# "Debian/Ubuntu: sudo apt install ..." diyordu; macOS'ta o oneri
# ise yaramaz.
# UMAY Linux'a degil POSIX'e dayanir; tanima listesi de oyle olmali.
# Eskiden yalniz Darwin ve Linux taniniyordu, geri kalan her sey
# "bilinmeyen" olup genel oneri aliyordu — oysa *BSD, MSYS2 ve Termux
# bu portla CALISIR, yalnizca derleyici onerisi baska.
case "$(uname -s)" in
  Darwin)        OS=macos;   DERLEYICI_ONERI="xcode-select --install" ;;
  Linux)
    # Android (Termux) da Linux cekirdegidir ama paket yoneticisi pkg.
    if [ -n "${PREFIX:-}" ] && [ -d "${PREFIX:-}/bin" ] &&
       command -v pkg >/dev/null 2>&1; then
      OS=android; DERLEYICI_ONERI="pkg install clang make"
    else
      OS=linux;   DERLEYICI_ONERI="sudo apt install build-essential"
    fi ;;
  FreeBSD)       OS=freebsd; DERLEYICI_ONERI="pkg install llvm gmake" ;;
  OpenBSD)       OS=openbsd; DERLEYICI_ONERI="pkg_add llvm gmake" ;;
  NetBSD)        OS=netbsd;  DERLEYICI_ONERI="pkgin install clang gmake" ;;
  MSYS*|MINGW*)  OS=windows-msys
                 DERLEYICI_ONERI="pacman -S mingw-w64-x86_64-gcc make" ;;
  CYGWIN*)       OS=windows-cygwin
                 DERLEYICI_ONERI="Cygwin kurulumundan gcc-core ve make" ;;
  SunOS)         OS=solaris; DERLEYICI_ONERI="pkg install gcc" ;;
  *)             OS=bilinmeyen
                 DERLEYICI_ONERI="C derleyicisi (c11) ve make kurun" ;;
esac
yaz "isletim sistemi: $OS"

command -v make >/dev/null 2>&1 || hata "make yok. $DERLEYICI_ONERI"
if   command -v cc  >/dev/null 2>&1; then DERLEYICI=cc
elif command -v clang >/dev/null 2>&1; then DERLEYICI=clang
elif command -v gcc >/dev/null 2>&1; then DERLEYICI=gcc
else hata "C derleyicisi yok. $DERLEYICI_ONERI"; fi
yaz "derleyici: $DERLEYICI"

# --- 2) derle ---
yaz "derleniyor..."
make CC="$DERLEYICI" >/dev/null || hata "derleme basarisiz. Ayrinti icin: make"
yaz "derlendi: kov/umay"

# --- 3) sina ---
yaz "sinamalar kosuluyor..."
if make CC="$DERLEYICI" sina >/tmp/umay_sina.txt 2>&1; then
  # IKI takim kosuyor (UMAY + US1 motoru); "tail -1" yalniz
  # ikincisini gosteriyordu ve "10 gecti" yaziyordu. Ikisi de yazilir.
  grep -o '[0-9]* gecti, [0-9]* kaldi' /tmp/umay_sina.txt | while read -r r; do
    yaz "$r"
  done
else
  yaz "UYARI: sinamalar gecmedi, ayrinti: /tmp/umay_sina.txt"
fi

# --- 4) kur ---
make CC="$DERLEYICI" kur ONEK="$ONEK" >/dev/null || hata "kurulum basarisiz"
yaz "kuruldu: $ONEK/bin/umay"
yaz "veri    : $ONEK/share/umay/kut"

# --- 5) PATH ---
case ":$PATH:" in
  *":$ONEK/bin:"*) yaz "PATH tamam" ;;
  *) echo
     yaz "UYARI: $ONEK/bin PATH'te degil. Kabuk ayarina ekleyin:"
     yaz "  echo 'export PATH=\"$ONEK/bin:\$PATH\"' >> ~/.bashrc" ;;
esac

# --- 6) ses ---
if [ "$SES" = 1 ]; then
  echo
  ./ses/piper_kur.sh || yaz "UYARI: ses kurulamadi; UMAY yaziyla calisir."
fi

# --- 7) dogrula ---
echo
yaz "dogrulama:"
if "$ONEK/bin/umay" --surum >/dev/null 2>&1; then
  yaz "  surum : $("$ONEK/bin/umay" --surum)"
else
  hata "kurulan program calismiyor"
fi
cik=$(cd /tmp && "$ONEK/bin/umay" "merhaba" </dev/null 2>&1 | grep -c "UMAY  ")
if [ "$cik" -gt 0 ]; then
  yaz "  baska dizinden calisiyor ve yanit veriyor"
else
  yaz "  UYARI: baska dizinden yanit alinamadi (veri yeri?)"
fi

echo
yaz "hazir.  umay            -> konusma kipi"
yaz "        umay \"soru\"     -> tek soru"
yaz "        umay --yardim"
