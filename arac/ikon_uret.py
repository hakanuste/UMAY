#!/usr/bin/env python3
"""UMAY uygulama simgesi: docs/gorsel/umay-yapay-zeka-amblem.png -> Dock simgesi (Umay.icns) + arayüz amblemi.

Çıktılar (depoda tutulur; Mac'te iconutil gerekmez):
  uygulama/Umay/Umay.icns         macOS Dock/Finder simgesi (PNG parçalı .icns)
  uygulama/Umay/umay-amblem.png   uygulama içinde gösterilen amblem (512 px, saydam)
  uygulama/Umay/simge-1024.png    iOS/Android uygulama simgesi kaynağı
Amblem sahibinin kendi özgün çizimidir; yalnız kırpılır, ölçeklenir ve turkuaz mavi yuvarlak zemine oturtulur.
Kullanım: python3 arac/ikon_uret.py   (Pillow gerekir)"""
import io, struct, os
from PIL import Image, ImageDraw, ImageFilter

KOK = os.path.join(os.path.dirname(__file__), "..")
KAYNAK = os.path.join(KOK, "docs/gorsel/umay-yapay-zeka-amblem.png")
CIKTI = os.path.join(KOK, "uygulama/Umay")

am = Image.open(KAYNAK).convert("RGBA")
kutu = am.getchannel("A").point(lambda v: 255 if v > 24 else 0).getbbox()
am = am.crop(kutu)

def simge(boy=1024):
    tuval = Image.new("RGBA", (boy, boy), (0, 0, 0, 0))
    g = int(boy * 824 / 1024)            # macOS şablonu: 824 px gövde, çevresi gölgeye boş
    x0 = (boy - g) // 2
    # gövde: turkuaz mavi dikey geçiş
    zemin = Image.new("RGBA", (g, g))
    ust, alt = (47, 196, 200), (6, 112, 138)      # turkuaz mavi (üstte açık, altta derin)
    px = zemin.load()
    for y in range(g):
        t = y / (g - 1)
        r = int(ust[0] + (alt[0] - ust[0]) * t); gr = int(ust[1] + (alt[1] - ust[1]) * t); b = int(ust[2] + (alt[2] - ust[2]) * t)
        for x in range(g):
            px[x, y] = (r, gr, b, 255)
    maske = Image.new("L", (g, g), 0)
    ImageDraw.Draw(maske).rounded_rectangle((0, 0, g - 1, g - 1), radius=int(g * 0.225), fill=255)
    # gölge
    golge = Image.new("RGBA", (boy, boy), (0, 0, 0, 0))
    gm = Image.new("L", (boy, boy), 0)
    ImageDraw.Draw(gm).rounded_rectangle((x0, x0 + int(boy * 0.012), x0 + g, x0 + g + int(boy * 0.012)), radius=int(g * 0.225), fill=110)
    golge.putalpha(gm.filter(ImageFilter.GaussianBlur(boy * 0.012)))
    tuval = Image.alpha_composite(tuval, golge)
    tuval.paste(zemin, (x0, x0), maske)
    # ince altın çerçeve
    ImageDraw.Draw(tuval).rounded_rectangle((x0 + 3, x0 + 3, x0 + g - 4, x0 + g - 4), radius=int(g * 0.225) - 3, outline=(201, 162, 74, 150), width=max(2, boy // 256))
    # amblem: gövdenin ~%88'ine sığdır, biraz yukarı kaydır (alttaki yazı için)
    hedef = int(g * 0.90)
    oran = min(hedef / am.width, hedef / am.height)
    a = am.resize((int(am.width * oran), int(am.height * oran)), Image.LANCZOS)
    tuval.alpha_composite(a, ((boy - a.width) // 2, x0 + (g - a.height) // 2))
    return tuval

b1024 = simge(1024)
b1024.save(os.path.join(CIKTI, "simge-1024.png"))

# arayüz amblemi: saydam, kare, 512
kare = max(am.size)
tuv = Image.new("RGBA", (kare, kare), (0, 0, 0, 0))
tuv.alpha_composite(am, ((kare - am.width) // 2, (kare - am.height) // 2))
tuv.resize((512, 512), Image.LANCZOS).save(os.path.join(CIKTI, "umay-amblem.png"), optimize=True)

# .icns (PNG parçaları; macOS 10.7+)
parcalar = [("icp4", 16), ("icp5", 32), ("icp6", 64), ("ic07", 128), ("ic08", 256), ("ic09", 512), ("ic10", 1024),
            ("ic11", 32), ("ic12", 64), ("ic13", 256), ("ic14", 512)]   # son dördü @2x
veri = b""
for tur, boy in parcalar:
    t = io.BytesIO()
    b1024.resize((boy, boy), Image.LANCZOS).save(t, "PNG", optimize=True)
    d = t.getvalue()
    veri += tur.encode() + struct.pack(">I", len(d) + 8) + d
with open(os.path.join(CIKTI, "Umay.icns"), "wb") as f:
    f.write(b"icns" + struct.pack(">I", len(veri) + 8) + veri)

# ---- DEDE KORKUT: siyah zemini kaldır (köşelerden taşkın doldurma), saydam amblem + turkuaz yuvarlak simge ----
dk = Image.open(os.path.join(KOK, "docs/gorsel/dede-korkut-bilgelik-amblem.png")).convert("RGB")
esik = Image.new("L", dk.size, 255)
gri = dk.convert("L")
yarim = gri.point(lambda v: 255 if v <= 14 else 0)          # neredeyse siyah
zem = yarim.copy()
for k in ((0, 0), (dk.width - 1, 0), (0, dk.height - 1), (dk.width - 1, dk.height - 1)):
    ImageDraw.floodfill(zem, k, 128)
dis = zem.point(lambda v: 255 if v == 128 else 0)           # yalnız dıştan bağlı siyah
alfa = dis.point(lambda v: 0 if v else 255).filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(1.2))
dkr = dk.convert("RGBA"); dkr.putalpha(alfa)
dkr = dkr.crop(dkr.getchannel("A").point(lambda v: 255 if v > 24 else 0).getbbox())
kare2 = max(dkr.size)
t2 = Image.new("RGBA", (kare2, kare2), (0, 0, 0, 0))
t2.alpha_composite(dkr, ((kare2 - dkr.width) // 2, (kare2 - dkr.height) // 2))
t2.resize((768, 768), Image.LANCZOS).save(os.path.join(CIKTI, "dede-korkut-amblem.png"), optimize=True)
am = dkr
dksimge = simge(512)
dksimge.save(os.path.join(CIKTI, "dede-korkut-simge.png"), optimize=True)
print("tamam:", ", ".join(sorted(os.listdir(CIKTI))))
