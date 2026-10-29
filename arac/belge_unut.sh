#!/usr/bin/env bash
# Bir belgeyi kütüphaneden unutturur (parçalarını siler), istenirse sonraki alımda zorunlu alan atar.
# Kullanım: arac/belge_unut.sh <ad parçası> [alan] [kütüphane]
# Ad parçası harf büyüklüğüne, Türkçe harflere (ü/u, ı/i…) ve '-'/boşluk farkına bakmaz.
set -eu
ARA="${1:?kullanım: $0 <kitap adından bir parça> [alan] [kütüphane]}"
ALAN="${2:-}"
KUT="${3:-$HOME/Library/Application Support/Umay/kut}"
[ -d "$KUT" ] || { echo "HATA: kütüphane yok: $KUT"; exit 1; }

# Ara sözcüğü, Türkçe harf ve ayraç farklarına dayanıklı bir desene çevir.
DESEN=$(printf '%s' "$ARA" | LC_ALL=C awk '
  BEGIN{
    m["u"]="(u|ü|U|Ü)"; m["U"]=m["u"]; m["ü"]=m["u"]; m["Ü"]=m["u"];
    m["o"]="(o|ö|O|Ö)"; m["O"]=m["o"]; m["ö"]=m["o"]; m["Ö"]=m["o"];
    m["c"]="(c|ç|C|Ç)"; m["C"]=m["c"]; m["ç"]=m["c"]; m["Ç"]=m["c"];
    m["s"]="(s|ş|S|Ş)"; m["S"]=m["s"]; m["ş"]=m["s"]; m["Ş"]=m["s"];
    m["g"]="(g|ğ|G|Ğ)"; m["G"]=m["g"]; m["ğ"]=m["g"]; m["Ğ"]=m["g"];
    m["i"]="(i|ı|I|İ)"; m["I"]=m["i"]; m["ı"]=m["i"]; m["İ"]=m["i"];
  }
  { s=$0; out="";
    while (length(s)>0) {
      # UTF-8 çok baytlı harfleri bütün al
      c=substr(s,1,1); n=1
      if (c ~ /[\300-\337]/) n=2; else if (c ~ /[\340-\357]/) n=3
      ch=substr(s,1,n); s=substr(s,n+1)
      if (ch in m) out=out m[ch]
      else if (ch ~ /^[A-Za-z]$/) out=out "[" tolower(ch) toupper(ch) "]"
      else if (ch ~ /^[-_ ]$/) out=out "[-_ ]"
      else if (ch ~ /^[][\\.*^$+?(){}|\/]$/) out=out "\\" ch
      else out=out ch
    }
    printf "%s", out }')

say=0
while IFS= read -r -d '' f; do
  if grep -Eq -- "^#kaynak:.*$DESEN" "$f" 2>/dev/null; then rm -f "$f"; say=$((say+1)); fi
done < <(find "$KUT" -name 'kabul-*.txt' -print0)

ISL="$KUT/gelen/_islendi.txt"
if [ -f "$ISL" ]; then grep -Ev -- "$DESEN" "$ISL" > "$ISL.yeni" || true; mv "$ISL.yeni" "$ISL"; fi

ADL="$KUT/gelen/_adlar.txt"
if [ -n "$ALAN" ] && [ -f "$ADL" ]; then
  : > "$ADL.yeni"
  while IFS= read -r satir || [ -n "$satir" ]; do
    if printf '%s\n' "$satir" | grep -Eq -- "$DESEN"; then
      k=$(printf '%s' "$satir" | cut -f1); o=$(printf '%s' "$satir" | cut -f2)
      printf '%s\t%s\t%s\n' "$k" "$o" "$ALAN" >> "$ADL.yeni"
    else
      printf '%s\n' "$satir" >> "$ADL.yeni"
    fi
  done < "$ADL"
  mv "$ADL.yeni" "$ADL"
fi
echo "$say parça silindi; '$ARA' işlenmişler listesinden çıkarıldı. Uygulamayı açınca belge yeniden alınır."
