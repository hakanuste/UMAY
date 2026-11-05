#!/usr/bin/env python3
"""UMAY'ın onaylı soru–yanıt çiftlerini yerel ince ayar (LoRA) verisine çevirir.

Kaynak : ~/Library/Application Support/Umay/egitim/onayli.jsonl   (yalnız "doğru, ekle" dediğin çiftler)
         isteğe bağlı: --sohbet  → sohbet.json içindeki aynı kalıp ("doğrulanmadı" → "doğru" → "Eklendi")
Çıktı  : ~/Library/Application Support/Umay/egitim/egitim.jsonl   (sohbet biçimi: system/user/assistant)
         ASLA depoya yazılmaz. Dosyayı Colab'a yüklemeden önce --onizle ile kendin oku.

Gizlilik süzgeci: parola/anahtar, kimlik, banka, kart, telefon, e-posta, sağlık, mülk ve para ayrıntısı içeren çiftler
ELENİR (içerik yazdırılmaz, yalnız neden ve sayı). Süzgeç tutucudur: şüphede eler.

Kullanım:
  python3 arac/egitim_verisi.py                 # onayli.jsonl → egitim.jsonl
  python3 arac/egitim_verisi.py --sohbet        # sohbet.json'dan da çiftleri al
  python3 arac/egitim_verisi.py --onizle 5      # ilk 5 çifti ekrana yaz (yazmadan önce incele)
  python3 arac/egitim_verisi.py --girdi X.jsonl --cikti Y.jsonl
"""
import argparse, json, os, re, sys

KOK = os.path.expanduser("~/Library/Application Support/Umay")
if not os.path.isdir(KOK):                       # Linux / deneme
    KOK = os.path.expanduser("~/.local/share/Umay")

KISILIK = ("Sen Dede Korkut'sun: bilge, ağırbaşlı, kısa ve doğrudan konuşan bir yardımcısın. "
           "Öz Türkçe kullan; en çok üç cümleyle yanıtla; bilmediğini uydurma.")

# (ad, düzenli ifade) — eşleşen çift elenir
# Türkçe ekler (parolam, ilacımı, maaşım…) için ifadeler yalnız SÖZCÜK BAŞINDA sınırlanır; sonu açık bırakılır.
SUZGEC = [
    ("parola/anahtar", r"\b(parola|şifre|sifre|password|passwd|api[ _-]?key|anahtar|token|sk_[A-Za-z0-9]{6,}|pin\s*kod)"),
    ("kimlik no", r"(?<!\d)\d{11}(?!\d)|\b(tc\s*kimlik|kimlik\s*no|pasaport\s*no)"),
    ("banka/iban", r"\bTR\d{2}[ ]?\d{4}|\b(iban|hesap\s*no|swift)"),
    ("kart no", r"(?<!\d)(?:\d[ -]?){13,19}(?!\d)"),
    ("telefon", r"(?<!\d)(?:\+?90|0)?[ ]?5\d{2}[ ]?\d{3}[ ]?\d{2}[ ]?\d{2}(?!\d)"),
    ("e-posta", r"[\w.+-]+@[\w-]+\.[\w.-]+"),
    ("sağlık", r"\b(tanı|teşhis|teshis|ilaç|ilac|ameliyat|hastalık|hastane|hastalig|kanser|diyabet|şeker\s*hastası|tansiyon|reçete|recete|tahlil|doz|tedavi|terapi|psikolog|depresyon|alerji|ağrı kesici)"),
    ("mülk/adres", r"\b(tapu|ada\s*/?\s*parsel|parsel\s*no|ada\s*no|ev\s*adres|mahalle\s*no|kapı\s*no|mülk|emlak\s*değer)|\bno\s*:\s*\d+\s*/\s*\d+"),
    ("para ayrıntısı", r"\b(maaş|maas|bakiye|borç|borc|kredi\s*kart|taksit|gelir\s*vergi|banka\s*hesab|birikim|yatırım\s*hesab)|\d[\d.]*\s*(tl|₺|lira|usd|eur|\$|€)(?!\w)"),
]
SUZGEC = [(a, re.compile(r, re.IGNORECASE)) for a, r in SUZGEC]

ATLA = ("İnternetten bulundu", "Doğruysa «doğru", "(Dayanağım", "Dayanağım ", "Güven:", "Dikkat:", "[")
def temizle(yanit):
    """Künye/üst bilgi satırlarını ve 'X söyler:' önekini at; yalnız yanıtın özü kalsın."""
    satir = []
    for s in yanit.splitlines():
        t = s.strip()
        if not t or any(t.startswith(a) for a in ATLA) or t.endswith("söyler:") or t.startswith("(Kütüphanemde kayıt yok"):
            continue
        satir.append(t.strip('"“”'))
    return " ".join(satir).strip()

def elenme_nedeni(*metinler):
    for ad, rx in SUZGEC:
        if any(rx.search(m) for m in metinler):
            return ad
    return None

def onayli_oku(yol):
    if not os.path.isfile(yol): return []
    out = []
    for s in open(yol, encoding="utf-8"):
        s = s.strip()
        if not s: continue
        try: j = json.loads(s)
        except ValueError: continue
        if isinstance(j, dict) and j.get("soru") and j.get("yanit"):
            out.append((j["soru"], j["yanit"]))
    return out

def sohbetten_oku(yol):
    """sohbet.json: [{benMi, metin, sistemMi}, ...]. Kalıp: soru → yanıt('doğrulanmadı') → 'doğru…' → 'Eklendi'."""
    if not os.path.isfile(yol): return []
    try: ileti = json.load(open(yol, encoding="utf-8"))
    except ValueError: return []
    out, soru, bekleyen = [], None, None
    for i in ileti:
        m, ben = i.get("metin", ""), i.get("benMi", False)
        if ben:
            k = m.lower().strip()
            if k.startswith(("doğru", "dogru")): continue
            if k.startswith(("yanlış", "yanlis")): bekleyen = None; continue
            soru = m
        else:
            if "doğrulanmadı" in m and soru: bekleyen = (soru, m)
            elif "Eklendi" in m and bekleyen: out.append(bekleyen); bekleyen = None
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--girdi", default=os.path.join(KOK, "egitim", "onayli.jsonl"))
    ap.add_argument("--cikti", default=os.path.join(KOK, "egitim", "egitim.jsonl"))
    ap.add_argument("--sohbet", action="store_true", help="sohbet.json'daki onaylı çiftleri de al")
    ap.add_argument("--onizle", type=int, default=0, help="ilk N çifti yazdır (dosyaya yazmaz)")
    ap.add_argument("--en-az", type=int, default=0, help="çift sayısı bundan azsa uyar")
    a = ap.parse_args()

    # güvenlik: çıktı depo içinde olmasın
    depo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if os.path.abspath(a.cikti).startswith(depo + os.sep):
        sys.exit("HATA: çıktı depo içine yazılamaz (kişisel veri). Başka bir yol ver.")

    ciftler = onayli_oku(a.girdi)
    if a.sohbet: ciftler += sohbetten_oku(os.path.join(KOK, "sohbet.json"))

    gorulen, temiz, elenen, bos = set(), [], {}, 0
    for soru, yanit in ciftler:
        soru, ozu = soru.strip(), temizle(yanit)
        if len(soru) < 3 or len(ozu) < 8: bos += 1; continue
        n = elenme_nedeni(soru, ozu)
        if n: elenen[n] = elenen.get(n, 0) + 1; continue
        anahtar = (soru.lower(), ozu.lower())
        if anahtar in gorulen: continue
        gorulen.add(anahtar); temiz.append((soru, ozu))

    print(f"okunan çift: {len(ciftler)} · kullanılabilir: {len(temiz)} · boş/kısa: {bos}")
    for ad, n in sorted(elenen.items()): print(f"  elenen ({ad}): {n}")
    if a.en_az and len(temiz) < a.en_az:
        print(f"UYARI: {len(temiz)} çift az; ince ayar için en az {a.en_az}+ (tercihen birkaç yüz) çift gerekir.")

    if a.onizle:
        for s, y in temiz[:a.onizle]: print(f"\nSORU : {s}\nYANIT: {y}")
        return
    if not temiz: sys.exit("Yazılacak çift yok.")
    os.makedirs(os.path.dirname(a.cikti), exist_ok=True)
    with open(a.cikti, "w", encoding="utf-8") as f:
        for s, y in temiz:
            f.write(json.dumps({"messages": [{"role": "system", "content": KISILIK},
                                             {"role": "user", "content": s},
                                             {"role": "assistant", "content": y}]}, ensure_ascii=False) + "\n")
    os.chmod(a.cikti, 0o600)
    print(f"yazıldı: {a.cikti}  ({len(temiz)} çift)\nColab'a yüklemeden önce: --onizle 10 ile oku.")

if __name__ == "__main__":
    main()
