#!/usr/bin/env python3
"""UMAY ince ayar (LoRA) — hazır bir modele 'dayanaklı, öz Türkçe, uydurmayan' yanıt biçimini öğretir.
Gerçekler modele YAZILMAZ (onlar dayanak olarak gelir); model yalnız davranışı öğrenir.
KULLANIM (GPU'lu makine: Colab, HF Jobs, kendi kartın):
  umay --veri > veri.jsonl
  pip install torch transformers peft trl datasets accelerate
  python3 arac/ince_ayar.py veri.jsonl cikti/ [Qwen/Qwen3-1.7B]
Bu betik bu ortamda ÇALIŞTIRILMADI (GPU ve ağırlık yok); ilk koşuda sürüm uyumsuzluğu çıkabilir.
"""
import sys
if len(sys.argv) < 3:
    sys.exit(__doc__)
veri, cikti = sys.argv[1], sys.argv[2]
taban = sys.argv[3] if len(sys.argv) > 3 else "Qwen/Qwen3-1.7B"
import torch
from datasets import load_dataset
from peft import LoraConfig
from trl import SFTConfig, SFTTrainer
ds = load_dataset("json", data_files=veri, split="train").train_test_split(test_size=0.1, seed=7)
cfg = SFTConfig(output_dir=cikti, num_train_epochs=3, per_device_train_batch_size=2,
                gradient_accumulation_steps=8, learning_rate=1e-4, lr_scheduler_type="cosine",
                warmup_ratio=0.05, logging_steps=5, eval_strategy="epoch", save_strategy="epoch",
                bf16=torch.cuda.is_bf16_supported(), fp16=not torch.cuda.is_bf16_supported(), max_length=1536, assistant_only_loss=False)
lora = LoraConfig(r=16, lora_alpha=32, lora_dropout=0.05, target_modules="all-linear", task_type="CAUSAL_LM")
t = SFTTrainer(model=taban, args=cfg, train_dataset=ds["train"], eval_dataset=ds["test"], peft_config=lora)
t.train()
t.model = t.model.merge_and_unload()
t.model.save_pretrained(cikti + "/birlesik")
t.processing_class.save_pretrained(cikti + "/birlesik")
import os
hedef = os.environ.get("UMAY_HF_DEPO")          # örn. Hakanuste/umay-qwen3-1.7b  (HF_TOKEN gerekir)
if hedef:
    t.model.push_to_hub(hedef, private=True); t.processing_class.push_to_hub(hedef, private=True)
    print("HF'ye yüklendi (özel):", hedef)
print("bitti:", cikti + "/birlesik", "-> sonra GGUF'a çevir: docs/INCE_AYAR.md")
