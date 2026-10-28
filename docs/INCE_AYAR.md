# Seçenek 2 — hazır modeli ince ayarlamak

**Amaç:** modele bilgi yüklemek değil; davranışı öğretmek: yalnız dayanağa göre yanıtla, yetmiyorsa "bulamadım" de, öz Türkçe yaz. Gerçekler yine bilgi deposundan (dayanak) gelir; böylece model güncel kalır, yanılsaması azalır.

## Adımlar
1. Veri: `umay --veri > veri.jsonl` (bilgi deposundan sohbet biçimli örnek; yabancı sözcük içeren yanıtlar atılır, her kayıt için bir "bulamadım" örneği eklenir). Kendi belgelerini önce `kut/gelen/` ile içeri al — veri o kadar çoğalır.
2. Eğitim (GPU gerekir; bu ortamda yok): `python3 arac/ince_ayar.py veri.jsonl cikti/ Qwen/Qwen3-1.7B` — LoRA, 3 tur; `cikti/birlesik` dizinine birleşmiş model yazılır.
3. GGUF: llama.cpp'nin `convert_hf_to_gguf.py` ve `llama-quantize` araçlarıyla (örn. Q8_0). Dosyayı `kut/model/` altına koy; UMAY ilk `.gguf`'u bulur.
4. Ölçüt: aynı sorularda eski/yeni modeli karşılaştır (cevap dayanağa sadık mı, yabancı sözcük kaldı mı, "bulamadım" doğru yerde mi). Kötüleştiyse eskisine dön.

## Dürüst sınırlar
- Şu an örnek sayısı çok az (≈90, kütüphane büyüdükçe artar). Bu davranış öğretmeye yeter, bilgi öğretmeye yetmez.
- Yanıtlar dayanak cümlelerinden seçilir (çıkarıcı); model özet/yorum becerisini bu veriyle kazanmaz. Daha iyisi için daha büyük bir öğretmen modelle veri üretmek gerekir.
- `ince_ayar.py` denenmedi. Sonuç kalitesi bilinmiyor.

## Hugging Face / Colab ile (ekran kartı olmadan kendi bilgisayarında değil, bulutta)
HF hesabı `Hakanuste` Pro değil; HF Jobs ücretli ekran kartı ister (koşulları hesabında kontrol et). Ücretsiz yol: Google Colab (T4, 16 GB) — betik bf16 yoksa kendiliğinden fp16'ya geçer.
1. Mac'te `umay --veri > veri.jsonl` (kendi belgelerinle).
2. Colab → Çalışma zamanı türü: T4 GPU. `veri.jsonl` ve `arac/ince_ayar.py` dosyalarını yükle.
3. `!pip install -q peft trl datasets accelerate` sonra `!python ince_ayar.py veri.jsonl cikti Qwen/Qwen3-1.7B`
4. Modeli HF'ye özel depo olarak yüklemek için: `HF_TOKEN` (yazma izinli) ve `UMAY_HF_DEPO=Hakanuste/umay-qwen3-1.7b` ortam değişkenlerini ver.
5. Birleşmiş modeli GGUF'a çevir (llama.cpp `convert_hf_to_gguf.py`, sonra `llama-quantize … Q8_0`), `kut/model/` içine koy.
