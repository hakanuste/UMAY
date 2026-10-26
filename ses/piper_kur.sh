#!/bin/bash
# =====================================================================
# UMAY — Piper ses arka ucu kurulumu (ana bilgisayar)
#
# Piper: sinir agi tabanli (VITS) konusma sentezleyicisi. CPU'da
# gercek zamanli kosar, cevrim disidir, Turkce sesleri vardir.
#
# Bu betik ucunu de hazirlar: piper, Turkce ses modeli, ses calici.
# Sonra "make yerel" ile derlenen UMAY butun yanitlarini sesle soyler.
# =====================================================================
set -u
SES_DIZIN="${UMAY_SES_DIZIN:-$HOME/.local/share/umay/ses}"
SES_ADI="${UMAY_SES_ADI:-tr_TR-fahrettin-medium}"
KAYNAK="https://huggingface.co/rhasspy/piper-voices/resolve/main/tr/tr_TR"

yaz(){ printf '  %s\n' "$1"; }

echo "UMAY — Piper kurulumu"
echo

case "$(uname -s)" in
  Darwin)        OS=macos ;;
  Linux)         OS=linux ;;
  FreeBSD)       OS=freebsd ;;
  OpenBSD)       OS=openbsd ;;
  NetBSD)        OS=netbsd ;;
  MSYS*|MINGW*)  OS=windows-msys ;;
  CYGWIN*)       OS=windows-cygwin ;;
  *)             OS=bilinmeyen ;;
esac
yaz "isletim sistemi: $OS"

# 1) KONUSMA MOTORU — iki yol
# UMAY ikisini de kullanabilir: "piper" calistirilabiliri varsa onu,
# yoksa modeli DOGRUDAN ONNX ile kosar (ses/soylet.py). Piper ikilisi
# her yerde paketli degil; ozellikle Intel macOS'ta pip paketi
# sorunlu olabilir. Ikinci yol onu gereksiz kilar.
YOL=""
if command -v piper >/dev/null 2>&1; then
  yaz "piper zaten kurulu: $(command -v piper)"
  YOL=ikili
else
  yaz "piper ikilisi yok; dogrudan ONNX yolu deneniyor"
  EKSIK=""
  command -v python3   >/dev/null 2>&1 || EKSIK="$EKSIK python3"
  command -v espeak-ng >/dev/null 2>&1 || EKSIK="$EKSIK espeak-ng"
  python3 -c 'import onnxruntime' >/dev/null 2>&1 || EKSIK="$EKSIK onnxruntime"
  python3 -c 'import numpy'       >/dev/null 2>&1 || EKSIK="$EKSIK numpy"
  if [ -z "$EKSIK" ]; then
    yaz "dogrudan ONNX yolu hazir (python3 + onnxruntime + espeak-ng)"
    YOL=onnx
  else
    yaz "eksik:$EKSIK"
    if [ "$OS" = macos ]; then
      yaz "  brew install espeak-ng"
      yaz "  pip3 install numpy onnxruntime"
      yaz "  NOT: Intel Mac'te onnxruntime'in son surumleri x86_64"
      yaz "       tekerlegi yayinlamiyor olabilir. Olmazsa surum sabitle:"
      yaz "       pip3 install 'onnxruntime==1.19.2'"
    else
      yaz "  sudo apt install espeak-ng python3-numpy"
      yaz "  pip install onnxruntime"
    fi
    yaz "ya da piper ikilisini kurun: pip install --user piper-tts"
    exit 1
  fi
fi

# 2) ses modeli
mkdir -p "$SES_DIZIN"
if [ -f "$SES_DIZIN/$SES_ADI.onnx" ]; then
  yaz "ses modeli zaten var: $SES_DIZIN/$SES_ADI.onnx"
else
  # tr_TR-fahrettin-medium -> fahrettin/medium
  KISI=$(echo "$SES_ADI" | sed 's/^tr_TR-//; s/-[a-z]*$//')
  KALITE=$(echo "$SES_ADI" | sed 's/.*-//')
  yaz "ses modeli iniyor: $SES_ADI"
  for uzanti in onnx onnx.json; do
    curl -fL --progress-bar -o "$SES_DIZIN/$SES_ADI.$uzanti" \
         "$KAYNAK/$KISI/$KALITE/$SES_ADI.$uzanti" \
      || { yaz "INDIRILEMEDI: $SES_ADI.$uzanti"; exit 1; }
  done
fi

# 3) calici
CALICI=""
for c in afplay paplay aplay play ffplay; do
  if command -v "$c" >/dev/null 2>&1; then CALICI="$c"; break; fi
done
if [ -n "$CALICI" ]; then
  yaz "ses calici: $CALICI"
else
  if [ "$OS" = macos ]; then
    yaz "ses calici YOK. macOS'ta afplay sistemle gelir; PATH'i denetleyin."
  else
    yaz "ses calici YOK. Debian/Ubuntu: sudo apt install alsa-utils"
  fi
  exit 1
fi

# 4) soylet.py dogrudan ONNX yolu icin yerinde mi
if [ "$YOL" = onnx ]; then
  BURASI="$(cd "$(dirname "$0")" && pwd)"
  if [ -f "$BURASI/soylet.py" ]; then
    mkdir -p "$SES_DIZIN"
    cp "$BURASI/soylet.py" "$SES_DIZIN/soylet.py"
    yaz "soylet.py kuruldu: $SES_DIZIN/soylet.py"
  else
    yaz "UYARI: soylet.py bulunamadi ($BURASI)"
  fi
fi

echo
yaz "hazir. Deneme:"
if [ "$YOL" = ikili ]; then
  yaz "  echo 'Ötüken uyandı.' | piper --model $SES_DIZIN/$SES_ADI.onnx --output_file /tmp/d.wav && $CALICI /tmp/d.wav"
else
  yaz "  echo 'Ötüken uyandı.' | python3 -I $SES_DIZIN/soylet.py $SES_DIZIN/$SES_ADI.onnx $SES_DIZIN/$SES_ADI.onnx.json - /tmp/d.wav 1.52 && $CALICI /tmp/d.wav"
fi
echo
yaz "UMAY artik sesle yanitlar:  make yerel && ./kov/umay-yerel"
yaz "Baska ses istersen:  UMAY_PIPER_SES=/yol/ses.onnx ./kov/umay-yerel"
yaz "Dede Korkut agirligi:  UMAY_PIPER_UZUN=1.52  (buyuk deger = yavas)"
