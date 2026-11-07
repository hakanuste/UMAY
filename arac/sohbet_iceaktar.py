#!/usr/bin/env python3
"""Google Gemini sohbet dökümünden YALNIZ SENİN SORULARINI çıkarır (yanıtları almaz) ve gizlilik süzgecinden geçirir.

Neden yalnız sorular: Gemini'nin yanıtlarını başka bir modeli eğitmek için kullanmak Google'ın hizmet koşullarına aykırı
olabilir; senin kendi yazdıkların ise senindir. Yanıtları UMAY kendi kütüphanesinden üretir (arac/toplu_yanitla.py), sen onaylarsın.

Veri nasıl alınır: takeout.google.com → "Tümünü seç"i kaldır → "Etkinliğim" (My Activity) → Gemini Uygulamaları → JSON biçimi.
(Gemini web sayfasından elle kopyaladığın düz metin dosyası da olur: her soru ayrı satır/paragraf, --duz ile.)

Çıktı: ~/Library/Application Support/Umay/egitim/sorular.txt  (her satır bir soru; depoya yazılmaz).
Kullanım:
  python3 arac/sohbet_iceaktar.py MyActivity.json
  python3 arac/sohbet_iceaktar.py MyActivity.json --onizle 10
  python3 arac/sohbet_iceaktar.py MyActivity.json --on-ek "Şu istemi gönderdiniz: "   # ön ek tanınmazsa
  python3 arac/sohbet_iceaktar.py sohbet.txt --duz
Her şey bu bilgisayarda işlenir; hiçbir yere gönderilmez.
"""
import argparse, html, json, os, re, sys, importlib.util

BURASI = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("egitim_verisi", os.path.join(BURASI, "egitim_verisi.py"))
ev = importlib.util.module_from_spec(spec); spec.loader.exec_module(ev)

ON_EKLER = ["Prompted ", "Şu istemi gönderdiniz: ", "Şunu sordunuz: ", "İstem: ", "Sordunuz: ", "Şunu söylediniz: ", "Prompt: "]

def etiket_temizle(t):
    return html.unescape(re.sub(r"<[^>]+>", " ", t or "")).strip()

def json_sorular(yol, ek_on_ek):
    veri = json.load(open(yol, encoding="utf-8"))
    if isinstance(veri, dict): veri = veri.get("items") or veri.get("activities") or [veri]
    on_ekler = ([ek_on_ek] if ek_on_ek else []) + ON_EKLER
    out, taninmayan = [], 0
    for k in veri:
        if not isinstance(k, dict): continue
        baslik = str(k.get("header", ""))
        if baslik and "gemini" not in baslik.lower() and "bard" not in baslik.lower(): continue
        t = etiket_temizle(str(k.get("title", "")))
        if not t: continue
        for p in on_ekler:
            if t.startswith(p):
                out.append(t[len(p):].strip()); break
        else:
            taninmayan += 1
    return out, taninmayan, (etiket_temizle(str(veri[0].get("title", "")))[:14] if veri and isinstance(veri[0], dict) else "")

def duz_sorular(yol):
    metin = open(yol, encoding="utf-8").read()
    return [p.strip() for p in re.split(r"\n\s*\n|\n", metin) if len(p.strip()) >= 6], 0, ""

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("girdi")
    ap.add_argument("--duz", action="store_true", help="düz metin: her satır/paragraf bir soru")
    ap.add_argument("--on-ek", default="", help="Takeout başlığındaki soru ön eki (tanınmazsa)")
    ap.add_argument("--cikti", default=os.path.join(ev.KOK, "egitim", "sorular.txt"))
    ap.add_argument("--onizle", type=int, default=0)
    ap.add_argument("--en-kisa", type=int, default=8, help="bundan kısa sorular atılır")
    a = ap.parse_args()

    depo = os.path.dirname(BURASI)
    if os.path.abspath(a.cikti).startswith(depo + os.sep):
        sys.exit("HATA: çıktı depo içine yazılamaz (kişisel veri).")

    ham, tanimayan, ornek = duz_sorular(a.girdi) if a.duz else json_sorular(a.girdi, a.on_ek)
    gorulen, temiz, elenen, kisa = set(), [], {}, 0
    for s in ham:
        s = " ".join(s.split())
        if len(s) < a.en_kisa: kisa += 1; continue
        n = ev.elenme_nedeni(s)
        if n: elenen[n] = elenen.get(n, 0) + 1; continue
        if s.lower() in gorulen: continue
        gorulen.add(s.lower()); temiz.append(s)

    print(f"okunan soru: {len(ham)} · kullanılabilir: {len(temiz)} · kısa: {kisa} · ön eki tanınmayan kayıt: {tanimayan}")
    for ad, n in sorted(elenen.items()): print(f"  elenen ({ad}): {n}")
    if tanimayan and not ham:
        print(f"Hiç soru çıkmadı. İlk kaydın başlığı şu harflerle başlıyor: «{ornek}» — --on-ek ile ön eki ver.")
    if a.onizle:
        for s in temiz[:a.onizle]: print("  •", s[:160])
        return
    if not temiz: sys.exit("Yazılacak soru yok.")
    os.makedirs(os.path.dirname(a.cikti), exist_ok=True)
    with open(a.cikti, "w", encoding="utf-8") as f: f.write("\n".join(temiz) + "\n")
    os.chmod(a.cikti, 0o600)
    print(f"yazıldı: {a.cikti}\nSonraki adım: python3 arac/toplu_yanitla.py  (UMAY yanıtlasın, sen onayla)")

if __name__ == "__main__":
    main()
