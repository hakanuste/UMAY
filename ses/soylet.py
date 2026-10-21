#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SOYLET — Piper (VITS) Turkce sesiyle metni seslendirir.

Boru hatti:
  metin --espeak-ng--> IPA fonemleri --esleme--> kimlikler --ONNX--> dalga

Piper'in kendi duzeni: basa BOS ("^"), her fonemden SONRA dolgu ("_"),
sona EOS ("$"). Haritada olmayan isaretler atlanir.

Kullanim:
  soylet.py <model.onnx> <config.json> "<metin>" <cikti.wav> [uzunluk] [renk.json]

Metin yerine "-" verilirse STANDART GIRISTEN okunur. UMAY bu yolu
kullanir: tirnak, kesme isareti ve kabuk icin anlamli karakterler
komut satirina hic girmez.

SURE OLCERKEN DIKKAT (noise_w)
VITS'in sure yordayicisi RASTGELEDIR. noise_w = 0.3 ile ayni metin
kosudan kosuya %3 oynar:  4.73 / 4.44 / 4.53 sn.
Bu, uzunluk olceginin kucuk degisimlerini GIZLER — olculdu, 1.6'dan
1.52'ye inmek sureyi 13.78'den 13.72'ye dusurmus gorunuyordu (%0.4),
oysa gercek etki %5'tir. Olcek karsilastirmasi yapacaksan noise_w'yi
SIFIRLA; o zaman tam tek yonlu:
      uzun 0.5 -> 2.00 sn    1.0 -> 3.25    1.5 -> 4.34
      uzun 2.0 -> 5.46 sn    2.5 -> 6.70
Gunluk kullanimda noise_w yerinde kalir: o oynama konusmayi
dogallastirir.
"""
import json, subprocess, sys, unicodedata
import numpy as np
import onnxruntime as ort

# espeak'in affrikalari birlestirmek icin kullandigi baglayicilar
BAGLAYICI = {'‍', '͡', '͜', '͡', '⁀'}


def fonemlestir(metin, ses='tr'):
    """espeak-ng ile IPA fonem dizisi. Her sozcuk arasina bosluk kalir."""
    c = subprocess.run(['espeak-ng', '-v', ses, '-q', '--ipa=3', '--', metin],
                       capture_output=True, text=True)
    if c.returncode != 0:
        raise RuntimeError('espeak-ng basarisiz: ' + c.stderr.strip())
    ham = c.stdout.strip()
    # Satir sonlarini bosluga cevir, baglayicilari at
    ham = ham.replace('\n', ' ')
    return [k for k in ham if k not in BAGLAYICI]


def kimlige_cevir(fonemler, harita):
    bos, eos, dolgu = harita['^'], harita['$'], harita['_']
    kimlik = list(bos) + list(dolgu)
    atlanan = []
    for f in fonemler:
        if f not in harita:
            # Birlesik imleri ayristirmayi dene (orn. 'ɡ' yerine 'g')
            d = unicodedata.normalize('NFD', f)
            kondu = False
            for g in d:
                if g in harita:
                    kimlik.extend(harita[g]); kimlik.extend(dolgu)
                    kondu = True
            if not kondu:
                atlanan.append(f)
            continue
        kimlik.extend(harita[f])
        kimlik.extend(dolgu)
    kimlik.extend(eos)
    return kimlik, atlanan


# ---------------------------------------------------------------
# SES RENGI UYGULAMA
# arac/ses_rengi.py'nin cikardigi duzeltme burada uygulanir:
#   1. perde orani  -> yeniden ornekleme (perde VE formantlar kayar)
#   2. bant kazanci -> sifir evreli FIR ile tayf dengesi duzeltmesi
# Yeniden ornekleme sureyi de degistirir; cagiran length_scale'i ayni
# oranla carparak sureyi geri alir (soylet icinde yapilir).
# ---------------------------------------------------------------
def yeniden_ornekle(x, oran):
    """Perdeyi 'oran' kati yukseltir: 1/oran kadar ornek birakir.
    Dogrusal aradegerleme yeter; 2 kHz ustunde hafif yumusama olur,
    bant duzeltmesi zaten ondan sonra uygulanir."""
    if abs(oran - 1.0) < 1e-3:
        return x
    n = int(round(len(x) / oran))
    if n < 2:
        return x
    t = np.linspace(0.0, len(x) - 1.0, n)
    i = np.floor(t).astype(int)
    i = np.clip(i, 0, len(x) - 2)
    f = t - i
    return x[i] * (1.0 - f) + x[i + 1] * f


def fir_tasarla(bantlar, kazanc_db, hz, uzunluk=513):
    """Verilen bant kazanclarini saglayan sifir evreli FIR.

    Kazanc egrisi LOG frekansta aradegerlenir (kulagin olcegi), sonra
    tepkinin ters donusumu alinip Hann ile pencerelenir. Sifir evreli:
    konusmanin darbe yapisini kaydirmaz."""
    n = (uzunluk - 1) * 2
    fr = np.fft.rfftfreq(n, 1.0 / hz)
    # bant orta noktalari (log) ve kazanclari
    mx = np.array([np.sqrt(lo * hi) for lo, hi in bantlar])
    my = np.array(kazanc_db, dtype=np.float64)
    lf = np.log10(np.maximum(fr, 1.0))
    egri = np.interp(lf, np.log10(mx), my, left=my[0], right=my[-1])
    H = 10.0 ** (egri / 20.0)
    h = np.fft.irfft(H, n)
    h = np.roll(h, uzunluk // 2)[:uzunluk]
    return h * np.hanning(uzunluk)


def renk_uygula(x, hz, renk):
    oran = float(renk.get('perde_orani', 1.0))
    x = yeniden_ornekle(x, oran)
    bantlar = [tuple(b) for b in renk.get('bantlar', [])]
    kazanc = renk.get('kazanc_db', [])
    if bantlar and len(bantlar) == len(kazanc) and any(abs(g) > 0.1 for g in kazanc):
        h = fir_tasarla(bantlar, kazanc, hz)
        x = np.convolve(x, h, mode='same')
    tepe = np.abs(x).max()
    if tepe > 0.99:                  # duzeltme tepeyi buyutmus olabilir
        x = x * (0.99 / tepe)
    return x


def wav_yaz(yol, x, hz):
    import wave
    x = np.clip(x, -1.0, 1.0)
    k = (x * 32767.0).astype('<i2')
    w = wave.open(yol, 'wb')
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(hz)
    w.writeframes(k.tobytes()); w.close()


def soylet(model, ayar_yolu, metin, cikti, uzunluk=None, renk_yolu=None):
    ayar = json.load(open(ayar_yolu, encoding='utf-8'))
    renk = None
    if renk_yolu:
        renk = json.load(open(renk_yolu, encoding='utf-8'))
    harita = ayar['phoneme_id_map']
    hz = ayar['audio']['sample_rate']
    cik = ayar.get('inference', {})
    gurultu = float(cik.get('noise_scale', 0.667))
    uzun = float(uzunluk if uzunluk is not None else cik.get('length_scale', 1.0))
    # Yeniden ornekleme sureyi 1/oran kadar kisaltir; length_scale ayni
    # oranla carpilarak sure GERI ALINIR. Yoksa perde duzeltmesi
    # konusmayi hizlandirirdi.
    if renk:
        uzun *= float(renk.get('perde_orani', 1.0))
    gurultu_w = float(cik.get('noise_w', 0.8))

    fonemler = fonemlestir(metin, ayar.get('espeak', {}).get('voice', 'tr'))
    kimlik, atlanan = kimlige_cevir(fonemler, harita)
    if atlanan:
        print('  atlanan imler: %s' % ' '.join(sorted(set(atlanan))),
              file=sys.stderr)

    s = ort.InferenceSession(model, providers=['CPUExecutionProvider'])
    girdi = np.array([kimlik], dtype=np.int64)
    sonuc = s.run(None, {
        'input': girdi,
        'input_lengths': np.array([girdi.shape[1]], dtype=np.int64),
        'scales': np.array([gurultu, uzun, gurultu_w], dtype=np.float32),
    })[0].squeeze()
    if renk:
        sonuc = renk_uygula(sonuc, hz, renk)
    wav_yaz(cikti, sonuc, hz)
    return len(sonuc) / hz, hz, len(kimlik)


if __name__ == '__main__':
    if len(sys.argv) < 5:
        print(__doc__); sys.exit(2)
    u = float(sys.argv[5]) if len(sys.argv) > 5 else None
    metin = sys.argv[3]
    if metin == '-':
        metin = sys.stdin.read().strip()
        if not metin:
            print('bos metin', file=sys.stderr); sys.exit(1)
    renk = sys.argv[6] if len(sys.argv) > 6 else None
    sure, hz, n = soylet(sys.argv[1], sys.argv[2], metin, sys.argv[4], u, renk)
    print('%s  %.2f sn  %d Hz  %d fonem kimligi' % (sys.argv[4], sure, hz, n))
