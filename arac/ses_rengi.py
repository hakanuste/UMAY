#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SES RENGI — bir hedef kaydin OLCULEBILIR ses rengini cikarir.

NE YAPAR
  Hedef kayit ile Piper cikisini ayni olcutlerle olcer ve aradaki
  farki bir duzeltme dosyasina yazar:
      perde orani   : hedefin perde ortancasi / Piper'in ortancasi
      bant kazanclari: hedefin tayf dengesine oturtan EQ egrisi

NE YAPMAZ — VE NEDEN
  Bu bir SES KLONU DEGILDIR. Belirli bir insanin ses kimligini
  yeniden uretmek icin konusmaci gommesi olan bir model gerekir;
  Piper'da bu, VITS'i o konusmacinin kayitlariyla INCE AYARDAN
  gecirmek demektir ve gereken sey 130 saniye degil, hizalanmis
  cevriyaziyla saatlerce temiz kayittir. Burada yapilan, kaydin
  OLCULEN akustik niteliklerine (perde kusagi ve tayf dengesi)
  oturtmaktir — ses rengi yaklasir, kimlik tasinmaz.

OLCULEN FARK (Soylev kaydi / Piper tr modeli, 300-1000 Hz = 0 dB)
      60- 200 Hz   -0.4  /  +5.9   -> Piper 6.3 dB fazla gur
     200- 300 Hz   -0.5  /  +1.8   -> 2.3 dB fazla
    1000-2000 Hz   -2.0  /  -9.6   -> Piper 7.6 dB KARANLIK
    2000-3000 Hz   -9.4  / -15.8   -> 6.3 dB karanlik
    3000-5000 Hz  -13.1  / -12.2   -> denk
    5000-8000 Hz  -14.8  /  -9.7   -> 5.2 dB fazla cizirtili
  1-2 kHz, anlasilirligi tasiyan banttir (ikinci formant bolgesi);
  oradaki 7.6 dB eksik yalniz tini degil, NETLIK meselesidir.

Kullanim:
  ses_rengi.py <hedef.wav> <piper_ornek.wav> <cikti.json>
"""
import json, sys
import numpy as np

# EQ bant kenarlari (Hz). Kenarlar, olculen bantlarla ayni.
BANTLAR = [(60, 200), (200, 300), (300, 1000), (1000, 2000),
           (2000, 3000), (3000, 5000), (5000, 8000)]
REFERANS = (300, 1000)          # 0 dB kabul edilen bant
EN_COK_KAZANC = 9.0             # dB; asiri duzeltme tiniyi bozar


def wav_oku(yol):
    import wave
    w = wave.open(yol)
    n, k = w.getnframes(), w.getnchannels()
    x = np.frombuffer(w.readframes(n), dtype='<i2').astype(np.float64)
    if k > 1:
        x = x.reshape(-1, k).mean(1)
    return x, w.getframerate()


def perde_ortanca(x, hz):
    """Ozilinti ile perde; yalnizca GUCLU cerceveler sayilir."""
    P, H = int(hz * 0.040), int(hz * 0.010)
    esik = np.percentile(np.abs(x), 70)
    f0 = []
    for i in range(0, len(x) - P, H):
        w = x[i:i + P]
        if np.abs(w).mean() < esik:
            continue
        w = w - w.mean()
        ac = np.correlate(w, w, 'full')[len(w) - 1:]
        lo, hi = int(hz / 250), int(hz / 60)
        if hi >= len(ac):
            continue
        k = lo + int(np.argmax(ac[lo:hi]))
        if ac[k] > 0.35 * ac[0]:
            f0.append(hz / k)
    return float(np.median(f0)) if f0 else 0.0


def ltas_db(x, hz, N=2048):
    """Uzun donem ortalama tayfi; referans banda gore dB."""
    k = len(x) // N
    if k < 1:
        raise ValueError('kayit cok kisa')
    S = (np.abs(np.fft.rfft(x[:k * N].reshape(k, N) * np.hanning(N),
                            axis=1)) ** 2).mean(0)
    fr = np.fft.rfftfreq(N, 1.0 / hz)
    r = S[(fr >= REFERANS[0]) & (fr < REFERANS[1])].sum()
    out = []
    for lo, hi in BANTLAR:
        sec = (fr >= lo) & (fr < hi)
        out.append(10 * np.log10(S[sec].sum() / r + 1e-12) if sec.any() else 0.0)
    return out


def yeniden_ornekle(x, oran):
    """soylet.py'deki perde kaydirmasinin AYNISI. Olcum, calisma
    zamaninda uygulanacak zincirin uzerinde yapilmak zorunda."""
    if abs(oran - 1.0) < 1e-3:
        return x
    n = int(round(len(x) / oran))
    if n < 2:
        return x
    t = np.linspace(0.0, len(x) - 1.0, n)
    i = np.clip(np.floor(t).astype(int), 0, len(x) - 2)
    f = t - i
    return x[i] * (1.0 - f) + x[i + 1] * f


def renk_cikar(hedef_yolu, piper_yolu):
    hx, hhz = wav_oku(hedef_yolu)
    px, phz = wav_oku(piper_yolu)

    hp, pp = perde_ortanca(hx, hhz), perde_ortanca(px, phz)
    oran = (hp / pp) if (hp > 0 and pp > 0) else 1.0

    # EQ, PERDE KAYDIRMASINDAN SONRAKI sinyale gore olculur.
    # Ilk surumde kaydirilmamis sinyal olculuyor ama duzeltme
    # kaydirilmis sinyale uygulaniyordu; yeniden ornekleme tayfi %12.5
    # yukari tasidigi icin bantlar kayiyordu. Olculen sonuc: 5-8 kHz
    # hedefi 3.8 dB ASTI (-18.6 yerine -14.8 olmaliydi). Duzeltmeyi
    # olcerken calisma zincirinin AYNISINI kurmak gerekiyor.
    px_kaydirilmis = yeniden_ornekle(px, oran)

    hl, pl = ltas_db(hx, hhz), ltas_db(px_kaydirilmis, phz)
    kazanc = []
    for i, (lo, hi) in enumerate(BANTLAR):
        g = hl[i] - pl[i]
        g = max(-EN_COK_KAZANC, min(EN_COK_KAZANC, g))
        kazanc.append(round(float(g), 2))

    return {
        'perde_hedef': round(hp, 1),
        'perde_piper': round(pp, 1),
        'perde_orani': round(float(oran), 4),
        'bantlar': [list(b) for b in BANTLAR],
        'kazanc_db': kazanc,
        'hedef_ltas_db': [round(v, 2) for v in hl],
        'piper_ltas_db': [round(v, 2) for v in pl],
        'not': ('Ses KLONU degildir: olculen perde kusagi ve tayf '
                'dengesine oturtur, konusmaci kimligini tasimaz.'),
    }


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(__doc__); sys.exit(2)
    r = renk_cikar(sys.argv[1], sys.argv[2])
    with open(sys.argv[3], 'w', encoding='utf-8') as f:
        json.dump(r, f, ensure_ascii=False, indent=2)
    print('perde: hedef %.1f Hz / piper %.1f Hz -> oran %.4f'
          % (r['perde_hedef'], r['perde_piper'], r['perde_orani']))
    print('bant kazanclari (dB):')
    for (lo, hi), g, h, p in zip(BANTLAR, r['kazanc_db'],
                                 r['hedef_ltas_db'], r['piper_ltas_db']):
        print('  %5d-%5d  hedef %+6.1f  piper %+6.1f  ->  %+5.1f'
              % (lo, hi, h, p, g))
    print('yazildi: %s' % sys.argv[3])
