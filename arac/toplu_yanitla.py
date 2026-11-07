#!/usr/bin/env python3
"""sorular.txt'deki soruları UMAY'a sordurur, yanıtı sana gösterir; onayladıkların eğitim çifti olur.

Yanıt UMAY'ın kendi kütüphanesinden gelir (başka bir yapay zekâdan değil). Her çift için:
  e = onayla (onayli.jsonl'a eklenir)   h = at   d = yanıtı kendin düzelt   q = çık (kaldığın yer kaydedilir)
Gerekli: komut satırı UMAY ('./kur.sh' ile kurulur; tek soru kipi) ve gerçek bir dil modeli.
  python3 arac/toplu_yanitla.py --umay umay
  python3 arac/toplu_yanitla.py --umay ./kov/umay --sinir 30
"""
import argparse, json, os, subprocess, sys, importlib.util

BURASI = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("egitim_verisi", os.path.join(BURASI, "egitim_verisi.py"))
ev = importlib.util.module_from_spec(spec); spec.loader.exec_module(ev)
EGT = os.path.join(ev.KOK, "egitim")

def sor(umay, soru, sure):
    try:
        r = subprocess.run([umay, soru], capture_output=True, text=True, timeout=sure)
    except (OSError, subprocess.TimeoutExpired) as e:
        return ""
    return r.stdout.strip()

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--umay", default="umay")
    ap.add_argument("--sorular", default=os.path.join(EGT, "sorular.txt"))
    ap.add_argument("--onayli", default=os.path.join(EGT, "onayli.jsonl"))
    ap.add_argument("--ilerleme", default=os.path.join(EGT, "ilerleme.txt"))
    ap.add_argument("--sinir", type=int, default=0, help="bu oturumda en çok kaç soru")
    ap.add_argument("--sure", type=int, default=180, help="soru başına saniye sınırı")
    a = ap.parse_args()
    if not os.path.isfile(a.sorular): sys.exit(f"Soru dosyası yok: {a.sorular}\nÖnce: python3 arac/sohbet_iceaktar.py <dosya>")
    sorular = [s.strip() for s in open(a.sorular, encoding="utf-8") if s.strip()]
    basla = int(open(a.ilerleme).read().strip()) if os.path.isfile(a.ilerleme) else 0
    yapilan = 0
    for i in range(basla, len(sorular)):
        if a.sinir and yapilan >= a.sinir: break
        soru = sorular[i]
        print(f"\n[{i+1}/{len(sorular)}] SORU : {soru}")
        yanit = ev.temizle(sor(a.umay, soru, a.sure))
        if len(yanit) < 8: print("  (yanıt yok/çok kısa — atlandı)"); open(a.ilerleme, "w").write(str(i+1)); continue
        n = ev.elenme_nedeni(yanit)
        if n: print(f"  (yanıt gizlilik süzgecine takıldı: {n} — atlandı)"); open(a.ilerleme, "w").write(str(i+1)); continue
        print(f"        YANIT: {yanit}")
        while True:
            k = input("  [e]onayla  [h]at  [d]üzelt  [q]çık > ").strip().lower()[:1]
            if k in ("e", "h", "d", "q"): break
        if k == "q": break
        if k == "d":
            yanit = input("  doğru yanıt: ").strip()
            if len(yanit) < 8 or ev.elenme_nedeni(yanit): print("  (kısa ya da süzgece takıldı — atlandı)"); k = "h"
        if k in ("e", "d"):
            os.makedirs(EGT, exist_ok=True)
            with open(a.onayli, "a", encoding="utf-8") as f:
                f.write(json.dumps({"soru": soru, "yanit": yanit}, ensure_ascii=False) + "\n")
            print("  eklendi.")
        open(a.ilerleme, "w").write(str(i+1)); yapilan += 1
    print(f"\nkaldığın yer kaydedildi ({a.ilerleme}). Sonra: python3 arac/egitim_verisi.py --onizle 10")

if __name__ == "__main__":
    main()
