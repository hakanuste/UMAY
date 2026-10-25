#!/usr/bin/env python3
"""C motorunun karşılaştıracağı referans değerleri üretir.
Model dosyasını GERİ AÇARAK okur: böylece iki taraf da aynı
nicelenmiş ağırlıkları görür, karşılaştırma gerçek olur."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from us1 import model_oku, ileri

if __name__ == "__main__":
    # Yollar ARGUMANLA gelir. Eskiden "/home/claude/us1/umay.us1" ve
    # /tmp gomuluydu: yalniz bir makinede anlami olan bir referans,
    # referans degildir.
    model = sys.argv[1] if len(sys.argv) > 1 else "kut/umay.us1"
    cikti = sys.argv[2] if len(sys.argv) > 2 else "kov"
    os.makedirs(cikti, exist_ok=True)
    m = model_oku(model)
    imler = [1, 5, 9, 2]
    y = ileri(m, imler)
    open(os.path.join(cikti, "referans_imler.txt"), "w").write(
        " ".join(map(str, imler)))
    with open(os.path.join(cikti, "referans_mantik.txt"), "w") as f:
        for t in range(len(imler)):
            f.write(" ".join(f"{v:.6f}" for v in y[t]) + "\n")
    print("referans:", model, y.shape, "genlik", round(float(abs(y).max()), 3))
