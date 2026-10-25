# =====================================================================
# UMAY — tek basina calisan surum
#
# ÖTÜKEN'e bagli degildir: ne cekirdek kaynagi, ne baglayici betigi,
# ne de ÖTÜKEN'e ozgu bir kosullu derleme kalmistir. Sira bir Linux
# programidir.
#
#   make          -> kov/umay          (program)
#   make sina     -> kov/umay_sina     (sinama takimi) ve kosar
#   make kur      -> ~/.local'a kurar
#   make temiz
# =====================================================================
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -Iinclude
LDFLAGS ?=

KAYNAK = core/umay_str.c core/umay_ortak.c core/umay_bilgi.c \
         core/umay_ozan.c core/umay_bengi.c core/umay_onay.c \
         core/umay_kanal.c core/umay_main.c \
         alp/otag.c alp/alp_gozcu.c alp/alp_onarici.c alp/alp_bilge.c \
         alp/alp_yazman.c alp/alp_uzman.c \
         us/us_stub.c us/umay_us.c us/umay_us_ipc.c us/umay_us_yerel.c \
         us/us1.c us/umay_us_ollama.c \
         ses/umay_ses.c ses/umay_piper.c \
         gui/umay_gui_konsol.c \
         port/umay_port_host.c \
         kabuk/umay_cli.c

SINA_KAYNAK = $(filter-out kabuk/umay_cli.c,$(KAYNAK)) sina/sina.c

# BASLIK BAGIMLILIKLARI: basliklar degisince de yeniden derlenmeli.
# Yoksa "make" sessizce eski ikiliyi birakir. Nitekim birakti: indeks
# siniri 1200'den 4096'ya cikarildi, make hicbir sey yapmadi, kitaplik
# yine 1200'de doldu. Yanlis olcum, kaybedilen zaman.
BASLIK = $(wildcard include/*.h)

all: kov/umay

kov/umay: $(KAYNAK) $(BASLIK)
	@mkdir -p kov
	$(CC) $(CFLAGS) -DUMAY_SES_ACIK=1 -o $@ $(KAYNAK) $(LDFLAGS)
	@echo "kov/umay hazir — ./kov/umay ile calistirin."

# "make sina" IKI takimi da kosar: UMAY sinamalari ve US1 motor
# sinamasi. Ikincisi depoda vardi ama hicbir hedefe bagli degildi,
# yani hic kosmuyordu — bagli olmayan sinama, olmayan sinamadir.
sina: kov/umay_sina
	./kov/umay_sina
	@$(MAKE) --no-print-directory sina-us1

kov/umay_sina: $(SINA_KAYNAK) $(BASLIK)
	@mkdir -p kov
	$(CC) $(CFLAGS) -DUMAY_SINAMA -o $@ $(SINA_KAYNAK) $(LDFLAGS)

ONEK ?= $(HOME)/.local
# Kurulumda veri yeri IKILIYE GOMULUR: ozel bir onekle kurulunca
# program verisini bulamiyor ve sessizce stub motora dusuyordu.
kur: $(KAYNAK) $(BASLIK)
	@mkdir -p kov
	$(CC) $(CFLAGS) -DUMAY_SES_ACIK=1 \
	      -DUMAY_KUT_ONEK='"$(ONEK)/share/umay/kut"' \
	      -o kov/umay $(KAYNAK) $(LDFLAGS)
	install -d $(ONEK)/bin $(ONEK)/share/umay
	install -m755 kov/umay $(ONEK)/bin/umay
	cp -r kut $(ONEK)/share/umay/
	@echo "kuruldu: $(ONEK)/bin/umay"
	@echo "veri    : $(ONEK)/share/umay/kut"
	@case ":$$PATH:" in *":$(ONEK)/bin:"*) ;; \
	  *) echo "UYARI: $(ONEK)/bin PATH'te degil." ;; esac

# --- ÖTÜKEN dikisi ---
# UMAY, ÖTÜKEN bittiginde buradan baglanir. Degisen TEK sey port
# katmanidir: ana bilgisayar portu yerine ÖTÜKEN portu derlenir.
# Cekirdekte (core/) tek bir kosullu derleme YOKTUR.
OTUKEN_KAYNAK = $(filter-out port/umay_port_host.c ses/umay_piper.c \
                             us/umay_us_ollama.c \
                             gui/umay_gui_konsol.c kabuk/umay_cli.c,$(KAYNAK)) \
                port/umay_port_otuken.c gui/umay_ulgen.c

# Dikisin SAGLAM oldugunu kanitlar: ÖTÜKEN portu ve cekirdek, ÖTÜKEN
# kipinde derleniyor mu? Baglama yapilmaz (cekirdek ortami gerekir),
# yalniz derleme denetlenir. ÖTÜKEN'e dokunmadan kosulabilir.
otuken-denetle:
	@for d in $(OTUKEN_KAYNAK); do \
	  $(CC) -std=c11 -Wall -Wextra -Iinclude -DUMAY_OTUKEN \
	        -ffreestanding -fno-stack-protector -fsyntax-only $$d \
	    || exit 1; \
	done
	@echo "ÖTÜKEN dikisi saglam: $(words $(OTUKEN_KAYNAK)) dosya derlendi."

# Gercek ÖTÜKEN yapimi: cekirdek agacindaki ulib ve baglayici betigi
# gerekir. OT=... ile ÖTÜKEN kokunu verin.
OT ?= ../ot
otuken:
	@test -d $(OT) || { echo "ÖTÜKEN agaci yok: OT=$(OT)"; exit 1; }
	@mkdir -p kov
	$(CC) -std=c11 -Wall -Wextra -O2 -Iinclude -DUMAY_OTUKEN \
	      -ffreestanding -fno-stack-protector -fno-pie -mcmodel=large \
	      -nostdlib -T umay.ld -o kov/umay.elf $(OTUKEN_KAYNAK)
	@echo "kov/umay.elf hazir — ÖTÜKEN diskine UMAY.ELF olarak koyun."

ses:
	./ses/piper_kur.sh

kaldir:
	rm -f $(ONEK)/bin/umay
	rm -rf $(ONEK)/share/umay
	@echo "kaldirildi: $(ONEK)/bin/umay"

# --- KITAPLIK ---
# Ham arsivi UMAY'in okudugu parcalara cevirir. Parcalar DEPOYA
# GIRMEZ (.gitignore): iceride el kitaplari ve yayinlanmis kitap
# metinleri var, onlarin yeri bir kaynak deposu degil. Kurulan
# makinede bu hedefle yeniden uretilir.
#   make kitaplik ARSIV=/yol/Umay_Kutuphanesi
ARSIV ?=
kitaplik:
	@test -n "$(ARSIV)" || { echo "kullanim: make kitaplik ARSIV=/yol/Umay_Kutuphanesi"; exit 2; }
	@test -d "$(ARSIV)" || { echo "dizin yok: $(ARSIV)"; exit 2; }
	python3 -I arac/kitaplik_kur.py "$(ARSIV)" kut

# --- US1 MOTOR SINAMASI ---
# Motorun KENDISINI sinar: yukleme, bozuk/kisa/yanlis surum dosyalari,
# ileri gecis, sozluk disi im, belirlenimci ve tohumlu secim, ve
# Python referansiyla SAYISAL karsilastirma (ayni nicelenmis
# agirliklar iki tarafta da okunur).
#
# NEDEN AYRI HEDEF: bu sinama depoda us1/sinama/ altinda duruyordu ve
# Makefile'da HIC YOKTU, yani hic kosmuyordu. Icinde gomulu makine
# yollari vardi. Artik kosuyor ve yollar argumanla geliyor.
sina-us1: kov/umay_us1_sina
	@python3 -I arac/us1/referans.py kut/umay.us1 kov >/dev/null 2>&1 || \
	  echo "  (referans uretilemedi — numpy yok mu? sayisal karsilastirma atlanir)"
	./kov/umay_us1_sina kut/umay.us1 kov/referans_imler.txt kov/referans_mantik.txt

kov/umay_us1_sina: sina/us1_sina.c us/us1.c us/us1.h
	@mkdir -p kov
	$(CC) $(CFLAGS) -o $@ sina/us1_sina.c us/us1.c

# --- CEVIRI ---
# Kitapligin Turkce olmayan parcalarini BIR KEZ cevirir. Olculdu:
# 515 parca, 1.16 milyon karakter, neredeyse tamami "tulga" alaninda
# (Musso ve M104 el kitaplari). Ceviri bir KURULUM adimidir, sorgu
# aninda yapilmaz: denetlenebilir olsun ve her soruda baska cikmasin.
#   make ceviri-say              -> ne kadar is var (kuru kosu)
#   make ceviri MODEL=qwen2.5    -> Ollama ile cevirir
MODEL ?=
EN_COK ?=
ceviri-say:
	python3 -I arac/cevir.py kut --kuru
ceviri:
	python3 -I arac/cevir.py kut $(if $(MODEL),--model $(MODEL),) $(if $(EN_COK),--en-cok $(EN_COK),)

# --- EGITIM ---
# Kontrol noktasindan SURDURUR (arac/egit_derlem.py). Kap askiya
# alinirsa ilerleme kaybolmaz; --sifirdan ile bastan baslar.
EGITIM_CIK ?= egitim
ADIM       ?= 400000
egit:
	python3 -I arac/egit_derlem.py kut "$(EGITIM_CIK)" $(ADIM)

temiz:
	rm -f kov/umay kov/umay_sina kov/umay_us1_sina kov/umay.elf \
	      kov/referans_imler.txt kov/referans_mantik.txt

.PHONY: all sina sina-us1 kur kaldir ses otuken otuken-denetle kitaplik ceviri ceviri-say egit temiz
