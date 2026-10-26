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
# -Werror: UYARIYI HATA SAY.
# OLCULDU: bir islev bildirimi yanlislikla bir #ifdef dalinin icine
# dustu. Konak yapisi bunu "implicit declaration of function" UYARISI
# olarak gecti ve ikili uretildi; hata ancak -Werror kullanan ÖTÜKEN
# kapisinda ortaya cikti. Ortuk bildirim C'de islevin int dondurdugunu
# VARSAYAR — imlec donduren bir islevde bu sessiz bozulmadir.
# Iki yapi ayni siddette olmali.
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2 -Iinclude
LDFLAGS ?=

KAYNAK = core/umay_str.c core/umay_ortak.c core/umay_bilgi.c \
         core/umay_ozan.c core/umay_bengi.c core/umay_onay.c \
         core/umay_kanal.c core/umay_ogren.c core/umay_main.c \
         alp/otag.c alp/alp_gozcu.c alp/alp_onarici.c alp/alp_bilge.c \
         alp/alp_yazman.c alp/alp_uzman.c \
         us/us_stub.c us/umay_us.c us/umay_us_ipc.c us/umay_us_yerel.c \
         us/us1.c us/umay_us_ollama.c \
         ses/umay_ses.c ses/umay_piper.c \
         gui/umay_gui_konsol.c \
         port/umay_port_host.c port/umay_host_uyum.c \
         kabuk/umay_cli.c

SINA_KAYNAK = $(filter-out kabuk/umay_cli.c,$(KAYNAK)) sina/sina.c

# Kaynak listeleri (arac/capraz.sh ve CMakeLists.txt ayni listeyi kullansin):
liste-kaynak:
	@echo $(KAYNAK)
liste-sina:
	@echo $(SINA_KAYNAK)
liste-gomulu:
	@echo $(GOMULU_KAYNAK)

# GOMULU KITAPLIK (Android/iOS uygulamalari, masaustu uygulamalari):
# komut satiri ve konsol on yuzu cikar, gomulu on yuz girer.
GOMULU_KAYNAK = $(filter-out kabuk/umay_cli.c gui/umay_gui_konsol.c,$(KAYNAK)) \
                gui/umay_gui_gomulu.c

# BASLIK BAGIMLILIKLARI: basliklar degisince de yeniden derlenmeli.
# Yoksa "make" sessizce eski ikiliyi birakir. Nitekim birakti: indeks
# siniri 1200'den 4096'ya cikarildi, make hicbir sey yapmadi, kitaplik
# yine 1200'de doldu. Yanlis olcum, kaybedilen zaman.
BASLIK = $(wildcard include/*.h)

.DEFAULT_GOAL := all
all: kov/umay

kov/umay: $(KAYNAK) $(BASLIK)
	@mkdir -p kov
	$(CC) $(CFLAGS) -DUMAY_SES_ACIK=1 -o $@ $(KAYNAK) $(LDFLAGS)
	@echo "kov/umay hazir — ./kov/umay ile calistirin."

# TEK TAKIM, TEK SONUC SATIRI.
# Once US1 motor sinamasi ayri bir ikilideydi (us1/sinama/) ve
# Makefile'da HIC YOKTU, yani hic kosmuyordu. Kurtarildiktan sonra iki
# ayri sonuc satiri verir oldu ve kur.sh "tail -1" ile yalniz
# ikincisini okuyup "10 gecti" yaziyordu. Artik hepsi sina.c'de.
#
# Referans degerleri once uretilir: C sabit nokta motoru Python
# referansiyla SAYISAL karsilastirilir. numpy yoksa sinama atlandigini
# SOYLER, sessizce gecmez.
sina: kov/umay_sina
	@python3 -I arac/us1/referans.py kut/umay.us1 kov >/dev/null 2>&1 || true
	./kov/umay_sina

kov/umay_sina: $(SINA_KAYNAK) $(BASLIK)
	@mkdir -p kov
	$(CC) $(CFLAGS) -DUMAY_SINAMA -o $@ $(SINA_KAYNAK) $(LDFLAGS)

# Gomulu kitaplik ve sinamasi: kov/libumay.a, kov/gomulu_sina
kutuphane: kov/libumay.a
kov/libumay.a: $(GOMULU_KAYNAK) $(BASLIK)
	@mkdir -p kov/gomulu
	@for d in $(GOMULU_KAYNAK); do \
	  $(CC) $(CFLAGS) -c $$d -o kov/gomulu/`echo $$d | tr / _`.o || exit 1; \
	done
	@rm -f $@ && ar rcs $@ kov/gomulu/*.o
	@echo "kov/libumay.a hazir."

gomulu-sina: kov/libumay.a sina/gomulu_sina.c
	$(CC) $(CFLAGS) -o kov/gomulu_sina sina/gomulu_sina.c kov/libumay.a $(LDFLAGS)
	./kov/gomulu_sina kut

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
	@# KITAPLIK ZORUNLU DEGILDIR — temiz bir kopyada YOKTUR.
	@# Parcalar .gitignore'dadir; "make kitaplik" ile uretilir.
	@# OLCULMUS HATA: burada duz "cp -r kut" vardi. Temiz klonda
	@#   cp: cannot stat 'kut': No such file or directory
	@#   make: *** [kur] Error 1
	@# ikili KURULDUKTAN SONRA patliyordu: yarim kurulum, sifir disi
	@# cikis kodu, "kuruldu" satirini hic gormeyen kullanici.
	@# Kurulum, elde olmayan veriye BAGLI OLMAMALI.
	@if [ -d kut ]; then \
	  cp -r kut $(ONEK)/share/umay/ && \
	  echo "veri    : $(ONEK)/share/umay/kut"; \
	else \
	  echo "veri    : YOK — kitaplik kurulmadi (zorunlu degil)."; \
	  echo "          uretmek icin: make kitaplik ARSIV=<ham_arsiv>"; \
	  echo "          sonra yeniden: make kur ONEK=$(ONEK)"; \
	fi
	@echo "kuruldu: $(ONEK)/bin/umay"
	@case ":$$PATH:" in *":$(ONEK)/bin:"*) ;; \
	  *) echo "UYARI: $(ONEK)/bin PATH'te degil." ;; esac

# --- ÖTÜKEN dikisi ---
# UMAY, ÖTÜKEN bittiginde buradan baglanir. Degisen TEK sey port
# katmanidir: ana bilgisayar portu yerine ÖTÜKEN portu derlenir.
# Cekirdekte (core/) tek bir kosullu derleme YOKTUR.
OTUKEN_KAYNAK = $(filter-out port/umay_port_host.c port/umay_host_uyum.c ses/umay_piper.c \
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
	@echo "  sozdizimi: $(words $(OTUKEN_KAYNAK)) dosya derlendi."
	@$(MAKE) -s otuken-bag
	@echo "ÖTÜKEN dikisi saglam."

# BAGLAMA DUZEYI KAPISI — otuken-denetle'nin GOREMEDIGI hatalar icin.
#
# NEDEN GEREKLI: otuken-denetle yalniz -fsyntax-only yapar. Bir simge
# hic TANIMLI OLMASA bile sozdizimi denetiminden gecer. Nitekim gecti:
# umay.ld "ENTRY(_start)" diyordu, _start hicbir dosyada yoktu, ve
# "make otuken" BAGLANDI — baglayici yalniz uyardi:
#     cannot find entry symbol _start; defaulting to 0000008000a00000
# O adresteki simge um_bellek_kopya'ydi: ÖTÜKEN programi yukleyip bir
# bellek kopyalama islevine atlardi. Hata calistirana kadar gorunmezdi.
#
# Bu kapi ÖTÜKEN AGACI OLMADAN kosar: kaynaklar nesneye derlenir,
# sonra iki sey kanitlanir:
#   1. _start TANIMLI (giris noktasi gercekten var)
#   2. hicbir simge cozulmemis degil (konak kitapligi sizmamis)
otuken-bag:
	@rm -rf kov/otn && mkdir -p kov/otn
	@for d in $(OTUKEN_KAYNAK); do \
	  o=kov/otn/`echo $$d | tr / _`.o; \
	  $(CC) -std=c11 -Wall -Wextra -Werror -O1 -Iinclude -DUMAY_OTUKEN \
	        -ffreestanding -fno-stack-protector -fno-pie -c $$d -o $$o \
	    || exit 1; \
	done
	@nm kov/otn/*.o | grep -qw 'T _start' \
	  || { echo "HATA: _start TANIMLI DEGIL — umay.ld ENTRY(_start) diyor."; \
	       echo "      ÖTÜKEN bu ELF'i yukleyip yanlis adrese atlar."; exit 1; }
	@nm --defined-only kov/otn/*.o | awk 'NF==3{print $$3}' | sort -u > kov/otn/tanimli
	@nm -u kov/otn/*.o | awk 'NF==2 && $$1=="U"{print $$2}' | sort -u > kov/otn/gereken
	@# BAGLAYICI BETIGININ VERDIGI SIMGELER: umay.ld icindeki "ad = ."
	@# atamalari nesne duzeyinde cozulmemis gorunur, baglamada cozulur.
	@# Betikten OKUNUR, elle yazilmaz: boylece betikte adi degisen ya da
	@# yanlis yazilan bir simge de hata verir.
	@sed -n 's/^[[:space:]]*\([A-Za-z_][A-Za-z0-9_]*\)[[:space:]]*=.*/\1/p' \
	   umay.ld | sort -u > kov/otn/betikten
	@eksik=`comm -23 kov/otn/gereken kov/otn/tanimli | comm -23 - kov/otn/betikten`; \
	 if [ -n "$$eksik" ]; then \
	   echo "HATA: cozulmemis simge (konak kitapligi sizmis olabilir):"; \
	   echo "$$eksik" | sed 's/^/        /'; exit 1; \
	 fi
	@kullanilan=`comm -12 kov/otn/gereken kov/otn/betikten | tr '\n' ' '`; \
	 [ -n "$$kullanilan" ] && echo "  baglayici betiginden: $$kullanilan" || true
	@echo "  baglama: _start tanimli, cozulmemis simge yok ($(words $(OTUKEN_KAYNAK)) nesne)."

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
	rm -f kov/umay kov/umay_sina kov/umay.elf \
	      kov/referans_imler.txt kov/referans_mantik.txt

.PHONY: all sina kur kaldir ses otuken otuken-denetle otuken-bag kitaplik ceviri ceviri-say egit temiz
