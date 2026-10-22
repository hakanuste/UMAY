#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CEVIR — kitapligin Turkce olmayan parcalarini BIR KEZ Turkceye cevirir.

NEDEN SORGU ANINDA DEGIL
  Olculdu: kitapligin %33.4'u Ingilizcedir — 503 parca, 1.14 milyon
  karakter — ve neredeyse tamami "tulga" alanindadir (524 parcanin
  486'si: Musso ve M104 el kitaplari). Kullanici Turkce soruyor, ajan
  Ingilizce metin getiriyordu.

  Ceviriyi her soruda yapmak uc sebeple yanlis:
    1. Ayni parca yuzlerce kez cevrilir, her seferinde baska cikar.
    2. Sorgu gecikmesi cevirmene baglanir; sebekesiz yasamda cevirmen
       her zaman ayakta olmaz.
    3. Ceviri DENETLENEMEZ. Bir kez cevirip diske yazarsan, yanlisi
       gorup elle duzeltebilirsin; her soruda yeniden uretilirse
       duzeltme tutmaz.
  Bu yuzden ceviri bir KURULUM adimidir: bir kez kosar, sonucu
  kitapliga yazar, ozgun metni de yerinde BIRAKIR.

CIKTI DUZENI
  Cevrilen parca, ozguniyle AYNI kategoriye "<ad>-tr.txt" olarak
  yazilir ve kunyesine sunlar konur:
      #dil: tr
      #ozgun_dil: en
      #ozgun: b0301001.txt
  UMAY'in bilgi katmani bu alanlari okur ve yaniti verirken "ozgunden
  cevrilmis kayit" oldugunu SOYLER. Ozgun dosya silinmez: istersen
  aslina bakabilirsin, ajan da gerekirse onu gosterebilir.

ARKA UCLAR
  ollama   yerelde kosan Ollama (varsayilan). Model cok dilli olmali;
           qwen2.5, gemma2/3, llama3.1 gibi.
  komut    kendi cevirmenini ver: UMAY_CEVIR_KOMUT. Metin STDIN'den
           girer, Turkcesi STDOUT'tan beklenir.
  yok      hicbir sey cevirmez; yalnizca NE CEVRILECEGINI sayar.
           Once bununla kos, buyuklugu gor.

SAHTE BASARI YOK
  Cevirmen yoksa, yanit bossa ya da cikti hala Ingilizce gorunuyorsa
  o parca ATLANIR ve raporda sayilir. Cevrilmemis bir parcayi
  "cevrildi" diye yazmak, kullaniciya yanlis dilde kaynak vermekten
  kotudur.

Kullanim:
  cevir.py <kut_dizini> [--arka-uc ollama|komut|yok] [--model ad]
           [--en-cok N] [--kuru]
"""
import json, os, re, subprocess, sys, time
import urllib.request

OLLAMA = os.environ.get('UMAY_OLLAMA', '127.0.0.1:11434')
KOMUT = os.environ.get('UMAY_CEVIR_KOMUT', '')

TR_HARF = set('çğıöşüÇĞİÖŞÜ')
TR_SOZ = set('bir ve bu için ile olarak gibi daha çok var olan ise ancak '
             'kadar sonra de da ne her hem'.split())
EN_SOZ = set('the and for with this that from should must have been are '
             'was will not you your can'.split())

ISTEM = (
    "Asagidaki teknik metni TURKCEYE cevir.\n"
    "Kurallar:\n"
    "- Yalnizca ceviriyi yaz; aciklama, giris, baslik ekleme.\n"
    "- Sayilari, olcu birimlerini, parca numaralarini, kod ve model\n"
    "  adlarini AYNEN birak.\n"
    "- Tablo ve liste duzenini koru.\n"
    "- Anlamadigin bir terimi UYDURMA; oldugu gibi birak.\n"
    "- Oz Turkce sozcukleri yegele.\n\n"
    "METIN:\n"
)


def dil_sez(metin):
    kelime = re.findall(r"[a-zçğıöşü']+", metin.lower())
    if len(kelime) < 12:
        return 'belirsiz'
    tr = sum(1 for w in kelime if w in TR_SOZ) / len(kelime)
    en = sum(1 for w in kelime if w in EN_SOZ) / len(kelime)
    ozel = sum(1 for c in metin if c in TR_HARF) / max(1, len(metin))
    if tr + ozel * 3 > en * 1.2:
        return 'tr'
    if en > tr:
        return 'en'
    return 'belirsiz'


# --------------------------- arka uclar ---------------------------
def ollama_modeller():
    try:
        d = urllib.request.urlopen('http://%s/api/tags' % OLLAMA, timeout=10).read()
        return [m['name'] for m in json.loads(d).get('models', [])]
    except Exception:
        return []


def ollama_cevir(metin, model):
    govde = json.dumps({
        'model': model,
        'prompt': ISTEM + metin,
        'stream': False,
        'options': {'temperature': 0.2, 'num_predict': 2048},
    }).encode('utf-8')
    istek = urllib.request.Request('http://%s/api/generate' % OLLAMA, data=govde,
                                   headers={'Content-Type': 'application/json'})
    d = urllib.request.urlopen(istek, timeout=600).read()
    return json.loads(d).get('response', '').strip()


def komut_cevir(metin, komut):
    p = subprocess.run(komut, shell=True, input=metin, text=True,
                       capture_output=True, timeout=600)
    if p.returncode != 0:
        raise RuntimeError(p.stderr.strip()[:200] or 'komut basarisiz')
    return p.stdout.strip()


# --------------------------- kunye ---------------------------
def kunye_coz(t):
    sat = t.split('\n')
    i = 0
    alan = {}
    while i < len(sat) and sat[i].startswith('#'):
        if ':' in sat[i]:
            k, v = sat[i][1:].split(':', 1)
            alan[k.strip()] = v.strip()
        i += 1
    while i < len(sat) and not sat[i].strip():
        i += 1
    return alan, '\n'.join(sat[i:]).strip()


def kunye_yaz(alan, sira):
    out = []
    for k in sira:
        if k in alan:
            out.append('#%s: %s' % (k, alan[k]))
    for k in alan:
        if k not in sira:
            out.append('#%s: %s' % (k, alan[k]))
    return '\n'.join(out) + '\n\n'


SIRA = ['kategori', 'baslik', 'etiket', 'kaynak', 'dil', 'ozgun_dil', 'ozgun']


def kos(kut, arka_uc, model, en_cok, kuru):
    aday = []
    for kok, _, dosyalar in os.walk(kut):
        for d in sorted(dosyalar):
            if not d.endswith('.txt') or d.endswith('-tr.txt'):
                continue
            if d in ('sozluk.txt', 'ayar.txt', 'bengi.txt'):
                continue
            yol = os.path.join(kok, d)
            try:
                t = open(yol, encoding='utf-8').read()
            except Exception:
                continue
            alan, govde = kunye_coz(t)
            if alan.get('dil', 'tr') == 'tr' or not govde:
                continue
            # zaten cevrilmis mi
            hedef = os.path.join(kok, d[:-4] + '-tr.txt')
            if os.path.exists(hedef):
                continue
            aday.append((yol, hedef, alan, govde))

    print('cevrilecek parca : %d' % len(aday))
    print('toplam karakter  : %d' % sum(len(a[3]) for a in aday))
    kat = {}
    for _, _, alan, g in aday:
        k = alan.get('kategori', '?')
        kat[k] = kat.get(k, 0) + 1
    print('alanlara gore    : %s'
          % ', '.join('%s=%d' % kv for kv in sorted(kat.items())))
    if kuru or arka_uc == 'yok':
        print('\n(kuru kosu — hicbir sey cevrilmedi)')
        return 0, 0, len(aday)

    if en_cok:
        aday = aday[:en_cok]
        print('bu kosuda        : %d (--en-cok)' % len(aday))

    oldu = atlandi = 0
    t0 = time.time()
    for no, (yol, hedef, alan, govde) in enumerate(aday, 1):
        try:
            if arka_uc == 'ollama':
                tr = ollama_cevir(govde, model)
            else:
                tr = komut_cevir(govde, KOMUT)
        except Exception as e:
            print('  [ATLANDI] %s — cevirmen hatasi: %s'
                  % (os.path.basename(yol), str(e)[:90]))
            atlandi += 1
            continue

        # DENETIM: ceviri gercekten Turkce mi, ve bos/kisa degil mi
        if not tr or len(tr) < len(govde) * 0.3:
            print('  [ATLANDI] %s — cikti cok kisa (%d -> %d)'
                  % (os.path.basename(yol), len(govde), len(tr)))
            atlandi += 1
            continue
        if dil_sez(tr) == 'en':
            print('  [ATLANDI] %s — cikti hala Ingilizce gorunuyor'
                  % os.path.basename(yol))
            atlandi += 1
            continue

        y = dict(alan)
        y['ozgun_dil'] = alan.get('dil', 'en')
        y['dil'] = 'tr'
        y['ozgun'] = os.path.basename(yol)
        if 'baslik' in y:
            y['baslik'] = y['baslik'] + ' [TR]'
        with open(hedef, 'w', encoding='utf-8') as f:
            f.write(kunye_yaz(y, SIRA) + tr + '\n')
        oldu += 1
        if no % 10 == 0 or no == len(aday):
            gecen = time.time() - t0
            print('  %d/%d  oldu %d  atlandi %d  (%.1f sn/parca)'
                  % (no, len(aday), oldu, atlandi, gecen / no))
    return oldu, atlandi, len(aday)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(2)
    kut = sys.argv[1]
    arka_uc = 'ollama'
    model = os.environ.get('UMAY_OLLAMA_MODEL', '')
    en_cok = 0
    kuru = '--kuru' in sys.argv
    for i, a in enumerate(sys.argv):
        if a == '--arka-uc' and i + 1 < len(sys.argv): arka_uc = sys.argv[i + 1]
        if a == '--model'   and i + 1 < len(sys.argv): model = sys.argv[i + 1]
        if a == '--en-cok'  and i + 1 < len(sys.argv): en_cok = int(sys.argv[i + 1])
    if not os.path.isdir(kut):
        print('dizin yok: %s' % kut); sys.exit(2)

    if arka_uc == 'ollama' and not kuru:
        ms = ollama_modeller()
        if not ms:
            print('Ollama ulasilamadi (%s). Kurulu mu, kosuyor mu?' % OLLAMA)
            print('Kuru kosu icin: --kuru   ya da  --arka-uc yok')
            sys.exit(1)
        if not model:
            model = ms[0]
        if model not in ms and not any(m.startswith(model) for m in ms):
            print("'%s' kurulu degil. Kurulu olanlar: %s" % (model, ', '.join(ms)))
            sys.exit(1)
        print('arka uc: ollama / %s' % model)
    elif arka_uc == 'komut':
        if not KOMUT:
            print('UMAY_CEVIR_KOMUT verilmedi'); sys.exit(2)
        print('arka uc: komut / %s' % KOMUT)

    oldu, atlandi, toplam = kos(kut, arka_uc, model, en_cok, kuru)
    if not kuru and arka_uc != 'yok':
        print('\ncevrildi %d, atlandi %d, kalan %d'
              % (oldu, atlandi, toplam - oldu - atlandi))
        if atlandi:
            print('Atlananlar CEVRILMEDI; yeniden kosarsan tekrar denenir.')
