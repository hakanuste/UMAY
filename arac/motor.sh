#!/bin/bash
# =====================================================================
# UMAY gömülü motor yapımı (llama.cpp) — ANA BİLGİSAYAR için.
#   arac/motor.sh            motoru derler, kov/umay-motorlu üretir
#   arac/motor.sh --yerel    CPU'ya özel (-march=native) hızlı yapım (dağıtım için DEĞİL)
# Telefon/öteki platformlar için bkz. docs/MOTOR.md (Android NDK, iOS, macOS, Windows).
# Çekirdek C11 + -Werror kalır; yalnız bağlama c++ ile yapılır.
# =====================================================================
set -eu
cd "$(dirname "$0")/.."
YEREL=OFF
for a in "$@"; do case "$a" in --yerel) YEREL=ON ;; *) echo "bilinmeyen: $a"; exit 2 ;; esac; done

command -v cmake >/dev/null || { echo "HATA: cmake yok"; exit 1; }
GEN=""; command -v ninja >/dev/null && GEN="-G Ninja"
# Dağıtılabilir yapım: x86'da AVX2 tabanı (2013+ işlemciler); ARM'da varsayılan.
EK="-DGGML_NATIVE=$YEREL"
case "$(uname -m)" in x86_64|amd64) [ "$YEREL" = OFF ] && EK="$EK -DGGML_AVX2=ON -DGGML_FMA=ON -DGGML_F16C=ON -DGGML_AVX=ON" ;; esac
cmake -S motor -B kov/motor $GEN -DCMAKE_BUILD_TYPE=Release $EK >/dev/null
cmake --build kov/motor --target umay_motor -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

# çekirdek: us/umay_motor_yok.c YERİNE motor/umay_motor.cpp
KAYNAK=$(make -s liste-kaynak | tr ' ' '\n' | grep -v 'us/umay_motor_yok.c' | tr '\n' ' ')
mkdir -p kov/motorlu
OBJ=""
for d in $KAYNAK; do
  o="kov/motorlu/$(echo "$d" | tr / _).o"
  cc -std=c11 -Wall -Wextra -Werror -O2 -Iinclude -DUMAY_SES_ACIK=1 -c "$d" -o "$o"
  OBJ="$OBJ $o"
done
LIBS=$(find kov/motor -name '*.a' | sort | tr '\n' ' ')
# bağımlılık sırası: umay_motor → llama → ggml → ggml-cpu → ggml-base
ORD=""
for ad in libumay_motor libllama libggml libggml-cpu libggml-base; do
  for l in $LIBS; do [ "$(basename "$l" .a)" = "$ad" ] && ORD="$ORD $l"; done
done
c++ -o kov/umay-motorlu $OBJ $ORD -lm -lpthread
echo "kov/umay-motorlu hazır."
