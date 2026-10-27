#!/bin/bash
# UMAY gomulu LLM modeli indirici.  kullanim: arac/model.sh [kucuk|orta|buyuk|ONERI] [KUT_DIZINI]
# Dosyalar Hugging Face'te dogrulandi (Qwen/Qwen3-*-GGUF). Indirme bitmeden dosya adi
# .gguf olmaz (yarim dosya model sanilmaz).
set -u
SEC="${1:-oneri}"; KUT="${2:-${UMAY_KUT:-kut}}"
bellek_mb(){
  if [ -r /proc/meminfo ]; then awk '/MemTotal/{print int($2/1024)}' /proc/meminfo
  elif command -v sysctl >/dev/null 2>&1; then echo $(( $(sysctl -n hw.memsize 2>/dev/null || echo 0) / 1048576 ))
  else echo 0; fi
}
if [ "$SEC" = oneri ]; then
  m=$(bellek_mb)
  if   [ "$m" -ge 12000 ]; then SEC=buyuk
  elif [ "$m" -ge 4000 ];  then SEC=orta
  else SEC=kucuk; fi
  echo "  bellek ${m} MB -> onerilen: $SEC"
fi
case "$SEC" in
  kucuk) R=Qwen/Qwen3-0.6B-GGUF; D=Qwen3-0.6B-Q8_0.gguf; B="0,6 GB" ;;
  orta)  R=Qwen/Qwen3-1.7B-GGUF; D=Qwen3-1.7B-Q8_0.gguf; B="1,8 GB" ;;
  buyuk) R=Qwen/Qwen3-4B-GGUF;   D=Qwen3-4B-Q4_K_M.gguf; B="2,5 GB" ;;
  *) echo "  HATA: bilinmeyen secim '$SEC' (kucuk|orta|buyuk|oneri)"; exit 2 ;;
esac
mkdir -p "$KUT/model" || { echo "  HATA: $KUT/model yaratilamadi"; exit 1; }
if [ -s "$KUT/model/$D" ]; then echo "  model zaten var: $KUT/model/$D"; exit 0; fi
U="https://huggingface.co/$R/resolve/main/$D"
echo "  indiriliyor ($B): $D"
if command -v curl >/dev/null 2>&1; then curl -fL --retry 3 -o "$KUT/model/$D.parca" "$U"
elif command -v wget >/dev/null 2>&1; then wget -O "$KUT/model/$D.parca" "$U"
else echo "  HATA: curl ya da wget gerekli"; exit 1; fi || { rm -f "$KUT/model/$D.parca"; echo "  HATA: indirme basarisiz"; exit 1; }
[ "$(head -c4 "$KUT/model/$D.parca")" = "GGUF" ] || { rm -f "$KUT/model/$D.parca"; echo "  HATA: inen dosya GGUF degil"; exit 1; }
mv "$KUT/model/$D.parca" "$KUT/model/$D" && echo "  hazir: $KUT/model/$D"
