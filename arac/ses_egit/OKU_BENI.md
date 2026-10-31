# Kendi sesinden Dede Korkut sesi (Piper ince ayarı)

**Neden bu yol:** Piper çevrim dışı çalışır ve eski Intel Mac'te gerçek zamanlıdır. Hazır Türkçe sesin
(`dfki`) denetim noktası (ckpt) açık: rhasspy/piper-checkpoints (veri kümesi) `tr/tr_TR/dfki/medium`.
Onun üstüne 30-60 dakikalık kendi sesinle **ince ayar** yapılır; sıfırdan eğitimden çok daha az veri ister.

**Gizlilik:** ses kayıtların açık depoya girmez (`.gitignore`). Colab'a yalnız kendi Drive'ından yüklersin.

## 1) Veriyi hazırla (Colab, GPU açık: Çalışma zamanı → T4)
```
!pip -q install faster-whisper
!apt -qq install -y ffmpeg
from google.colab import drive; drive.mount('/content/drive')
!git clone https://github.com/hakanuste/UMAY.git
!python3 UMAY/arac/ses_egit/hazirla.py /content/drive/MyDrive/Hakan_ses.mp4 /content/veri
```
`/content/veri/metadata.csv` makine yazısıdır: aç, yanlış yazılmış sözcükleri düzelt (özellikle özel adlar).
Kısa/boğuk/gürültülü satırları sil. Temiz 30+ dakika yeter.

## 2) İnce ayar (denenmemiş — Colab'da sırayla dene)
Yeni Piper eğiticisi (piper1-gpl):
```
!git clone https://github.com/OHF-Voice/piper1-gpl.git
%cd piper1-gpl
!pip -q install -e .[train]
!bash build_monotonic_align.sh      # betik adı sürüme göre değişebilir; depodaki TRAINING.md'ye bak
!huggingface-cli download rhasspy/piper-checkpoints --repo-type dataset \
    --include "tr/tr_TR/dfki/medium/*.ckpt" --local-dir /content/ckpt
!python3 -m piper.train fit \
    --data.voice_name hakan --data.csv_path /content/veri/metadata.csv --data.audio_dir /content/veri/wavs \
    --model.sample_rate 22050 --data.espeak_voice tr --data.cache_dir /content/cache \
    --data.config_path /content/cikti/config.json --data.batch_size 16 \
    --ckpt_path "$(ls /content/ckpt/tr/tr_TR/dfki/medium/*.ckpt)"
```
Birkaç yüz epoch (yaklaşık 1-3 saat T4) sonra dur. Bayrak adları sürüme göre değişebilir; doğrusu depodaki
TRAINING.md'dedir.

Eski eğitici (rhasspy/piper, `piper_train`) gerekirse Python 3.10 ortamı ister (`uv venv --python 3.10`).

## 3) ONNX'e çevir ve Mac'e koy
```
!python3 -m piper.train.export_onnx --checkpoint <en-son .ckpt> --output-file hakan.onnx
```
`hakan.onnx` ve `config.json`'u (adını `hakan.onnx.json` yap) Mac'te `~/.local/share/umay/ses/` içine koy.
Uygulama Ayarlar'da "Ses modeli" seçicisinde görünür.

## Dürüst beklenti
Bu senin sesinin klonudur. "Yaşlı bilge" tınısı için kayıtta yavaş ve sakin okumak, ya da uygulamadaki
"Dede Korkut tınısı" işlemini üstüne açmak gerekir. Sonuç kalitesini önceden bilemem; ilk deneme yetmezse
daha çok/temiz veriyle yinelenir.
