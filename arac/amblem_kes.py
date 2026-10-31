#!/usr/bin/env python3
"""docs/gorsel/*-amblem.png dosyalarını uygulama için küçültür: uygulama/Umay/amblem-<ad>.png (192 px, saydam).
Kullanım: python3 arac/amblem_kes.py"""
import os
from PIL import Image
kok = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
girdi = os.path.join(kok, "docs", "gorsel")
cikti = os.path.join(kok, "uygulama", "Umay")
adlar = {"asena": "asena-kurtarma", "bitig": "bitig-dosya-sistemi", "ergenekon": "ergenekon-yeniden-kurulum",
         "kervan": "kervan-paket-yonetimi", "kut": "kut-yetkilendirme", "orkun": "orkun-gelistirici-araclari",
         "tamga": "tamga-dijital-kimlik", "timur": "timur-guvenlik", "tore": "tore-guvenlik-politikalari",
         "tulpar": "tulpar-web-tarayici", "ulak": "ulak-ag", "ulgen": "ulgen"}
for kisa, dosya in adlar.items():
    im = Image.open(os.path.join(girdi, dosya + "-amblem.png")).convert("RGBA")
    k = im.getbbox() or (0, 0) + im.size          # saydam kenarları kırp
    im = im.crop(k)
    im.thumbnail((192, 192), Image.LANCZOS)
    kare = Image.new("RGBA", (192, 192), (0, 0, 0, 0))
    kare.paste(im, ((192 - im.width) // 2, (192 - im.height) // 2), im)
    kare.save(os.path.join(cikti, f"amblem-{kisa}.png"), optimize=True)
    print("amblem-%s.png" % kisa)
