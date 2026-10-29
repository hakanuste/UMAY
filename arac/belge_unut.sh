#!/bin/bash
# UMAY'ın içeri aldığı bir belgeyi UNUTTURUR (yanlış alana dağılmışsa yeniden almak için).
#   arac/belge_unut.sh <kitap adından bir parça> [alan] [kütüphane dizini]
# alan verilirse (örn. yazit) belge yeniden alınırken parçaların HEPSİ o alana konur (tahmin yok).
# Kütüphane (varsayılan): ~/Library/Application Support/Umay/kut   (macOS uygulaması)
# Silinen: o belgeden gelen kabul-*.txt parçaları ve gelen/_islendi.txt satırı.
# gelen/ içindeki PDF'e dokunulmaz; uygulama açılınca yeniden alır.
set -eu
ARA="${1:?kullanım: $0 <kitap adından bir parça> [kütüphane]}"
ALAN="${2:-}"
KUT="${3:-$HOME/Library/Application Support/Umay/kut}"
[ -d "$KUT" ] || { echo "HATA: kütüphane yok: $KUT"; exit 1; }
say=0
while IFS= read -r -d '' f; do
  if grep -qi -- "^#kaynak:.*$ARA" "$f" 2>/dev/null; then rm -f "$f"; say=$((say+1)); fi
done < <(find "$KUT" -name 'kabul-*.txt' -print0)
ISL="$KUT/gelen/_islendi.txt"
if [ -f "$ISL" ]; then grep -vi -- "$ARA" "$ISL" > "$ISL.yeni" || true; mv "$ISL.yeni" "$ISL"; fi
ADL="$KUT/gelen/_adlar.txt"
if [ -n "$ALAN" ] && [ -f "$ADL" ]; then
  awk -F'\t' -v a="$ARA" -v k="$ALAN" 'BEGIN{IGNORECASE=1} { if (index(tolower($2), tolower(a))) print $1 "\t" $2 "\t" k; else print $0 }' "$ADL" > "$ADL.yeni" && mv "$ADL.yeni" "$ADL"
fi
echo "$say parça silindi; '$ARA' işlenmişler listesinden çıkarıldı. Uygulamayı açınca belge yeniden alınır."
