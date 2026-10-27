// UMAY gömülü motor — llama.cpp üzerinden umay_motor_api_t uygulaması.
// Dürüstlük: yükleme/üretim hatasında BAŞARISIZ döner, uydurma yanıt vermez.
#include "umay_motor.h"
#include "llama.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

llama_model   *g_model = nullptr;
llama_context *g_ctx = nullptr;
int            g_baglam = 2048;
char           g_ad[160] = "llama.cpp";

void sessiz(ggml_log_level, const char *, void *) {}

int yukle(const char *yol, int baglam, int is) {
    if (g_model) return 0;
    llama_log_set(sessiz, nullptr);
    llama_backend_init();
    llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;          // CPU: her platformda aynı davranış; hızlandırıcılar sonra
    g_model = llama_model_load_from_file(yol, mp);
    if (!g_model) return -1;
    g_baglam = baglam > 0 ? baglam : 2048;
    llama_context_params cp = llama_context_default_params();
    cp.n_ctx = (uint32_t)g_baglam;
    cp.n_batch = (uint32_t)(g_baglam < 512 ? g_baglam : 512);
    unsigned hw = std::thread::hardware_concurrency();
    int nt = is > 0 ? is : (int)(hw > 1 ? (hw > 8 ? 4 : hw / 2 > 0 ? hw / 2 : 1) : 1);
    cp.n_threads = nt;
    cp.n_threads_batch = nt;
    g_ctx = llama_init_from_model(g_model, cp);
    if (!g_ctx) { llama_model_free(g_model); g_model = nullptr; return -2; }
    char desc[96] = "";
    llama_model_desc(g_model, desc, sizeof desc);
    std::snprintf(g_ad, sizeof g_ad, "llama.cpp · %s · %d token bağlam · %d iş parçacığı", desc, g_baglam, nt);
    return 0;
}

// UTF-8 sonu yarım kalmışsa kes
void utf8_kirp(std::string &s) {
    size_t n = s.size();
    size_t i = n;
    while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) i--;
    if (i == 0) return;
    unsigned char lead = (unsigned char)s[i - 1];
    size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (need > 1 && (n - (i - 1)) < need) s.resize(i - 1);
}

int uret(const char *sistem, const char *kullanici, uint32_t en_cok_im, char *cikti, uint32_t n) {
    if (!g_model || !g_ctx || !cikti || n < 2) return -1;
    const llama_vocab *vocab = llama_model_get_vocab(g_model);
    const char *tmpl = llama_model_chat_template(g_model, nullptr);
    if (!tmpl) tmpl = "chatml";
    llama_chat_message msgs[2] = { { "system", sistem ? sistem : "" }, { "user", kullanici ? kullanici : "" } };
    std::vector<char> metin(std::strlen(msgs[0].content) + std::strlen(msgs[1].content) + 1024);
    int32_t uz = llama_chat_apply_template(tmpl, msgs, 2, true, metin.data(), (int32_t)metin.size());
    if (uz < 0) return -3;
    if ((size_t)uz > metin.size()) {
        metin.resize((size_t)uz + 1);
        uz = llama_chat_apply_template(tmpl, msgs, 2, true, metin.data(), (int32_t)metin.size());
        if (uz < 0) return -3;
    }
    // belirteçle
    int32_t nt = -llama_tokenize(vocab, metin.data(), uz, nullptr, 0, true, true);
    if (nt <= 0) return -4;
    std::vector<llama_token> tok((size_t)nt);
    if (llama_tokenize(vocab, metin.data(), uz, tok.data(), nt, true, true) < 0) return -4;
    uint32_t en = en_cok_im ? en_cok_im : 256;
    if ((int64_t)nt + (int64_t)en > (int64_t)g_baglam) return UMAY_MOTOR_TASTI;

    llama_memory_clear(llama_get_memory(g_ctx), true);
    llama_sampler *sm = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sm, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sm, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(sm, llama_sampler_init_temp(0.3f));
    llama_sampler_chain_add(sm, llama_sampler_init_dist(1234));     // sabit tohum: aynı soru aynı yanıt

    std::string yanit;
    int sonuc = 0;
    // istem n_batch'ten uzunsa parça parça işlenir (aksi halde llama.cpp iddia hatasıyla çöker)
    const int32_t nb = (int32_t)(g_baglam < 512 ? g_baglam : 512);
    for (int32_t off = 0; off < (int32_t)tok.size() && sonuc == 0; off += nb) {
        int32_t k = (int32_t)tok.size() - off; if (k > nb) k = nb;
        llama_batch pb = llama_batch_get_one(tok.data() + off, k);
        if (llama_decode(g_ctx, pb) != 0) sonuc = -5;
    }
    for (uint32_t k = 0; k < en && sonuc == 0; k++) {
        llama_token t = llama_sampler_sample(sm, g_ctx, -1);
        if (llama_vocab_is_eog(vocab, t)) break;
        char parca[256];
        int32_t pl = llama_token_to_piece(vocab, t, parca, sizeof parca, 0, false);
        if (pl < 0) { sonuc = -6; break; }
        yanit.append(parca, (size_t)pl);
        llama_batch ob = llama_batch_get_one(&t, 1);
        if (llama_decode(g_ctx, ob) != 0) { sonuc = -5; break; }
    }
    llama_sampler_free(sm);
    if (sonuc != 0) return sonuc;
    utf8_kirp(yanit);
    if (yanit.size() + 1 > n) { yanit.resize(n - 1); utf8_kirp(yanit); }
    std::memcpy(cikti, yanit.data(), yanit.size());
    cikti[yanit.size()] = 0;
    return yanit.empty() ? -7 : 0;
}

void kapat() {
    if (g_ctx) { llama_free(g_ctx); g_ctx = nullptr; }
    if (g_model) { llama_model_free(g_model); g_model = nullptr; }
}

const char *ad() { return g_ad; }

const umay_motor_api_t G = { yukle, uret, kapat, ad };

} // namespace

extern "C" const umay_motor_api_t *umay_motor_api(void) { return &G; }
