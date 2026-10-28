# UMAY — gomulu kitaplik (Android Studio externalNativeBuild, Windows/macOS uygulamalari).
# Komut satiri programi icin Makefile kullanilir. Kaynak listesi Makefile'daki
# GOMULU_KAYNAK ile AYNI olmali: arac/capraz.sh bunu denetler.
cmake_minimum_required(VERSION 3.16)
project(umay C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(UMAY_PAYLASIMLI "libumay'i paylasimli kutuphane (.so/.dll/.dylib) olarak uret" OFF)
option(UMAY_MOTOR "Gomulu llama.cpp motorunu bagla (motor/ altindaki yapi)" OFF)
option(UMAY_JNI "Android JNI koprusunu da derle (jni.h gerekir)" OFF)

set(UMAY_KAYNAK
  core/umay_str.c core/umay_ortak.c core/umay_bilgi.c core/umay_ozan.c
  core/umay_bengi.c core/umay_onay.c core/umay_kanal.c core/umay_ogren.c core/umay_bekci.c core/umay_arastir.c core/umay_egit.c
  core/umay_kabul.c core/umay_pdf.c core/umay_ceviri.c core/umay_ozturk.c core/umay_main.c
  alp/otag.c alp/alp_gozcu.c alp/alp_onarici.c alp/alp_bilge.c alp/alp_arastirmaci.c alp/alp_cevirmen.c
  alp/alp_yazman.c alp/alp_uzman.c
  us/us_stub.c us/umay_us.c us/umay_us_ipc.c us/umay_us_yerel.c
  us/us1.c us/umay_us_ollama.c us/umay_us_llama.c us/umay_motor_yok.c
  ses/umay_ses.c ses/umay_piper.c
  port/umay_port_host.c port/umay_host_uyum.c
  gui/umay_gui_gomulu.c)

if(UMAY_MOTOR)
  list(REMOVE_ITEM UMAY_KAYNAK us/umay_motor_yok.c)   # yerine motor/umay_motor.cpp baglanir
endif()

if(UMAY_JNI)
  list(APPEND UMAY_KAYNAK port/android/umay_jni.c)
endif()

if(UMAY_PAYLASIMLI OR UMAY_JNI)
  add_library(umay SHARED ${UMAY_KAYNAK})
else()
  add_library(umay STATIC ${UMAY_KAYNAK})
endif()
target_include_directories(umay PUBLIC include)
if(WIN32)
  target_link_libraries(umay PRIVATE ws2_32)
endif()

if(UMAY_MOTOR)
  add_subdirectory(motor)
  target_link_libraries(umay PUBLIC umay_motor)
endif()
