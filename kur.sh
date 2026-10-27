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
MODEL=""
for a in "$@"; do
  case "$a" in
    --ses) SES=1 ;;
    --model) MODEL=oneri ;;
    --model=*) MODEL="${a#--model=}" ;;
    --onek=*) ONEK="${a#--onek=}" ;;
    --yardim|-y|-h)
      echo "kullanim: ./kur.sh [--ses] [--model[=kucuk|orta|buyuk]] [--onek=DIZIN]"
      echo "  --ses        Piper ses arka ucunu da kurar"
      echo "  --model[=kucuk|orta|buyuk]  gomulu LLM modelini indirir (bos: bellege gore)"
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
case "$(uname -s)" in
  Darwin) OS=macos; DERLEYICI_ONERI="xcode-select --install" ;;
  Linux)  OS=linux; DERLEYICI_ONERI="sudo apt install build-essential" ;;
  *)      OS=bilinmeyen; DERLEYICI_ONERI="C derleyicisi ve make kurun" ;;
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

# --- 6b) model ---
if [ -n "$MODEL" ]; then
  echo
  ./arac/model.sh "$MODEL" "$ONEK/share/umay/kut" || yaz "UYARI: model indirilemedi; UMAY yedek motorla calisir."
  yaz "NOT: model yalniz motorlu yapida kullanilir (./arac/motor.sh)."
fi

# --- 7) dogrula ---
echo
yaz "dogrulama:"
if "$ONEK/bin/umay" --surum >/dev/null 2>&1; then
  yaz "  surum : $("$ONEK/bin/umay" --surum)"
else
  hata "kurulan program calismiyor"
fi
# Tek soru kipinde stdout YALNIZ yanittir (banner basilmaz); eski sinama
# banner'daki "UMAY  " yazisini ariyordu, hic bulamayip her kurulumda
# YANLIS uyari veriyordu. Simdi: cikis kodu 0 ve yanit bos degil; ayrica
# bir alan sorusunda dayanak (veri yeri) bulunuyor mu.
yanit=$(cd /tmp && "$ONEK/bin/umay" "merhaba" </dev/null 2>/dev/null); kod=$?
if [ "$kod" -eq 0 ] && [ -n "$yanit" ]; then
  yaz "  baska dizinden calisiyor ve yanit veriyor"
else
  yaz "  UYARI: baska dizinden yanit alinamadi (cikis kodu: $kod)"
fi
dayanak=$(cd /tmp && "$ONEK/bin/umay" "gunes paneli nedir" </dev/null 2>/dev/null | grep -c "Dayana")
if [ "$dayanak" -gt 0 ]; then
  yaz "  veri yeri bulundu (yanit dayanak gosteriyor)"
else
  yaz "  UYARI: yanit dayanak gostermedi (veri yeri bulunamamis olabilir)"
fi

echo
yaz "hazir.  umay            -> konusma kipi"
yaz "        umay \"soru\"     -> tek soru"
yaz "        umay --yardim"
