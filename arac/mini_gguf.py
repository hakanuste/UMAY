#!/usr/bin/env python3
"""Rastgele ağırlıklı MİNİK GGUF üretir — YALNIZ motor boru hattını (yükle → belirteçle →
üret) sınamak için. Anlamlı metin üretmez; gerçek model değildir.
Kullanım: mini_gguf.py <cikti.gguf>"""
import sys, numpy as np, gguf

cikti = sys.argv[1]
rng = np.random.default_rng(7)
kelimeler = ["▁kerpiç", "▁duvar", "▁için", "▁saman", "▁toprak", "▁dayanak", "▁soru", "▁yanıt", "▁ve", "▁bir"]
tokens = ["<unk>", "<s>", "</s>"] + ["<0x%02X>" % i for i in range(256)] + ["▁"] + \
         [chr(c) for c in range(ord('a'), ord('z') + 1)] + kelimeler + ["<|im_start|>", "<|im_end|>"]
scores = [0.0] * len(tokens)
types = [2, 3, 3] + [6] * 256 + [1] * (len(tokens) - 259)
types[-2] = 4; types[-1] = 4          # kullanıcı tanımlı (özel) belirteçler
V, D, L, FF, H = len(tokens), 64, 2, 128, 4

w = gguf.GGUFWriter(cikti, "llama")
w.add_name("umay-mini-test")
w.add_context_length(2048); w.add_embedding_length(D); w.add_block_count(L)
w.add_feed_forward_length(FF); w.add_head_count(H); w.add_head_count_kv(H)
w.add_layer_norm_rms_eps(1e-5); w.add_rope_dimension_count(D // H); w.add_file_type(0)
w.add_tokenizer_model("llama"); w.add_token_list(tokens); w.add_token_scores(scores); w.add_token_types(types)
w.add_bos_token_id(1); w.add_eos_token_id(2); w.add_unk_token_id(0)
w.add_chat_template("{% for m in messages %}<|im_start|>{{ m['role'] }}\n{{ m['content'] }}<|im_end|>\n{% endfor %}<|im_start|>assistant\n")

def t(ad, *sekil, std=0.05):
    w.add_tensor(ad, (rng.standard_normal(sekil) * std).astype(np.float32))
def bir(ad, n): w.add_tensor(ad, np.ones(n, dtype=np.float32))

t("token_embd.weight", V, D); bir("output_norm.weight", D); t("output.weight", V, D)
for i in range(L):
    bir(f"blk.{i}.attn_norm.weight", D)
    for n in ("q", "k", "v"): t(f"blk.{i}.attn_{n}.weight", D, D)
    t(f"blk.{i}.attn_output.weight", D, D)
    bir(f"blk.{i}.ffn_norm.weight", D)
    t(f"blk.{i}.ffn_gate.weight", FF, D); t(f"blk.{i}.ffn_up.weight", FF, D); t(f"blk.{i}.ffn_down.weight", D, FF)
w.write_header_to_file(); w.write_kv_data_to_file(); w.write_tensors_to_file(); w.close()
print("yazıldı:", cikti)
