#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
EGIT_DERLEM — US1 modelini GERCEK derlem uzerinde egitir.

Onceki egitim ~900 karakterlik bir ornek metinle yapilmisti; model
Turkcenin harf istatistigini bile ogrenemezdi. Bu betik kitapligin
tamamini derlem olarak kullanir.

MOTORUN SINIRLARI (us/us1.h) asilmaz:
    d <= 96, katman <= 4, ffn <= 256, sozluk <= 128, baglam <= 96
Bu sinirlar icindeki en buyuk model ~400 bin degiskendir.

DURUST BEKLENTI: 400 bin degiskenli, karakter duzeyinde bir model
Turkcenin harf dizilimini, unlu uyumunu ve sik sozcuk bicimlerini
ogrenir. Anlamli cumle KURMAZ. Tutarli metin ureten en kucuk modeller
yuz milyon degisken dolayindadir — bunun iki yuz katindan fazla.
Buradaki kazanim, egitim hattinin ucdan uca kanitlanmasi ve gercek
bir taban olcumudur.

Kullanim:
    python3 arac/egit_derlem.py <kut_dizini> <cikti_dizini> [adim] [--sifirdan]

Varsayilan olarak kontrol noktasindan SURDURUR. --sifirdan ile bastan.
"""
import os, sys, time, json, collections
import numpy as np

BURASI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(BURASI, 'us1'))
from us1 import Kucuk, model_yaz          # noqa: E402
import egit as egitim                      # ileri_ara / geri  # noqa: E402

# Motor sinirlari icindeki en buyuk model
D, KATMAN, BAS, FFN, BAGLAM = 96, 4, 4, 256, 96
SOZLUK_AZAMI = 128
DOGRULAMA_ORAN = 0.02


def derlem_oku(kut):
    """Parca dosyalarinin GOVDESINI toplar; kunye satirlari atilir."""
    metinler = []
    for kok, _, dosyalar in os.walk(kut):
        for d in sorted(dosyalar):
            if not d.endswith('.txt') or d in ('sozluk.txt', 'ayar.txt',
                                               'bengi.txt'):
                continue
            yol = os.path.join(kok, d)
            try:
                t = open(yol, encoding='utf-8').read()
            except Exception:
                continue
            # kunye blogu: bastaki '#' satirlari ve ardindaki bos satir
            satirlar = t.split('\n')
            i = 0
            while i < len(satirlar) and satirlar[i].startswith('#'):
                i += 1
            while i < len(satirlar) and not satirlar[i].strip():
                i += 1
            govde = '\n'.join(satirlar[i:]).strip()
            if len(govde) > 50:
                metinler.append(govde)
    return '\n\n'.join(metinler)


def sozluk_kur(metin):
    """En sik SOZLUK_AZAMI-1 karakter; gerisi tek bir 'bilinmeyen'e."""
    say = collections.Counter(metin)
    sik = [c for c, _ in say.most_common(SOZLUK_AZAMI - 1)]
    sik = sorted(sik)
    harfler = sik + ['\x1a']                  # son yuva: bilinmeyen
    im = {c: i for i, c in enumerate(harfler)}
    bilinmeyen = len(harfler) - 1
    kapsam = sum(say[c] for c in sik) / max(1, len(metin))
    return harfler, im, bilinmeyen, kapsam


def kayip_olc(m, dizi, im_sayi=40, tohum=99):
    """Ortalama capraz-entropi; dogrulama diliminden ornekler."""
    r = np.random.RandomState(tohum)
    toplam = 0.0
    for _ in range(im_sayi):
        i = r.randint(0, len(dizi) - BAGLAM - 1)
        ara = egitim.ileri_ara(m, dizi[i:i + BAGLAM])
        k, _, _ = egitim.geri(m, ara, dizi[i + 1:i + BAGLAM + 1])
        toplam += k
    return toplam / im_sayi


KONTROL = 'kontrol.npz'


def kontrol_yaz(yol, par, mu, vu, t, en_iyi, rs):
    """TAM DUYARLIKLI egitim kontrol noktasi.

    NEDEN AYRI DOSYA: umay.us1 DAGITIM modelidir, satir basina
    simetrik 8 bitle NICELENMIS. Egitimi oradan surdurmek her
    duraklamada niceleme hatasi biriktirir; ustelik Adam'in momentleri
    ve adim sayaci orada hic yok. Kontrol noktasi float32 agirliklari,
    mu/vu momentlerini, adim sayacini ve ornekleyici durumunu tutar.

    NEDEN EKLENDI: egitim 65.000. adimda durdu (kap askiya alindi) ve
    SURDURULEMIYORDU — egit() her cagrida sifirdan yeni model
    kuruyordu. Yani 26 saat once yapilan 65.000 adim, yeniden
    baslatmakla cope gidecekti.
    """
    d = {}
    for i, p in enumerate(par):
        d['p%d' % i] = p
        d['m%d' % i] = mu[i]
        d['v%d' % i] = vu[i]
    d['t'] = np.array([t], dtype=np.int64)
    d['en_iyi'] = np.array([en_iyi], dtype=np.float64)
    d['rs_anahtar'] = rs[1]
    d['rs_konum'] = np.array([rs[2]], dtype=np.int64)
    # np.savez, adi .npz ile bitmiyorsa SONUNA EKLER; gecici adi
    # bastan .npz ile bitirip oyle tasiyoruz. Yazma yarida kesilirse
    # (kap askiya alinabilir) asil dosya bozulmasin diye once gecici,
    # sonra atomik tasima.
    gecici = yol + '.yeni.npz'
    with open(gecici, 'wb') as f:
        np.savez(f, **d)
    os.replace(gecici, yol)


def kontrol_oku(yol, par, mu, vu):
    """Kontrol noktasini YERINDE yukler. Bicim uyusmazsa 0 doner."""
    z = np.load(yol)
    for i, p in enumerate(par):
        q = z['p%d' % i]
        if q.shape != p.shape:
            return 0, float('inf'), None
        p[...] = q
        mu[i][...] = z['m%d' % i]
        vu[i][...] = z['v%d' % i]
    t = int(z['t'][0])
    en_iyi = float(z['en_iyi'][0])
    rs = ('MT19937', z['rs_anahtar'], int(z['rs_konum'][0]))
    return t, en_iyi, rs


def egit(dizi_egit, dizi_dog, sozluk, adim, cikti, gunluk, surdur=True):
    m = Kucuk(sozluk=sozluk, d=D, katman=KATMAN, bas=BAS,
              ffn=FFN, baglam=BAGLAM)
    par = m.parametreler()
    degisken = sum(p.size for p in par)
    yaz(gunluk, 'model: d=%d katman=%d bas=%d ffn=%d baglam=%d sozluk=%d'
        % (D, KATMAN, BAS, FFN, BAGLAM, sozluk))
    yaz(gunluk, 'degisken sayisi: %d' % degisken)

    mu = [np.zeros_like(p) for p in par]
    vu = [np.zeros_like(p) for p in par]
    r = np.random.RandomState(1)
    bas_adim = 0
    en_iyi = float('inf')

    kyol = os.path.join(cikti, KONTROL)
    if surdur and os.path.exists(kyol):
        try:
            bas_adim, en_iyi, rs = kontrol_oku(kyol, par, mu, vu)
            if bas_adim and rs:
                r.set_state(rs)
                yaz(gunluk, 'SURDURULDU: %d. adimdan, en iyi dogrulama %.4f'
                    % (bas_adim, en_iyi))
            else:
                bas_adim, en_iyi = 0, float('inf')
                yaz(gunluk, 'kontrol noktasi bicimi uymadi, sifirdan')
        except Exception as e:
            bas_adim, en_iyi = 0, float('inf')
            yaz(gunluk, 'kontrol noktasi okunamadi (%s), sifirdan' % e)
    elif surdur and os.path.exists(os.path.join(cikti, 'umay.us1')):
        # Kontrol noktasi yok ama NICELENMIS model var: sicak baslangic.
        # Niceleme hatasi (satir basina ~%0.4) bir kerelik bedeldir ve
        # birkac bin adimda kapanir; 65.000 adimi atmaktan iyidir.
        try:
            from us1 import model_oku
            eski = model_oku(os.path.join(cikti, 'umay.us1'))
            if eski.d == D and eski.katman == KATMAN and eski.sozluk == sozluk:
                for p, q in zip(par, eski.parametreler()):
                    if p.shape == q.shape:
                        p[...] = q
                durum = os.path.join(cikti, 'durum.json')
                if os.path.exists(durum):
                    j = json.load(open(durum))
                    bas_adim = int(j.get('adim', 0))
                    en_iyi = float(j.get('dogrulama', float('inf')))
                yaz(gunluk, 'SICAK BASLANGIC: nicelenmis modelden, '
                            '%d. adim, dogrulama %.4f '
                            '(Adam momentleri sifirdan)' % (bas_adim, en_iyi))
        except Exception as e:
            yaz(gunluk, 'sicak baslangic olmadi (%s)' % e)

    t0 = time.time()
    if bas_adim >= adim:
        yaz(gunluk, 'hedef adim zaten asildi (%d >= %d)' % (bas_adim, adim))
        return m, en_iyi

    for t in range(bas_adim + 1, adim + 1):
        # ogrenme orani: isinma + kosinus inis
        if t < 2000:
            oran = 1e-3 * t / 2000.0
        else:
            ilerleme = (t - 2000) / max(1, adim - 2000)
            oran = 1e-3 * (0.05 + 0.95 * 0.5 * (1 + np.cos(np.pi * ilerleme)))

        i = r.randint(0, len(dizi_egit) - BAGLAM - 1)
        ara = egitim.ileri_ara(m, dizi_egit[i:i + BAGLAM])
        kayip, g, gk = egitim.geri(m, ara, dizi_egit[i + 1:i + BAGLAM + 1])

        grad = [g['gomme']]
        for j in range(m.katman):
            grad += [gk[j][a] for a in ('n1', 'wq', 'wk', 'wv', 'wo',
                                        'n2', 'w1', 'w2', 'w3')]
        grad += [g['son_norm'], g['cikis']]
        for p, gr, mm, vv in zip(par, grad, mu, vu):
            np.clip(gr, -1.0, 1.0, out=gr)
            mm *= 0.9; mm += 0.1 * gr
            vv *= 0.999; vv += 0.001 * gr * gr
            p -= oran * (mm / (1 - 0.9 ** t)) / (np.sqrt(vv / (1 - 0.999 ** t)) + 1e-8)

        if t % 5000 == 0 or t == adim:
            dk = kayip_olc(m, dizi_dog)
            gecen = time.time() - t0
            yaz(gunluk, 'adim %7d/%d  egitim %.4f  dogrulama %.4f  '
                        'oran %.2e  %.0f im/sn  gecen %.0f sn'
                % (t, adim, kayip, dk, oran,
                   (t - bas_adim) * BAGLAM / max(gecen, 1e-9), gecen))
            # Kontrol noktasi HER OLCUMDE yazilir (en iyi olmasa da):
            # amaci en iyi modeli korumak degil, ILERLEMEYI kaybetmemek.
            kontrol_yaz(kyol, par, mu, vu, t, en_iyi, r.get_state())
            # EN IYI dogrulama kaydedilir: asiri uyum olursa son model
            # degil, en iyisi kalir.
            if dk < en_iyi:
                en_iyi = dk
                boy = model_yaz(m, os.path.join(cikti, 'umay.us1'))
                with open(os.path.join(cikti, 'durum.json'), 'w') as f:
                    json.dump({'adim': t, 'dogrulama': float(dk),
                               'egitim': float(kayip), 'bayt': boy,
                               'degisken': int(degisken)}, f)
                kontrol_yaz(kyol, par, mu, vu, t, en_iyi, r.get_state())
    return m, en_iyi


def yaz(gunluk, s):
    print(s, flush=True)
    with open(gunluk, 'a', encoding='utf-8') as f:
        f.write(s + '\n')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('kullanim: egit_derlem.py <kut_dizini> <cikti_dizini> [adim]')
        sys.exit(2)
    kut, cikti = sys.argv[1], sys.argv[2]
    adim = int(sys.argv[3]) if len(sys.argv) > 3 else 400000
    os.makedirs(cikti, exist_ok=True)
    gunluk = os.path.join(cikti, 'egitim.log')

    metin = derlem_oku(kut)
    yaz(gunluk, 'derlem: %d karakter' % len(metin))
    if len(metin) < 10000:
        yaz(gunluk, 'DERLEM COK KUCUK - egitim anlamsiz olur, duruldu.')
        sys.exit(1)

    harfler, im, bilinmeyen, kapsam = sozluk_kur(metin)
    yaz(gunluk, 'sozluk: %d im, kapsam %%%.2f' % (len(harfler), kapsam * 100))
    dizi = np.array([im.get(c, bilinmeyen) for c in metin], dtype=np.int32)

    kes = int(len(dizi) * (1 - DOGRULAMA_ORAN))
    dizi_egit, dizi_dog = dizi[:kes], dizi[kes:]
    yaz(gunluk, 'egitim %d im, dogrulama %d im' % (len(dizi_egit), len(dizi_dog)))
    yaz(gunluk, 'adim %d  (~%.1f tur)' % (adim, adim * BAGLAM / len(dizi_egit)))

    with open(os.path.join(cikti, 'sozluk.txt'), 'w', encoding='utf-8') as f:
        f.write(''.join(harfler))

    surdur = '--sifirdan' not in sys.argv
    m, en_iyi = egit(dizi_egit, dizi_dog, len(harfler), adim, cikti,
                     gunluk, surdur)
    yaz(gunluk, 'BITTI. en iyi dogrulama kaybi: %.4f' % en_iyi)
