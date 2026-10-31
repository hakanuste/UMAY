#!/usr/bin/env python3
"""Kendi sesinden Piper eğitim verisi hazırla (Colab GPU'sunda ya da bilgisayarında çalışır).

Girdi : bir ses ya da video kütüğü (ör. Hakan_ses.mp4) — 30-60 dakika temiz konuşma
Çıktı : <cikti>/wavs/ses_0001.wav ...  (22050 Hz, tek kanal, 1,5-15 sn)
        <cikti>/metadata.csv            (LJSpeech: kimlik|metin)
Yöntem: ffmpeg sesi çıkarır; faster-whisper (Türkçe) konuşmayı yazıya döker ve cümle sınırlarını verir;
        her cümle ayrı kütük olur. Yazı makine çıktısıdır: metadata.csv'yi ELLE gözden geçir (yanlış yazılmış
        sözcükler eğitimi bozar).
Gizlilik: ses kaydın KENDİ sesindir. Bu kütükleri açık depoya (GitHub) KOYMA; .gitignore bunları dışlar.
Kullanım: pip install faster-whisper ; python3 hazirla.py Hakan_ses.mp4 veri"""
import os, subprocess, sys, wave, re

def main(girdi, cikti):
    os.makedirs(os.path.join(cikti, "wavs"), exist_ok=True)
    ham = os.path.join(cikti, "ham_22k.wav")
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", girdi, "-vn", "-ac", "1", "-ar", "22050", ham], check=True)

    from faster_whisper import WhisperModel
    model = WhisperModel("large-v3", device="cuda", compute_type="float16") if os.environ.get("GPU", "1") == "1" \
        else WhisperModel("small", device="cpu", compute_type="int8")
    parcalar, _ = model.transcribe(ham, language="tr", vad_filter=True, beam_size=5, condition_on_previous_text=False)

    with wave.open(ham, "rb") as w:
        fs, n = w.getframerate(), w.getnframes()
        veri = w.readframes(n)
    bps = 2
    satirlar, k = [], 0
    for p in parcalar:
        sure = p.end - p.start
        metin = re.sub(r"\s+", " ", p.text).strip()
        if sure < 1.5 or sure > 15 or len(metin) < 6 or p.no_speech_prob > 0.4:
            continue
        k += 1
        ad = f"ses_{k:04d}"
        a, b = int(max(0, p.start - 0.05) * fs) * bps, int(min(n / fs, p.end + 0.10) * fs) * bps
        with wave.open(os.path.join(cikti, "wavs", ad + ".wav"), "wb") as o:
            o.setnchannels(1); o.setsampwidth(bps); o.setframerate(fs); o.writeframes(veri[a:b])
        satirlar.append(f"{ad}|{metin}")
    with open(os.path.join(cikti, "metadata.csv"), "w", encoding="utf-8") as f:
        f.write("\n".join(satirlar) + "\n")
    os.remove(ham)
    print(f"{k} cümle yazıldı → {cikti}/metadata.csv ve wavs/. Şimdi metadata.csv'yi gözden geçir.")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("kullanım: hazirla.py <ses-ya-da-video> <çıktı-dizini>")
    main(sys.argv[1], sys.argv[2])
